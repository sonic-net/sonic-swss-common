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

    swss::DBConnector dbNtf(ASIC_DB, "localhost", 6379, 0);
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

    swss::DBConnector dbNtf(ASIC_DB, "localhost", 6379, 0);
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

    swss::DBConnector dbNtf(ASIC_DB, "localhost", 6379, 0);
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
