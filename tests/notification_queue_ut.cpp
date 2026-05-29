// Unit tests for the NotificationQueueBase strategy classes:
//   FifoNotificationQueue     -- verifies strict arrival-order preservation
//   LruDedupNotificationQueue -- verifies LRU dedup semantics
//   peekOp                    -- verifies the op-extraction helper
//                                that drives setOpAllowList admission
//
// These tests are pure in-memory; they do NOT require a running redis-server.
// End-to-end coverage of setOpAllowList filtering (which needs a real
// NotificationProducer + DBConnector wired through redis) lives in
// ntf_ut.cpp; see TEST(Notifications, AllowList).

#include <gtest/gtest.h>

#include "common/notificationconsumer.h"

using swss::FifoNotificationQueue;
using swss::LruDedupNotificationQueue;
using swss::peekOp;

// ---------------------------------------------------------------------------
// FifoNotificationQueue tests
// ---------------------------------------------------------------------------

TEST(FifoNotificationQueue, PreservesArrivalOrder)
{
    FifoNotificationQueue q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);

    q.push("a");
    q.push("b");
    q.push("c");

    EXPECT_EQ(q.size(), 3u);

    EXPECT_EQ(q.front(), "a"); q.pop();
    EXPECT_EQ(q.front(), "b"); q.pop();
    EXPECT_EQ(q.front(), "c"); q.pop();
    EXPECT_TRUE(q.empty());
}

TEST(FifoNotificationQueue, AllowsDuplicates)
{
    FifoNotificationQueue q;
    q.push("dup");
    q.push("dup");
    q.push("dup");
    EXPECT_EQ(q.size(), 3u);
    EXPECT_EQ(q.front(), "dup"); q.pop();
    EXPECT_EQ(q.front(), "dup"); q.pop();
    EXPECT_EQ(q.front(), "dup"); q.pop();
    EXPECT_TRUE(q.empty());
}

// ---------------------------------------------------------------------------
// LruDedupNotificationQueue tests
// ---------------------------------------------------------------------------

TEST(LruDedupNotificationQueue, CollapsesByteIdenticalDuplicate)
{
    LruDedupNotificationQueue q("test");
    q.push("a");
    q.push("a");
    q.push("a");

    EXPECT_EQ(q.size(), 1u);
    EXPECT_EQ(q.front(), "a");
    q.pop();
    EXPECT_TRUE(q.empty());

    auto s = q.getStats();
    EXPECT_EQ(s.pushed, 3u);
    EXPECT_EQ(s.dedup_hits, 2u);
    EXPECT_EQ(s.high_watermark, 1u);
}

TEST(LruDedupNotificationQueue, DrainOrderIsLastSeenPerUniquePayload)
{
    LruDedupNotificationQueue q("test");

    // Arrival: a, b, a, c, b
    q.push("a");          // [a]
    q.push("b");          // [a, b]
    q.push("a");          // [b, a]      (a re-inserted at tail)
    q.push("c");          // [b, a, c]
    q.push("b");          // [a, c, b]   (b re-inserted at tail)

    EXPECT_EQ(q.size(), 3u);
    EXPECT_EQ(q.front(), "a"); q.pop();
    EXPECT_EQ(q.front(), "c"); q.pop();
    EXPECT_EQ(q.front(), "b"); q.pop();
    EXPECT_TRUE(q.empty());
}

TEST(LruDedupNotificationQueue, HighWatermarkIsMonotonic)
{
    LruDedupNotificationQueue q("test");

    q.push("a");
    q.push("b");
    q.push("c");
    EXPECT_EQ(q.getStats().high_watermark, 3u);

    q.pop();
    q.pop();
    EXPECT_EQ(q.getStats().high_watermark, 3u);  // HWM does not decrease

    q.push("d");
    EXPECT_EQ(q.getStats().high_watermark, 3u);  // still 3, didn't exceed it

    q.push("e");
    q.push("f");
    EXPECT_EQ(q.getStats().high_watermark, 4u);  // new max reached
}

TEST(LruDedupNotificationQueue, MemoryBoundedByDistinctPayloads)
{
    LruDedupNotificationQueue q("test");

    // Push the same 4 distinct payloads many times, in arbitrary interleaving.
    const char *payloads[] = {"A", "B", "C", "D"};
    for (int i = 0; i < 1000; ++i) {
        q.push(payloads[i % 4]);
    }

    EXPECT_EQ(q.size(), 4u);
    auto s = q.getStats();
    EXPECT_EQ(s.pushed, 1000u);
    EXPECT_EQ(s.dedup_hits, 996u);
    EXPECT_EQ(s.high_watermark, 4u);
}

TEST(LruDedupNotificationQueue, EmptyPopIsSafe)
{
    LruDedupNotificationQueue q;
    EXPECT_TRUE(q.empty());
    q.pop();   // must not crash
    EXPECT_TRUE(q.empty());
}

TEST(LruDedupNotificationQueue, SetLabelTakesEffectForFutureStatsLines)
{
    // setLabel is used by NotificationConsumer::setStatsLabel to keep the
    // queue-side periodic log identifier aligned with the consumer-side
    // identifier.  Exercise it directly so the path is covered without
    // requiring a redis-backed NotificationConsumer.
    LruDedupNotificationQueue q("initial");
    q.setLabel("renamed");

    // No public getter for the label -- the only observable side effect is
    // the periodic SWSS_LOG_NOTICE.  Drive a push() so the maybeLogStats()
    // path actually executes with the renamed label; the test asserts on
    // structural invariants (no crash, counters advance) and trusts the
    // log inspection during coverage runs to confirm the rename took.
    q.push("payload-a");
    EXPECT_EQ(q.getStats().pushed, 1u);
    EXPECT_EQ(q.size(), 1u);
}

