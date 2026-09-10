#include <atomic>
#include <chrono>
#include <deque>
#include <iostream>
#include <memory>
#include <thread>
#include <unistd.h>
#include <vector>

#include "gtest/gtest.h"
#include "common/dbconnector.h"
#include "common/select.h"
#include "common/selectableevent.h"
#include "common/zmqclient.h"
#include "common/zmqproducerstatetable.h"
#include "common/zmqrouteconsumerstatetable.h"
#include "common/zmqrouteserver.h"
#include "common/zmqserver.h"

using namespace std;
using namespace swss;

#define TEST_DB "APPL_DB"

namespace {

// Minimal handler used to exercise the new ZmqMessageHandler::notifyPending
// virtual default (no-op) and the override mechanism.
class CountingHandler : public ZmqMessageHandler
{
public:
    void handleReceivedData(const std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> &) override
    {
        ++handleCount;
    }
    void notifyPending() override
    {
        ++notifyCount;
    }
    std::atomic<int> handleCount{0};
    std::atomic<int> notifyCount{0};
};

// Wait until pred() returns true or deadlineMs elapses. Returns true if pred
// became true within the deadline.
template <typename Pred>
bool waitFor(int deadlineMs, Pred pred)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(deadlineMs);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (pred())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return pred();
}

} // namespace

// notifyPending() default in the ZmqMessageHandler base class must be a no-op.
// This guards the contract that subclasses (e.g. ZmqConsumerStateTable) inherit
// without being forced to override.
TEST(ZmqMessageHandler, NotifyPendingDefaultIsNoOp)
{
    struct Bare : public ZmqMessageHandler
    {
        void handleReceivedData(const std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> &) override {}
    };
    Bare h;
    EXPECT_NO_THROW(h.notifyPending());
}

// hasData() returns true unconditionally and hasCachedData()/initializedWithData()
// return false. The consumer's readiness is event-driven via notifyPending; the
// dispatch loop relies on these return values.
TEST(ZmqRouteConsumerStateTable, ConstSelectableProperties)
{
    DBConnector db(TEST_DB, 0, true);
    ZmqRouteServer server("tcp://*:1238", "", true);
    ZmqRouteConsumerStateTable c(&db, "ZMQ_ROUTE_UT", server, 0, /*dbPersistence=*/false);

    EXPECT_TRUE(c.hasData());
    EXPECT_FALSE(c.hasCachedData());
    EXPECT_FALSE(c.initializedWithData());
}

// notifyPending() must signal the SelectableEvent so a Select loop wakes up.
TEST(ZmqRouteConsumerStateTable, NotifyPendingFiresSelectableEvent)
{
    DBConnector db(TEST_DB, 0, true);
    ZmqRouteServer server("tcp://*:1239", "", true);
    ZmqRouteConsumerStateTable c(&db, "ZMQ_ROUTE_UT", server, 0, /*dbPersistence=*/false);

    Select sel;
    sel.addSelectable(&c);

    Selectable *out = nullptr;
    // Should time out before notify.
    int rc = sel.select(&out, 50);
    EXPECT_EQ(rc, Select::TIMEOUT);

    c.notifyPending();

    rc = sel.select(&out, 100);
    EXPECT_EQ(rc, Select::OBJECT);
    EXPECT_EQ(out, &c);
    // Note: Select::select() drains the eventfd internally via readData(); do
    // not call out->readData() again, that would block on an empty eventfd.
}

