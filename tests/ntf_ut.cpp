#include <gtest/gtest.h>
#include "common/ipprefix.h"

#include <iostream>
#include <thread>

#include <unistd.h>

#include "common/notificationconsumer.h"
#include "common/notificationproducer.h"
#include "common/selectableevent.h"
#include "common/select.h"
#include "common/logger.h"
#include "common/table.h"

std::shared_ptr<std::thread> notification_thread;

const int messages = 5000;

void ntf_thread(swss::NotificationConsumer& nc)
{
    SWSS_LOG_ENTER();

    swss::Select s;

    s.addSelectable(&nc);

    int collected = 0;

    while (collected < messages)
    {
        swss::Selectable *sel;

        int result = s.select(&sel);

        if (result == swss::Select::OBJECT)
        {
            swss::KeyOpFieldsValuesTuple kco;

            std::string op;
            std::string data;
            std::vector<swss::FieldValueTuple> values;

            nc.pop(op, data, values);

            SWSS_LOG_INFO("notification: op = %s, data = %s", op.c_str(), data.c_str());
            EXPECT_EQ(op, "ntf");
            int i = stoi(data);
            EXPECT_EQ(i, collected + 1);

            collected++;
        }
    }
    EXPECT_EQ(collected, messages);
}

TEST(Notifications, test)
{
    SWSS_LOG_ENTER();

    swss::DBConnector dbNtf("ASIC_DB", 0, true);
    swss::NotificationConsumer nc(&dbNtf, "NOTIFICATIONS");
    notification_thread = std::make_shared<std::thread>(std::thread(ntf_thread, std::ref(nc)));

    swss::NotificationProducer notifications(&dbNtf, "NOTIFICATIONS");

    std::vector<swss::FieldValueTuple> entry;

    for(int i = 0; i < messages; i++)
    {
        std::string s = std::to_string(i+1);

        auto sentClients = notifications.send("ntf", s, entry);
        EXPECT_EQ(sentClients, 1);
    }

    notification_thread->join();
}

TEST(Notifications, pops)
{
    SWSS_LOG_ENTER();

    swss::DBConnector dbNtf("ASIC_DB", 0, true);
    swss::NotificationConsumer nc(&dbNtf, "NOTIFICATIONS", 100, (size_t)messages);
    swss::NotificationProducer notifications(&dbNtf, "NOTIFICATIONS");

    std::vector<swss::FieldValueTuple> entry;
    for(int i = 0; i < messages; i++)
    {
        auto s = std::to_string(i+1);
        auto sentClients = notifications.send("ntf", s, entry);
        EXPECT_EQ(sentClients, 1);
    }

    // Pop all the notifications
    swss::Select s;
    s.addSelectable(&nc);
    swss::Selectable *sel;

    int result = s.select(&sel);

    EXPECT_EQ(result, swss::Select::OBJECT);

    std::deque<swss::KeyOpFieldsValuesTuple> vkco;
    nc.pops(vkco);
    EXPECT_EQ(vkco.size(), (size_t)messages);

    for (size_t collected = 0; collected < vkco.size(); collected++)
    {
        auto data = kfvKey(vkco[collected]);
        auto op = kfvOp(vkco[collected]);

        EXPECT_EQ(op, "ntf");
        int i = stoi(data);
        EXPECT_EQ((size_t)i, collected + 1);
    }

    // Peek and get nothing more
    int rc = nc.peek();
    EXPECT_EQ(rc, 0);
}

TEST(Notifications, peek)
{
    SWSS_LOG_ENTER();

    swss::DBConnector dbNtf("ASIC_DB", 0, true);
    swss::NotificationConsumer nc(&dbNtf, "NOTIFICATIONS", 100, (size_t)10);
    swss::NotificationProducer notifications(&dbNtf, "NOTIFICATIONS");

    std::vector<swss::FieldValueTuple> entry;
    for(int i = 0; i < messages; i++)
    {
        auto s = std::to_string(i+1);
        auto sentClients = notifications.send("ntf", s, entry);
        EXPECT_EQ(sentClients, 1);
    }

    // Pop all the notifications
    std::deque<swss::KeyOpFieldsValuesTuple> vkco;
    size_t popped = 0;
    size_t npop = 10000;
    int collected = 0;
    while(nc.peek() > 0 && popped < npop)
    {
        nc.pops(vkco);
        popped += vkco.size();

        for (auto& kco : vkco)
        {
            collected++;
            auto data = kfvKey(kco);
            auto op = kfvOp(kco);

            EXPECT_EQ(op, "ntf");
            int i = stoi(data);
            EXPECT_EQ(i, collected);
        }
    }
    EXPECT_EQ(popped, (size_t)messages);

    // Peek and get nothing more
    int rc = nc.peek();
    EXPECT_EQ(rc, 0);
}

