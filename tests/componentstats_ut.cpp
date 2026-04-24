#include "gtest/gtest.h"
#include "common/componentstats.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using namespace swss;

// These tests focus on the in-process behavior (no Redis required).
// The writer thread's DB connect attempts will fail and be retried silently;
// that does not affect the getters below, which read from the in-memory map.

TEST(ComponentStats, IncrementBasic)
{
    auto s = ComponentStats::create("UT1");
    s->increment("entity_a", "SET");
    s->increment("entity_a", "SET");
    s->increment("entity_a", "DEL");

    EXPECT_EQ(2U, s->get("entity_a", "SET"));
    EXPECT_EQ(1U, s->get("entity_a", "DEL"));
    EXPECT_EQ(0U, s->get("entity_a", "UNKNOWN"));
    EXPECT_EQ(0U, s->get("unknown_entity", "SET"));
}

TEST(ComponentStats, IncrementByN)
{
    auto s = ComponentStats::create("UT2");
    s->increment("e", "COMPLETE", 5);
    s->increment("e", "COMPLETE", 3);
    EXPECT_EQ(8U, s->get("e", "COMPLETE"));

    // n=0 is a no-op
    s->increment("e", "COMPLETE", 0);
    EXPECT_EQ(8U, s->get("e", "COMPLETE"));
}

TEST(ComponentStats, SetValueOverwrites)
{
    auto s = ComponentStats::create("UT3");
    s->increment("e", "GAUGE", 10);
    s->setValue("e", "GAUGE", 3);
    EXPECT_EQ(3U, s->get("e", "GAUGE"));
}

TEST(ComponentStats, GetAllReturnsAllMetrics)
{
    auto s = ComponentStats::create("UT4");
    s->increment("e", "A", 1);
    s->increment("e", "B", 2);
    s->increment("e", "C", 3);

    auto all = s->getAll("e");
    EXPECT_EQ(3U, all.size());
    EXPECT_EQ(1U, all["A"]);
    EXPECT_EQ(2U, all["B"]);
    EXPECT_EQ(3U, all["C"]);

    EXPECT_TRUE(s->getAll("missing").empty());
}

TEST(ComponentStats, MultipleEntitiesIndependent)
{
    auto s = ComponentStats::create("UT5");
    s->increment("e1", "X", 10);
    s->increment("e2", "X", 20);
    EXPECT_EQ(10U, s->get("e1", "X"));
    EXPECT_EQ(20U, s->get("e2", "X"));
}

TEST(ComponentStats, DisabledIsNoOp)
{
    auto s = ComponentStats::create("UT6");
    s->increment("e", "X", 5);
    s->setEnabled(false);
    s->increment("e", "X", 100);      // ignored
    s->setValue("e", "X", 999);       // ignored
    EXPECT_EQ(5U, s->get("e", "X"));
    s->setEnabled(true);
    s->increment("e", "X", 1);
    EXPECT_EQ(6U, s->get("e", "X"));
}

TEST(ComponentStats, SameComponentReturnsSameInstance)
{
    auto a = ComponentStats::create("UT_SINGLETON");
    auto b = ComponentStats::create("ut_singleton");  // case-insensitive
    EXPECT_EQ(a.get(), b.get());
    a->increment("e", "X", 7);
    EXPECT_EQ(7U, b->get("e", "X"));
}

TEST(ComponentStats, ConcurrentIncrementsAreExact)
{
    auto s = ComponentStats::create("UT_CONC");
    constexpr int kThreads = 8;
    constexpr int kOps = 1000;

    std::vector<std::thread> ts;
    for (int i = 0; i < kThreads; ++i)
    {
        ts.emplace_back([s]{
            for (int j = 0; j < kOps; ++j)
            {
                s->increment("hot", "SET");
            }
        });
    }
    for (auto& t : ts) t.join();

    EXPECT_EQ(static_cast<uint64_t>(kThreads) * kOps, s->get("hot", "SET"));
}

TEST(ComponentStats, StopIsIdempotent)
{
    auto s = ComponentStats::create("UT_STOP");
    s->stop();
    s->stop();  // should not crash or hang
}