// End-to-end smoke test: send one ZMQ message via ZmqProducerStateTable and
// verify the ingress callback sees the deserialized tuple. This also exercises
// the path where ZmqRouteServer::mqPollThread fires notifyPending after the
// burst quiesces (BURST_QUIESCE_MS).
TEST(ZmqRouteConsumerStateTable, IngressCallbackReceivesData)
{
    const string tableName = "ZMQ_ROUTE_UT";
    const string pushEndpoint = "tcp://localhost:1240";
    const string pullEndpoint = "tcp://*:1240";

    DBConnector db(TEST_DB, 0, true);
    ZmqRouteServer server(pullEndpoint, "", /*lazyBind=*/true);
    ZmqRouteConsumerStateTable c(&db, tableName, server, 0, /*dbPersistence=*/false);

    std::atomic<int> cbInvocations{0};
    std::atomic<int> tuplesSeen{0};
    std::string firstKey;
    std::mutex captureMutex;
    c.setIngressCallback(
        [&](const std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> &kcos) {
            std::lock_guard<std::mutex> lk(captureMutex);
            cbInvocations++;
            tuplesSeen += static_cast<int>(kcos.size());
            if (firstKey.empty() && !kcos.empty())
                firstKey = kfvKey(*kcos.front());
        });

    server.bind(); // lazy bind after handler registered

    Select sel;
    sel.addSelectable(&c);

    // Producer side.
    ZmqClient client(pushEndpoint, 0);
    ZmqProducerStateTable p(&db, tableName, client, /*dbPersistence=*/false);
    vector<FieldValueTuple> fvs{{"prefix", "10.0.0.0/24"}, {"nh", "1.1.1.1"}};
    p.set("route_one", fvs);

    // Wait for callback to be invoked (mqPollThread → handleReceivedData →
    // m_ingressCallback). Then wait for SelectableEvent to fire (BURST_QUIESCE_MS).
    ASSERT_TRUE(waitFor(2000, [&] { return cbInvocations.load() >= 1; }));
    EXPECT_GE(tuplesSeen.load(), 1);

    Selectable *out = nullptr;
    int rc = sel.select(&out, 200);
    EXPECT_EQ(rc, Select::OBJECT);
    EXPECT_EQ(out, &c);

    {
        std::lock_guard<std::mutex> lk(captureMutex);
        EXPECT_EQ(firstKey, "route_one");
    }
}

// Burst coalescing: send many messages back-to-back; verify all reach the
// callback, and the SelectableEvent fires far fewer times than there are
// messages (ideally once or twice for the whole burst). This is the core
// behavior the new ZmqRouteServer adds over ZmqServer.
TEST(ZmqRouteConsumerStateTable, BurstCoalescingFiresFewerWakeups)
{
    const string tableName = "ZMQ_ROUTE_UT";
    const string pushEndpoint = "tcp://localhost:1241";
    const string pullEndpoint = "tcp://*:1241";
    constexpr int N = 200;

    DBConnector db(TEST_DB, 0, true);
    ZmqRouteServer server(pullEndpoint, "", /*lazyBind=*/true);
    ZmqRouteConsumerStateTable c(&db, tableName, server, 0, /*dbPersistence=*/false);

    std::atomic<int> tuplesSeen{0};
    c.setIngressCallback(
        [&](const std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> &kcos) {
            tuplesSeen += static_cast<int>(kcos.size());
        });

    server.bind();

    Select sel;
    sel.addSelectable(&c);

    ZmqClient client(pushEndpoint, 0);
    ZmqProducerStateTable p(&db, tableName, client, /*dbPersistence=*/false);

    // Tight loop — these should land in one or a few bursts on the server side.
    for (int i = 0; i < N; ++i)
    {
        vector<FieldValueTuple> fvs{{"idx", std::to_string(i)}};
        p.set("burst_key_" + std::to_string(i), fvs);
    }

    ASSERT_TRUE(waitFor(5000, [&] { return tuplesSeen.load() >= N; }));

    // Count how many times Select wakes us. With burst coalescing, this should
    // be much smaller than N. We give the server a generous window for the
    // burst to quiesce and allow up to N/4 wakeups (very loose upper bound
    // tolerant of CI scheduling) — in practice it's usually 1–3.
    int wakeups = 0;
    Selectable *out = nullptr;
    // select() drains the eventfd internally on each OBJECT return; loop
    // continues to advance only when mqPollThread fires another notifyPending.
    while (sel.select(&out, 100) == Select::OBJECT)
    {
        ++wakeups;
    }
    EXPECT_GE(wakeups, 1);
    EXPECT_LT(wakeups, N / 4) << "burst coalescing produced "
                              << wakeups << " wakeups for " << N << " messages";
}