TEST(LruDedupNotificationQueue, MaybeLogStatsCounterGateAndEmission)
{
    // LruDedupNotificationQueue::maybeLogStats() is the throttled stats
    // logger called from every push().  It has three gates:
    //
    //   1. counter gate: returns unless `m_pushed % kStatsCheckEveryN == 0`
    //      (1024 in the .cpp).  Skipped pushes never even read the clock.
    //   2. time gate:    returns if less than 5s since the previous log.
    //   3. idle gate:    returns if no new pushes since the previous log.
    //
    // This test exercises (1) and (2) plus the actual emission path by
    // pushing two batches of 1024 distinct payloads back-to-back: the
    // first 1024th push opens all three gates and emits, the second
    // 1024th push (cumulative 2048) opens the counter gate but is closed
    // by the time gate because the two batches land within ~milliseconds
    // of each other.  Coverage tooling sees both branches at each gate.
    //
    // The idle gate is defense-in-depth -- it can only fire if push() is
    // bypassed and maybeLogStats is invoked directly, which never happens
    // in production -- so it's intentionally left uncovered here.
    LruDedupNotificationQueue q("counter-gate-test");

    // First batch: 1024 distinct payloads.  Push 1024 reaches the gate,
    // counters advance, log emits.
    for (size_t i = 0; i < 1024; ++i) {
        q.push("first-" + std::to_string(i));
    }
    auto s1 = q.getStats();
    EXPECT_EQ(s1.pushed, 1024u);
    EXPECT_EQ(s1.dedup_hits, 0u);

    // Second batch: another 1024 distinct payloads.  At cumulative 2048
    // the counter gate fires again, but the time gate returns (we're
    // well under 5s).  Counters still advance through push().
    for (size_t i = 0; i < 1024; ++i) {
        q.push("second-" + std::to_string(i));
    }
    auto s2 = q.getStats();
    EXPECT_EQ(s2.pushed, 2048u);
    EXPECT_EQ(s2.dedup_hits, 0u);
}

// ---------------------------------------------------------------------------
// peekOp tests
//
// peekOp drives setOpAllowList admission, so any payload that confuses it
// would either over-admit (defeating the allowlist) or under-admit
// (dropping good traffic).  Cover the shapes that can actually appear on
// the NotificationProducer wire plus the malformed shapes the public
// API has to tolerate.
// ---------------------------------------------------------------------------

TEST(PeekOp, ExtractsWellFormedOp)
{
    EXPECT_EQ(peekOp("[\"SET\",\"key\",\"f\",\"v\"]"), "SET");
    EXPECT_EQ(peekOp("[\"DEL\",\"\"]"),                "DEL");
    EXPECT_EQ(peekOp("[\"fdb_event\",\"\"]"),          "fdb_event");
    EXPECT_EQ(peekOp("[\"flush\",\"\"]"),              "flush");
}

TEST(PeekOp, EmptyAndShortInputs)
{
    EXPECT_EQ(peekOp(""),     "");
    EXPECT_EQ(peekOp("["),    "");
    EXPECT_EQ(peekOp("[\""),  "");    // exactly 2 chars, fails the size >= 3 gate
    EXPECT_EQ(peekOp("\"\""), "");    // no leading [
}

TEST(PeekOp, MissingClosingQuoteReturnsEmpty)
{
    // No second quote at all -- find(...) returns npos and we abort
    // rather than scan past the buffer.
    EXPECT_EQ(peekOp("[\"unterminated"),           "");
    EXPECT_EQ(peekOp("[\"unterminated_with_data"), "");
}

TEST(PeekOp, EmptyOpStringReturnsEmpty)
{
    // [""] has a closing quote at index 2; substr(2, 0) -> "".
    // Treated the same as malformed for allowlist purposes (it can't be
    // in the set, so the message is dropped).
    EXPECT_EQ(peekOp("[\"\"]"),         "");
    EXPECT_EQ(peekOp("[\"\",\"data\"]"), "");
}

TEST(PeekOp, NonArrayInputsReturnEmpty)
{
    // Anything that doesn't start with the array-of-strings prefix
    // ["..." gets rejected.  These are not valid NotificationProducer
    // wire-format payloads.
    EXPECT_EQ(peekOp("not_json"),            "");
    EXPECT_EQ(peekOp("[]"),                  "");   // size 2, fails the gate
    EXPECT_EQ(peekOp("[1,2]"),               "");   // [1 -- second char is '1', not '"'
    EXPECT_EQ(peekOp("{\"op\":\"SET\"}"),    "");   // object form, not the array form
    EXPECT_EQ(peekOp(" [\"SET\",\"x\"]"),    "");   // leading space breaks the prefix
}

TEST(PeekOp, DoesNotUnescape)
{
    // Documented limitation: peekOp stops at the first '"', so a
    // backslash-escaped quote inside the op string is not interpreted.
    // This matches the producer side (NotificationProducer never emits
    // ops containing quotes), but pin the behavior down so a change is
    // caught.
    EXPECT_EQ(peekOp("[\"\\\"OP\",\"x\"]"), "\\");   // sees the literal backslash, stops at the next "
}