TEST(Notifications, pipelineProducer)
{
    SWSS_LOG_ENTER();

    swss::DBConnector dbNtf("ASIC_DB", 0, true);
    swss::RedisPipeline pipeline{&dbNtf};
    swss::NotificationConsumer nc(&dbNtf, "NOTIFICATIONS", 100, (size_t)10);
    const bool buffered = true;
    swss::NotificationProducer notifications(&pipeline, "NOTIFICATIONS", buffered);

    std::vector<swss::FieldValueTuple> entry;
    for(int i = 0; i < messages; i++)
    {
        auto s = std::to_string(i+1);
        auto sentClients = notifications.send("ntf", s, entry);
        // In buffered mode we get -1 in return
        EXPECT_EQ(sentClients, -1);
    }

    // Flush the pipeline
    pipeline.flush();

    // Pop all the notifications
    std::deque<swss::KeyOpFieldsValuesTuple> vkco;
    size_t popped = 0;
    size_t npop = 10000;
    int collected = 0;
    while(nc.peek() > 0 && popped < npop)
    {
        nc.pops(vkco);
        popped += vkco.size();

        for (auto& kco : vkco)
        {
            collected++;
            auto data = kfvKey(kco);
            auto op = kfvOp(kco);

            EXPECT_EQ(op, "ntf");
            int i = stoi(data);
            EXPECT_EQ(i, collected);
        }
    }
    EXPECT_EQ(popped, (size_t)messages);

    // Peek and get nothing more
    int rc = nc.peek();
    EXPECT_EQ(rc, 0);
}

// ---------------------------------------------------------------------------
// End-to-end coverage of NotificationConsumer::setOpAllowList.  Publishes
// a mix of in-allowlist and out-of-allowlist ops over a real redis pub/sub
// channel and verifies that:
//   - only in-allowlist ops are admitted to the queue (observed via pop())
//   - getStats().received counts every message that hit processReply
//   - getStats().dropped_allowlist counts the filtered ones
//
// Uses the same real-redis fixture as the tests above so the wire-format
// path matches production.
// ---------------------------------------------------------------------------
TEST(Notifications, AllowListFilter)
{
    SWSS_LOG_ENTER();

    const std::string kChannel = "ALLOWLIST_TEST_NOTIFICATIONS";
    swss::DBConnector dbNtf("ASIC_DB", 0, true);
    swss::NotificationConsumer nc(&dbNtf, kChannel, 100, (size_t)64);

    // Allow only fdb_event + flush; drop everything else.
    nc.setOpAllowList({"fdb_event", "flush"});

    swss::NotificationProducer producer(&dbNtf, kChannel);

    swss::Select s;
    s.addSelectable(&nc);

    auto send = [&](const std::string& op) {
        std::vector<swss::FieldValueTuple> entry;
        // Retry briefly for the initial subscribe handshake -- send()
        // returns 0 sentClients if nobody is subscribed yet.
        for (int i = 0; i < 50; ++i) {
            auto n = producer.send(op, op + "_data", entry);
            if (n >= 1) return;
            usleep(20 * 1000);
        }
        FAIL() << "no subscriber attached for op=" << op;
    };

    // 3 admitted, 3 dropped at admission.
    send("fdb_event");          // admit
    send("bgp_event");          // drop
    send("flush");              // admit
    send("port_state_change");  // drop
    send("fdb_event");          // admit
    send("ignored");            // drop

    // Drain whatever is available; the publish path is async, so poll
    // with a bounded timeout.
    std::vector<std::string> admitted_ops;
    for (int i = 0; i < 100 && admitted_ops.size() < 3; ++i) {
        swss::Selectable *sel = nullptr;
        int result = s.select(&sel, 100 /* ms */);
        if (result == swss::Select::TIMEOUT) continue;
        std::deque<swss::KeyOpFieldsValuesTuple> vkco;
        nc.pops(vkco);
        for (auto& kco : vkco) admitted_ops.push_back(kfvOp(kco));
    }

    ASSERT_EQ(admitted_ops.size(), 3u);
    EXPECT_EQ(admitted_ops[0], "fdb_event");
    EXPECT_EQ(admitted_ops[1], "flush");
    EXPECT_EQ(admitted_ops[2], "fdb_event");

    auto stats = nc.getStats();
    EXPECT_EQ(stats.received, 6u);
    EXPECT_EQ(stats.dropped_allowlist, 3u);
}