// Without an ingress callback set, handleReceivedData should still be safe:
// no callback fires, no crash, and notifyPending still wakes Select.
TEST(ZmqRouteConsumerStateTable, NoIngressCallbackIsSafe)
{
    const string tableName = "ZMQ_ROUTE_UT";
    const string pushEndpoint = "tcp://localhost:1242";
    const string pullEndpoint = "tcp://*:1242";

    DBConnector db(TEST_DB, 0, true);
    ZmqRouteServer server(pullEndpoint, "", /*lazyBind=*/true);
    ZmqRouteConsumerStateTable c(&db, tableName, server, 0, /*dbPersistence=*/false);
    // intentionally do NOT call setIngressCallback

    server.bind();

    ZmqClient client(pushEndpoint, 0);
    ZmqProducerStateTable p(&db, tableName, client, /*dbPersistence=*/false);
    p.set("orphan_key", vector<FieldValueTuple>{{"f", "v"}});

    // Give mqPollThread time to deliver and the burst to quiesce.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    Select sel;
    sel.addSelectable(&c);
    Selectable *out = nullptr;
    // SelectableEvent should fire from mqPollThread's post-burst notifyPending.
    EXPECT_EQ(sel.select(&out, 500), Select::OBJECT);
}

// Multiple burst transitions: two bursts separated by an idle gap must each
// produce their own wakeup. The server fires notifyPending once per quiesced
// burst and re-arms across the gap, so "send 10 -> quiesce -> send 10 ->
// quiesce" yields two distinct wakeup phases rather than a single coalesced
// one. We drain the first burst's wakeup(s) before the second burst so the two
// phases are provably independent (the SelectableEvent's eventfd would
// otherwise collapse them). Guards the burst-boundary re-arm behavior.
TEST(ZmqRouteConsumerStateTable, MultipleBurstsEachWakeSeparately)
{
    const string tableName = "ZMQ_ROUTE_UT";
    const string pushEndpoint = "tcp://localhost:1243";
    const string pullEndpoint = "tcp://*:1243";
    constexpr int BURST = 10;

    DBConnector db(TEST_DB, 0, true);
    ZmqRouteServer server(pullEndpoint, "", /*lazyBind=*/true);
    ZmqRouteConsumerStateTable c(&db, tableName, server, 0, /*dbPersistence=*/false);

    std::atomic<int> tuplesSeen{0};
    c.setIngressCallback(
        [&](const std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> &kcos) {
            tuplesSeen += static_cast<int>(kcos.size());
        });

    server.bind();

    Select sel;
    sel.addSelectable(&c);

    ZmqClient client(pushEndpoint, 0);
    ZmqProducerStateTable p(&db, tableName, client, /*dbPersistence=*/false);

    // Send BURST messages back-to-back starting at index `base`.
    auto sendBurst = [&](int base) {
        for (int i = 0; i < BURST; ++i)
        {
            vector<FieldValueTuple> fvs{{"idx", std::to_string(base + i)}};
            p.set("burst_key_" + std::to_string(base + i), fvs);
        }
    };

    // Wait until `expectedTuples` have reached the callback, then count every
    // Select wakeup for this burst (a burst may internally quiesce more than
    // once under CI scheduling, so we drain until the eventfd is empty).
    auto waitAndDrainWakeups = [&](int expectedTuples) -> int {
        EXPECT_TRUE(waitFor(5000, [&] { return tuplesSeen.load() >= expectedTuples; }));
        int wakeups = 0;
        Selectable *out = nullptr;
        while (sel.select(&out, 200) == Select::OBJECT)
        {
            EXPECT_EQ(out, &c);
            ++wakeups;
        }
        return wakeups;
    };

    // Burst 1 -> wait for delivery -> drain its wakeup(s).
    sendBurst(0);
    int wakeupsBurst1 = waitAndDrainWakeups(BURST);

    // Idle gap well beyond BURST_QUIESCE_MS so burst 1 has fully settled and its
    // wakeup has been drained before burst 2 starts.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Burst 2 -> wait for delivery -> drain its wakeup(s).
    sendBurst(BURST);
    int wakeupsBurst2 = waitAndDrainWakeups(2 * BURST);

    // Each burst independently woke the consumer: the second burst produced a
    // fresh wakeup only after the first was drained, proving the server re-arms
    // notifyPending per burst (two burst transitions -> two wakeup phases).
    EXPECT_GE(wakeupsBurst1, 1) << "first burst produced no wakeup";
    EXPECT_GE(wakeupsBurst2, 1) << "second burst produced no fresh wakeup after the first drained";
    EXPECT_EQ(tuplesSeen.load(), 2 * BURST);
}

