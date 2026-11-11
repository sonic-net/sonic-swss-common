#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include "common/dbconnector.h"
#include "common/notificationconsumer.h"
#include "common/select.h"

using namespace swss;

TEST(NotificationConsumerInProcess, EnqueueAllowListAndPop)
{
    NotificationConsumer consumer("NOTIFICATIONS",
                                  NotificationQueuePolicy::Fifo,
                                  100,
                                  64);
    ASSERT_TRUE(consumer.isInProcessDelivery());

    consumer.setOpAllowList({"fdb_event"});

    consumer.enqueue("fdb_event", "payload", {});
    consumer.enqueue("other_op", "drop", {});

    EXPECT_TRUE(consumer.hasData());

    std::deque<KeyOpFieldsValuesTuple> entries;
    consumer.pops(entries);

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(kfvOp(entries[0]), "fdb_event");
    EXPECT_EQ(kfvKey(entries[0]), "payload");

    auto stats = consumer.getStats();
    EXPECT_EQ(stats.received, 2u);
    EXPECT_EQ(stats.dropped_allowlist, 1u);
}

TEST(NotificationConsumerInProcess, SelectWakesOnEnqueue)
{
    NotificationConsumer consumer("NOTIFICATIONS",
                                  NotificationQueuePolicy::Fifo,
                                  100,
                                  64);
    consumer.setOpAllowList({"ping"});

    Select s;
    s.addSelectable(&consumer);

    // Same thread: callback threads use enqueue(); here we only verify eventfd wake.
    consumer.enqueue("ping", "data", {});

    Selectable *sel = nullptr;
    const int rc = s.select(&sel, 1000);
    ASSERT_EQ(rc, Select::OBJECT);
    ASSERT_EQ(sel, &consumer);

    std::string op;
    std::string data;
    std::vector<FieldValueTuple> values;
    consumer.pop(op, data, values);
    EXPECT_EQ(op, "ping");
    EXPECT_EQ(data, "data");
}

TEST(NotificationConsumerInProcess, AllowListDropDoesNotWakeSelect)
{
    NotificationConsumer consumer("NOTIFICATIONS",
                                  NotificationQueuePolicy::Fifo,
                                  100,
                                  64);
    consumer.setOpAllowList({"wanted"});

    Select s;
    s.addSelectable(&consumer);

    // A message dropped by the allowlist leaves the queue unchanged, so the
    // main loop must not be woken.
    consumer.enqueue("unwanted", "data", {});

    Selectable *sel = nullptr;
    EXPECT_EQ(s.select(&sel, 100), Select::TIMEOUT);
    EXPECT_FALSE(consumer.hasData());

    auto stats = consumer.getStats();
    EXPECT_EQ(stats.received, 1u);
    EXPECT_EQ(stats.dropped_allowlist, 1u);

    // An allowed op still wakes the loop afterwards.
    consumer.enqueue("wanted", "data", {});
    EXPECT_EQ(s.select(&sel, 1000), Select::OBJECT);
    EXPECT_EQ(sel, &consumer);
}

TEST(NotificationConsumerInProcess, NotifyPendingWhenQueued)
{
    NotificationConsumer consumer("NOTIFICATIONS",
                                  NotificationQueuePolicy::Fifo,
                                  100,
                                  64);
    consumer.setOpAllowList({"x"});

    consumer.enqueue("x", "1", {});

    Select s;
    s.addSelectable(&consumer);
    consumer.readData();

    consumer.notifyPending();

    Selectable *sel = nullptr;
    const int rc = s.select(&sel, 1000);
    EXPECT_EQ(rc, Select::OBJECT);
    EXPECT_EQ(sel, &consumer);
}

TEST(NotificationConsumerInProcess, LruDedupViaEnqueue)
{
    NotificationConsumer consumer("NOTIFICATIONS",
                                  NotificationQueuePolicy::LruDedup,
                                  100,
                                  64);
    consumer.setOpAllowList({"fdb_event"});

    consumer.enqueue("fdb_event", "same", {});
    consumer.enqueue("fdb_event", "same", {});

    std::deque<KeyOpFieldsValuesTuple> entries;
    consumer.pops(entries);
    EXPECT_EQ(entries.size(), 1u);

    auto *lru = consumer.getLruDedupQueue();
    ASSERT_NE(lru, nullptr);
    auto qs = lru->getStats();
    EXPECT_EQ(qs.pushed, 2u);
    EXPECT_EQ(qs.dedup_hits, 1u);
}

namespace
{

// Every wait in the tests below is bounded by this deadline, so a lost wakeup
// fails the test instead of hanging it.
constexpr std::chrono::seconds kTestDeadline{30};

// Parse a "<thread>:<seq>" payload.
bool parsePayload(const std::string &payload, int &tid, int &seq)
{
    return std::sscanf(payload.c_str(), "%d:%d", &tid, &seq) == 2;
}

} // namespace

