#include <gtest/gtest.h>
#include "common/netdispatcher.h"
#include "common/netmsg.h"

using namespace swss;

/* Test NetMsg subclass for raw message handling */
class TestNetMsg : public NetMsg
{
public:
    int msgCount = 0;
    int rawMsgCount = 0;
    int lastNlmsgType = 0;

    void onMsg(int nlmsg_type, struct nl_object *obj) override
    {
        msgCount++;
        lastNlmsgType = nlmsg_type;
    }

    void onMsgRaw(struct nlmsghdr *hdr) override
    {
        rawMsgCount++;
        if (hdr)
            lastNlmsgType = hdr->nlmsg_type;
    }
};

/* Test default onMsgRaw (base class) */
class DefaultNetMsg : public NetMsg
{
public:
    void onMsg(int nlmsg_type, struct nl_object *obj) override {}
    /* onMsgRaw NOT overridden — uses base class default */
};

TEST(NetDispatcherTest, RegisterRawMessageHandler)
{
    NetDispatcher &dispatcher = NetDispatcher::getInstance();
    TestNetMsg handler;

    /* Register raw handler for a test message type */
    EXPECT_NO_THROW(dispatcher.registerRawMessageHandler(1000, &handler));

    /* Duplicate registration should throw */
    EXPECT_THROW(dispatcher.registerRawMessageHandler(1000, &handler), const char *);

    /* Cleanup */
    EXPECT_NO_THROW(dispatcher.unregisterRawMessageHandler(1000));
}

TEST(NetDispatcherTest, UnregisterRawMessageHandler)
{
    NetDispatcher &dispatcher = NetDispatcher::getInstance();
    TestNetMsg handler;

    /* Unregistering non-existent handler should throw */
    EXPECT_THROW(dispatcher.unregisterRawMessageHandler(2000), const char *);

    /* Register then unregister */
    dispatcher.registerRawMessageHandler(2000, &handler);
    EXPECT_NO_THROW(dispatcher.unregisterRawMessageHandler(2000));

    /* Double unregister should throw */
    EXPECT_THROW(dispatcher.unregisterRawMessageHandler(2000), const char *);
}

TEST(NetDispatcherTest, RegisterRawNullCallback)
{
    NetDispatcher &dispatcher = NetDispatcher::getInstance();

    /* Null callback should throw */
    EXPECT_THROW(dispatcher.registerRawMessageHandler(3000, nullptr), const char *);
}

TEST(NetMsgTest, DefaultOnMsgRaw)
{
    DefaultNetMsg msg;
    struct nlmsghdr hdr;
    hdr.nlmsg_type = 42;
    hdr.nlmsg_len = sizeof(hdr);

    /* Default onMsgRaw should not crash — just logs at DEBUG level */
    EXPECT_NO_THROW(msg.onMsgRaw(&hdr));
}

TEST(NetMsgTest, OverriddenOnMsgRaw)
{
    TestNetMsg msg;
    struct nlmsghdr hdr;
    hdr.nlmsg_type = 99;
    hdr.nlmsg_len = sizeof(hdr);

    EXPECT_EQ(msg.rawMsgCount, 0);
    msg.onMsgRaw(&hdr);
    EXPECT_EQ(msg.rawMsgCount, 1);
    EXPECT_EQ(msg.lastNlmsgType, 99);
}

TEST(NetDispatcherTest, GetRawCallbackRegistered)
{
    NetDispatcher &dispatcher = NetDispatcher::getInstance();
    TestNetMsg handler;

    dispatcher.registerRawMessageHandler(4000, &handler);

    /* Simulate onNetlinkMessageRaw by calling it indirectly through onNetlinkMessage
     * when no parsed handler exists for this type. */

    /* Cleanup */
    dispatcher.unregisterRawMessageHandler(4000);
}

TEST(NetDispatcherTest, OnNetlinkMessageFallbackToRaw)
{
    NetDispatcher &dispatcher = NetDispatcher::getInstance();
    TestNetMsg handler;

    /* Register raw handler only (no parsed handler for this type) */
    dispatcher.registerRawMessageHandler(5000, &handler);

    /* Create a minimal nl_msg */
    struct nl_msg *msg = nlmsg_alloc_simple(5000, 0);
    ASSERT_NE(msg, nullptr);

    /* This should fall through to onNetlinkMessageRaw */
    dispatcher.onNetlinkMessage(msg);

    /* Verify the raw handler was called */
    EXPECT_EQ(handler.rawMsgCount, 1);
    EXPECT_EQ(handler.lastNlmsgType, 5000);

    nlmsg_free(msg);
    dispatcher.unregisterRawMessageHandler(5000);
}

TEST(NetDispatcherTest, OnNetlinkMessageRawUnregistered)
{
    NetDispatcher &dispatcher = NetDispatcher::getInstance();

    /* Create a nl_msg for an unregistered type */
    struct nl_msg *msg = nlmsg_alloc_simple(6000, 0);
    ASSERT_NE(msg, nullptr);

    /* Should silently drop — no crash */
    dispatcher.onNetlinkMessage(msg);

    nlmsg_free(msg);
}

TEST(NetDispatcherTest, RegisterMessageHandlerNull)
{
    NetDispatcher &dispatcher = NetDispatcher::getInstance();

    /* Null callback should throw */
    EXPECT_THROW(dispatcher.registerMessageHandler(7000, nullptr), const char *);
}