// A handler unregistered (and possibly destroyed) between being marked dirty
// and the deferred quiesce flush must be skipped, not notified through a
// dangling pointer. Regression test for the raw-pointer flush in
// ZmqRouteServer::mqPollThread: the registry validates liveness under the
// same mutex removeHandler() takes.
TEST(ZmqHandlerRegistry, FlushSkipsUnregisteredHandlers)
{
    ZmqHandlerRegistry registry;

    CountingHandler stays;
    CountingHandler leaves;
    registry.registerHandler("APPL_DB", "T_STAYS", &stays);
    registry.registerHandler("APPL_DB", "T_LEAVES", &leaves);

    // Both were dirtied during a burst...
    ZmqHandlerRegistry::DirtyHandlerMap dirty{
        {&stays, std::chrono::steady_clock::now()},
        {&leaves, std::chrono::steady_clock::now()},
    };

    // ...but one consumer is torn down before the quiesce flush fires.
    registry.removeHandler("APPL_DB", "T_LEAVES");

    registry.flushDirtyHandlers(dirty, std::chrono::steady_clock::now());

    EXPECT_EQ(stays.notifyCount.load(), 1);
    EXPECT_EQ(leaves.notifyCount.load(), 0);   // skipped, not notified
    EXPECT_TRUE(dirty.empty());                // flush also clears the set

    // A second flush with an empty map is a no-op.
    registry.flushDirtyHandlers(dirty, std::chrono::steady_clock::now());
    EXPECT_EQ(stays.notifyCount.load(), 1);
}

// The cutoff must leave younger entries in place: only handlers dirty since
// at or before the cutoff are notified and erased. This is what bounds
// notification latency mid-burst without double-notifying fresh entries.
TEST(ZmqHandlerRegistry, FlushHonorsCutoff)
{
    ZmqHandlerRegistry registry;

    CountingHandler oldOne;
    CountingHandler youngOne;
    registry.registerHandler("APPL_DB", "T_OLD", &oldOne);
    registry.registerHandler("APPL_DB", "T_YOUNG", &youngOne);

    const auto now = std::chrono::steady_clock::now();
    ZmqHandlerRegistry::DirtyHandlerMap dirty{
        {&oldOne, now - std::chrono::milliseconds(100)},
        {&youngOne, now},
    };

    // Cutoff of "50ms ago": only the 100ms-old entry is overdue.
    registry.flushDirtyHandlers(dirty, now - std::chrono::milliseconds(50));

    EXPECT_EQ(oldOne.notifyCount.load(), 1);
    EXPECT_EQ(youngOne.notifyCount.load(), 0);
    EXPECT_EQ(dirty.size(), 1u);
    EXPECT_EQ(dirty.count(&youngOne), 1u);
}