// N sender threads call enqueue() while the main thread loops on Select + pops().
// Every message must arrive exactly once, and each thread's messages must
// arrive in the order that thread sent them.
TEST(NotificationConsumerInProcess, ConcurrentSendersFifoOrderAndCount)
{
    constexpr int kThreads = 4;
    constexpr int kPerThread = 2000;
    constexpr int kTotal = kThreads * kPerThread;

    // Small batch so the main thread needs many pops() rounds.
    NotificationConsumer consumer("NOTIFICATIONS",
                                  NotificationQueuePolicy::Fifo,
                                  100,
                                  16);
    consumer.setOpAllowList({"op"});

    Select s;
    s.addSelectable(&consumer);

    std::atomic<bool> go{false};
    std::vector<std::thread> senders;
    for (int t = 0; t < kThreads; ++t)
    {
        senders.emplace_back([&consumer, &go, t]() {
            while (!go.load())
            {
                std::this_thread::yield();
            }
            for (int i = 0; i < kPerThread; ++i)
            {
                consumer.enqueue("op", std::to_string(t) + ":" + std::to_string(i), {});
            }
        });
    }

    std::vector<int> nextSeq(kThreads, 0);
    int received = 0;
    bool ordered = true;
    bool malformed = false;

    go.store(true);

    const auto deadline = std::chrono::steady_clock::now() + kTestDeadline;
    while (received < kTotal && std::chrono::steady_clock::now() < deadline)
    {
        Selectable *sel = nullptr;
        const int rc = s.select(&sel, 100);
        if (rc != Select::OBJECT)
        {
            continue;
        }
        if (sel != &consumer)
        {
            // Do not ASSERT here: the sender threads must be joined first.
            ADD_FAILURE() << "select returned an unexpected selectable";
            break;
        }

        std::deque<KeyOpFieldsValuesTuple> entries;
        consumer.pops(entries);
        for (auto &e : entries)
        {
            int tid = -1;
            int seq = -1;
            if (!parsePayload(kfvKey(e), tid, seq) || tid < 0 || tid >= kThreads)
            {
                malformed = true;
                continue;
            }
            if (seq != nextSeq[tid])
            {
                ordered = false;
            }
            nextSeq[tid] = seq + 1;
            ++received;
        }
    }

    for (auto &th : senders)
    {
        th.join();
    }

    EXPECT_FALSE(malformed);
    EXPECT_EQ(received, kTotal);
    EXPECT_TRUE(ordered) << "a thread's messages arrived out of order";
    for (int t = 0; t < kThreads; ++t)
    {
        EXPECT_EQ(nextSeq[t], kPerThread) << "thread " << t;
    }
    EXPECT_FALSE(consumer.hasData());
    EXPECT_EQ(consumer.getStats().received, static_cast<uint64_t>(kTotal));
}

// A backlog larger than POP_BATCH_SIZE must drain completely through Select,
// one batch per select()/pops() round, in order.
TEST(NotificationConsumerInProcess, BacklogLargerThanBatchDrainsThroughSelect)
{
    constexpr size_t kBatch = 8;
    constexpr int kTotal = 50;   // 6 full batches plus a leftover of 2

    NotificationConsumer consumer("NOTIFICATIONS",
                                  NotificationQueuePolicy::Fifo,
                                  100,
                                  kBatch);
    consumer.setOpAllowList({"op"});

    Select s;
    s.addSelectable(&consumer);

    for (int i = 0; i < kTotal; ++i)
    {
        consumer.enqueue("op", std::to_string(i), {});
    }

    int received = 0;
    int rounds = 0;
    bool ordered = true;

    const auto deadline = std::chrono::steady_clock::now() + kTestDeadline;
    while (received < kTotal && std::chrono::steady_clock::now() < deadline)
    {
        Selectable *sel = nullptr;
        const int rc = s.select(&sel, 1000);
        ASSERT_EQ(rc, Select::OBJECT) << "backlog stopped draining after " << received << " messages";
        ASSERT_EQ(sel, &consumer);

        std::deque<KeyOpFieldsValuesTuple> entries;
        consumer.pops(entries);
        EXPECT_LE(entries.size(), kBatch);
        ++rounds;

        for (auto &e : entries)
        {
            if (kfvKey(e) != std::to_string(received))
            {
                ordered = false;
            }
            ++received;
        }
    }

    EXPECT_EQ(received, kTotal);
    EXPECT_TRUE(ordered);
    // 6 full batches of 8 plus a final batch of 2.
    EXPECT_EQ(rounds, 7);
    EXPECT_FALSE(consumer.hasData());
}

