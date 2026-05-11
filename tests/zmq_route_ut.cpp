#include <atomic>
#include <chrono>
#include <deque>
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