// End to end: a stream that never pauses for BURST_QUIESCE_MS and has no
// consumer-side threshold must still wake the Select loop within roughly
// BURST_MAX_HOLDOFF_MS. Before the holdoff, the only wake was the quiesce
// notify, which such a stream never triggers.
//
// The wake latency is what attributes the wake to the holdoff: a quiesce
// wake needs a >BURST_QUIESCE_MS gap in the stream, and at ~300us pacing
// such a gap takes a >16x scheduler stall. sleep_for() only guarantees a
// minimum gap though, so a single scheduler overshoot past the ~5ms
// quiesce timeout fires the quiesce flush (cutoff = now) and wakes select
// early even though the holdoff code is correct. A sub-40ms wake in one
// trial is therefore inconclusive, not a failure: the trial is discarded
// and re-run with fresh state. What separates a stray gap from a real
// regression is consistency -- kMaxAttempts independent early wakes would
// need that many independent >16x stalls, so if every trial wakes early
// the deferral is genuinely not happening (per-message notify) and the
// test fails. A timeout (unbounded deferral, e.g. BURST_MAX_HOLDOFF_MS
// reverted) or a >150ms wake fails the trial outright.
TEST(ZmqRouteConsumerStateTable, ContinuousStreamWakesWithinHoldoff)
{
    const string tableName = "ZMQ_ROUTE_UT_HOLDOFF";
    constexpr int kMaxAttempts = 5;

    DBConnector db(TEST_DB, 0, true);

    for (int attempt = 0; attempt < kMaxAttempts; ++attempt)
    {
        // Fresh harness (and port) per trial so an inconclusive trial's
        // already-signaled eventfd and in-flight messages can't leak into
        // the next one.
        const string port = std::to_string(1245 + attempt);
        const string pushEndpoint = "tcp://localhost:" + port;
        const string pullEndpoint = "tcp://*:" + port;

        ZmqRouteServer server(pullEndpoint, "", /*lazyBind=*/true);
        ZmqRouteConsumerStateTable c(&db, tableName, server, 0, /*dbPersistence=*/false);
        c.setIngressCallback(
            [](const std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> &) {});

        server.bind();

        Select sel;
        sel.addSelectable(&c);

        ZmqClient client(pushEndpoint, 0);
        ZmqProducerStateTable p(&db, tableName, client, /*dbPersistence=*/false);

        // Same-key updates every ~300us for the whole window: no quiesce gap
        // short of a >16x scheduler stall, no growth in distinct keys.
        std::atomic<bool> stop{false};
        std::thread producer([&] {
            while (!stop.load())
            {
                p.set("flap_key", vector<FieldValueTuple>{{"seq", "x"}});
                std::this_thread::sleep_for(std::chrono::microseconds(300));
            }
        });

        // 500ms select timeout: an unbounded deferral would time out here,
        // the 50ms holdoff wakes us an order of magnitude earlier.
        Selectable *out = nullptr;
        const auto selectStart = std::chrono::steady_clock::now();
        int ret = sel.select(&out, 500);
        const auto wakeLatency = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - selectStart);
        stop = true;
        producer.join();

        // Timeout means the deferral never flushed: the exact regression
        // this test exists to catch. Fail regardless of attempt.
        ASSERT_EQ(ret, Select::OBJECT)
            << "select timed out: notification deferral is unbounded";
        ASSERT_EQ(out, &c);

        // Holdoff wake lands at ~BURST_MAX_HOLDOFF_MS (50ms) plus one drain
        // pass; above 150ms the wake wasn't the holdoff.
        ASSERT_LE(wakeLatency.count(), 150);

        if (wakeLatency.count() >= 40)
        {
            SUCCEED();
            return;
        }

        // Inconclusive: a quiesce/gap wake fired first. Discard and retry.
        std::cout << "attempt " << attempt << " inconclusive: woke at "
                  << wakeLatency.count() << "ms (quiesce gap), retrying"
                  << std::endl;
    }

    FAIL() << "every trial woke below 40ms: " << kMaxAttempts
           << " independent >16x scheduler stalls is not plausible -- the "
              "burst deferral is not happening (per-message notify?)";
}