// enqueue() on a Redis-subscribed consumer is rejected: nothing is queued and it
// does not throw (it can run on a SAI callback thread).
TEST(NotificationConsumerInProcess, EnqueueOnRedisConsumerIsRejected)
{
    DBConnector db("TEST_DB", 0, true);
    NotificationConsumer redisConsumer(&db, "NC_SEND_REJECT_TEST");
    ASSERT_FALSE(redisConsumer.isInProcessDelivery());

    EXPECT_NO_THROW(redisConsumer.enqueue("op", "data", {}));

    EXPECT_FALSE(redisConsumer.hasData());
    EXPECT_EQ(redisConsumer.getStats().received, 0u);
}

// The setters take the lock the admission path reads under, so they can run
// while another thread is in enqueue().  Every enqueue is still counted.
TEST(NotificationConsumerInProcess, SettersAreSafeWhileEnqueuing)
{
    constexpr int kSends = 5000;

    NotificationConsumer consumer("NOTIFICATIONS",
                                  NotificationQueuePolicy::Fifo,
                                  100,
                                  64);

    std::atomic<bool> done{false};
    std::thread sender([&consumer, &done]() {
        for (int i = 0; i < kSends; ++i)
        {
            consumer.enqueue("op", std::to_string(i), {});
        }
        done.store(true);
    });

    const auto deadline = std::chrono::steady_clock::now() + kTestDeadline;
    int i = 0;
    while (!done.load() && std::chrono::steady_clock::now() < deadline)
    {
        consumer.setOpAllowList({"op", "other" + std::to_string(i % 4)});
        consumer.setStatsLabel("Test:label" + std::to_string(i % 4));
        ++i;
        std::this_thread::yield();
    }

    sender.join();

    EXPECT_TRUE(done.load());
    EXPECT_EQ(consumer.getStats().received, static_cast<uint64_t>(kSends));
}

// ---------------------------------------------------------------------------
// Manual micro-benchmark for the cost of enqueue() under a notification storm.
// Disabled by default so CI stays fast.  Run ./tests/tests with
// --gtest_also_run_disabled_tests and
// --gtest_filter='*DISABLED_EnqueueStormCost*'.
// Prefix the command with `strace -f -c -e trace=write` to count eventfd
// writes.
// ---------------------------------------------------------------------------
namespace
{
    constexpr int kStormSends = 200000;

    double usPerCall(std::chrono::steady_clock::time_point t0,
                     std::chrono::steady_clock::time_point t1,
                     int n)
    {
        return std::chrono::duration<double, std::micro>(t1 - t0).count() / n;
    }

    // Enqueue n messages; payload(i) picks the payload so the caller controls
    // whether messages are distinct or identical.
    template <typename PayloadFn>
    double runStorm(NotificationConsumer &consumer, const std::string &op, int n, PayloadFn payload)
    {
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < n; ++i)
        {
            consumer.enqueue(op, payload(i), {});
        }
        const auto t1 = std::chrono::steady_clock::now();
        return usPerCall(t0, t1, n);
    }
}

TEST(NotificationConsumerInProcess, DISABLED_EnqueueStormCost)
{
    {
        NotificationConsumer c("NOTIFICATIONS", NotificationQueuePolicy::Fifo, 100, 64);
        c.setOpAllowList({"op"});
        const double us = runStorm(c, "op", kStormSends, [](int i) { return std::to_string(i); });
        std::printf("[bench] Fifo, distinct payload      : %.3f us/enqueue\n", us);
    }
    {
        NotificationConsumer c("NOTIFICATIONS", NotificationQueuePolicy::LruDedup, 100, 64);
        c.setOpAllowList({"op"});
        const double us = runStorm(c, "op", kStormSends, [](int i) { return std::to_string(i); });
        std::printf("[bench] LruDedup, distinct payload  : %.3f us/enqueue\n", us);
    }
    {
        NotificationConsumer c("NOTIFICATIONS", NotificationQueuePolicy::LruDedup, 100, 64);
        c.setOpAllowList({"op"});
        const double us = runStorm(c, "op", kStormSends, [](int) { return std::string("same"); });
        auto qs = c.getLruDedupQueue()->getStats();
        std::printf("[bench] LruDedup, identical payload : %.3f us/enqueue (dedup_hits=%llu)\n",
                    us, static_cast<unsigned long long>(qs.dedup_hits));
    }
    {
        NotificationConsumer c("NOTIFICATIONS", NotificationQueuePolicy::Fifo, 100, 64);
        c.setOpAllowList({"op"});
        const double us = runStorm(c, "other", kStormSends, [](int) { return std::string("x"); });
        std::printf("[bench] allowlist drop              : %.3f us/enqueue\n", us);
    }
    {
        // Baseline: one eventfd write per call, nothing else.
        SelectableEvent ev;
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < kStormSends; ++i)
        {
            ev.notify();
        }
        const auto t1 = std::chrono::steady_clock::now();
        std::printf("[bench] raw eventfd notify()        : %.3f us/call\n",
                    usPerCall(t0, t1, kStormSends));
    }
}