// ---------------------------------------------------------------------------
// Covers the 5-arg constructor branch (NotificationQueuePolicy::LruDedup),
// the LruDedup-side of getLruDedupQueue() (non-null), and setStatsLabel()'s
// label-propagation path that calls into the underlying LruDedup queue.
// Mirrors AllowListFilter's real-redis fixture so the constructor's
// subscribeWithRetry() actually subscribes.
// ---------------------------------------------------------------------------
TEST(Notifications, LruDedupPolicyConstructorAndLabelPropagation)
{
    SWSS_LOG_ENTER();

    const std::string kChannel = "LRUDEDUP_TEST_NOTIFICATIONS";
    swss::DBConnector dbNtf("ASIC_DB", 0, true);
    swss::NotificationConsumer nc(&dbNtf, kChannel, 100, (size_t)64,
                                  swss::NotificationQueuePolicy::LruDedup);

    // 5-arg constructor must wire an LruDedup queue underneath -- visible
    // through the public accessor that NotificationConsumerStatsOrch uses.
    auto *lru = nc.getLruDedupQueue();
    ASSERT_NE(lru, nullptr) << "LruDedup policy must expose getLruDedupQueue()";

    // setStatsLabel propagates the label into the underlying queue.  No
    // public getter for the queue's label, so we exercise the path for
    // coverage and verify the call doesn't crash; the log line carries
    // the renamed identifier on the next periodic emission.
    nc.setStatsLabel("UnitTest:LruDedup");

    // Send a couple of messages so processReply runs and the stats
    // counters advance (covers the m_received accounting in the
    // post-allowlist-empty branch).
    swss::NotificationProducer producer(&dbNtf, kChannel);
    swss::Select s;
    s.addSelectable(&nc);

    auto send = [&](const std::string& op) {
        std::vector<swss::FieldValueTuple> entry;
        for (int i = 0; i < 50; ++i) {
            auto n = producer.send(op, op + "_data", entry);
            if (n >= 1) return;
            usleep(20 * 1000);
        }
        FAIL() << "no subscriber attached for op=" << op;
    };
    send("fdb_event");
    send("fdb_event");    // byte-identical to first -- LruDedup should collapse

    // Drain so processReply is invoked.
    size_t pops = 0;
    for (int i = 0; i < 50 && pops == 0; ++i) {
        swss::Selectable *sel = nullptr;
        if (s.select(&sel, 100 /* ms */) == swss::Select::TIMEOUT) continue;
        std::deque<swss::KeyOpFieldsValuesTuple> vkco;
        nc.pops(vkco);
        pops += vkco.size();
    }
    EXPECT_GE(pops, 1u);

    auto stats = nc.getStats();
    EXPECT_EQ(stats.received, 2u);
    EXPECT_EQ(stats.dropped_allowlist, 0u);   // no allowlist set, no drops
}

// ---------------------------------------------------------------------------
// Covers the FIFO branch of getLruDedupQueue() (returns null because the
// underlying queue is FifoNotificationQueue).  This is the default-policy
// path used by every legacy 4-arg call site.
// ---------------------------------------------------------------------------
TEST(Notifications, FifoConsumerReturnsNullFromGetLruDedupQueue)
{
    SWSS_LOG_ENTER();

    swss::DBConnector dbNtf("ASIC_DB", 0, true);
    swss::NotificationConsumer nc(&dbNtf, "FIFO_TEST_NOTIFICATIONS", 100,
                                  (size_t)64);
    // 4-arg default-Fifo path: getLruDedupQueue() must return null so that
    // NotificationConsumerStatsOrch knows to publish only the admit-side
    // counters and skip the lru_* fields.
    EXPECT_EQ(nc.getLruDedupQueue(), nullptr);

    // setStatsLabel on a Fifo consumer takes the early-return branch
    // (no inner LruDedup to forward to).
    nc.setStatsLabel("UnitTest:Fifo");
}

// ---------------------------------------------------------------------------
// Covers NotificationConsumer::maybeLogStats' counter-gate + time-gate +
// emission paths.  The gate is `(m_received & (kStatsCheckEveryN - 1)) == 0`
// where kStatsCheckEveryN = 1024 in the .cpp, so we need to drive 1024+
// messages through processReply.
//
// Slow-ish (a few hundred ms with the producer batching used here), but the
// alternative is exposing kStatsCheckEveryN as a configurable for tests
// only, which complicates the public API.
// ---------------------------------------------------------------------------
TEST(Notifications, ConsumerMaybeLogStatsCounterGate)
{
    SWSS_LOG_ENTER();

    const std::string kChannel = "STATSLOG_TEST_NOTIFICATIONS";
    swss::DBConnector dbNtf("ASIC_DB", 0, true);
    swss::RedisPipeline pipeline{&dbNtf};
    swss::NotificationConsumer nc(&dbNtf, kChannel, 100, (size_t)4096);
    const bool buffered = true;
    swss::NotificationProducer producer(&pipeline, kChannel, buffered);

    swss::Select s;
    s.addSelectable(&nc);

    // Buffered/pipelined producer: send 1100 ops so we cross the
    // kStatsCheckEveryN = 1024 boundary, ensuring the counter gate
    // releases at least once and the emission code path executes.
    const int kCount = 1100;
    std::vector<swss::FieldValueTuple> entry;
    for (int i = 0; i < kCount; ++i) {
        producer.send("ntf", std::to_string(i), entry);
    }
    pipeline.flush();

    // Drain.  pops() inside the consumer runs processReply for each
    // message, which is the function we care about exercising.
    size_t drained = 0;
    for (int i = 0; i < 200 && drained < (size_t)kCount; ++i) {
        swss::Selectable *sel = nullptr;
        if (s.select(&sel, 100 /* ms */) == swss::Select::TIMEOUT) continue;
        std::deque<swss::KeyOpFieldsValuesTuple> vkco;
        nc.pops(vkco);
        drained += vkco.size();
    }
    EXPECT_EQ(drained, (size_t)kCount);

    auto stats = nc.getStats();
    EXPECT_EQ(stats.received, (uint64_t)kCount);
    EXPECT_EQ(stats.dropped_allowlist, 0u);
}