// Regression guard for the destroy-time use-after-free in
// ZmqRouteConsumerStateTable. The registry detach lives in the base
// ~ZmqConsumerStateTable(), but C++ destroys members most-derived-first, so
// without a derived destructor m_ingressCallback is already gone by the time
// the base detaches. In that window ZmqRouteServer::mqPollThread can still find
// the (not-yet-unregistered) handler and dispatch into the overridden
// handleReceivedData(), reading a destroyed std::function and, for a callback
// with heap-allocating captures, dereferencing freed memory.
//
// This drives that path: a producer streams continuously while the consumer is
// destroyed mid-stream, and the ingress callback owns the *only* reference to a
// heap payload that it reads on every invocation. If a dispatch reaches the
// callback after destruction, the read lands on freed memory and AddressSanitizer
// (the sanitized CI leg) reports heap-use-after-free.
//
// The bug window is a narrow interleaving, so this is a best-effort stress
// reproducer rather than a deterministic trigger: it runs many create/stream/
// destroy cycles to raise the odds, and passes cleanly once the most-derived
// ~ZmqRouteConsumerStateTable() detaches before its members are destroyed
// (removeHandler() then blocks any in-flight dispatch and bars new ones while
// m_ingressCallback is still alive). The deterministic guarantee comes from that
// destructor ordering; this test is the empirical backstop.
TEST(ZmqRouteConsumerStateTable, DestroyDuringStreamIsSafe)
{
    const string tableName = "ZMQ_ROUTE_UT_DTOR";
    constexpr int kIterations = 150;

    DBConnector db(TEST_DB, 0, true);
    // Outlives every consumer, so an in-flight dispatch that touches it during
    // teardown is never itself a use-after-free — only the per-iteration heap
    // payload is, which is what we want to detect.
    std::atomic<long> observed{0};

    for (int iter = 0; iter < kIterations; ++iter)
    {
        // Unique port per iteration: avoids rebinding a socket still in
        // TCP teardown from the previous cycle.
        const string port = std::to_string(1260 + iter);
        const string pushEndpoint = "tcp://localhost:" + port;
        const string pullEndpoint = "tcp://*:" + port;

        auto server = std::make_unique<ZmqRouteServer>(pullEndpoint, "", /*lazyBind=*/true);
        auto consumer = std::make_unique<ZmqRouteConsumerStateTable>(
            &db, tableName, *server, 0, /*dbPersistence=*/false);

        // The lambda holds the sole reference to payload (moved in, no outer
        // copy kept), so destroying m_ingressCallback frees the vector. Reading
        // it on every dispatch turns a post-destruction invocation into an
        // AddressSanitizer-visible read of freed memory.
        {
            auto payload = std::make_shared<std::vector<int>>(64, iter);
            consumer->setIngressCallback(
                [payload = std::move(payload), &observed](
                    const std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> &kcos) {
                    long acc = 0;
                    for (int v : *payload)
                        acc += v;
                    observed += acc + static_cast<long>(kcos.size());
                });
        }

        server->bind();

        std::atomic<bool> stop{false};
        std::thread producer([&] {
            ZmqClient client(pushEndpoint, 0);
            ZmqProducerStateTable p(&db, tableName, client, /*dbPersistence=*/false);
            while (!stop.load(std::memory_order_relaxed))
            {
                p.set("flap_key", vector<FieldValueTuple>{{"seq", "x"}});
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });

        // Let the stream ramp so the poll thread is actively dispatching, then
        // destroy the consumer out from under it — the interleaving under test.
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        consumer.reset();

        stop.store(true, std::memory_order_relaxed);
        producer.join();
        server.reset();
    }

    // No assertion on `observed`: the callback may or may not have fired on any
    // given cycle depending on timing. The pass condition is simply reaching
    // here without a sanitizer abort or crash. Touch it so it isn't optimized
    // away.
    SUCCEED() << "completed " << kIterations
              << " destroy-during-stream cycles (observed=" << observed.load() << ")";
}
