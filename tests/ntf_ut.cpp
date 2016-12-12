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

std::shared_ptr<std::thread> notification_thread;

volatile int messages = 5000;

void ntf_thread()
{
    SWSS_LOG_ENTER();

    swss::DBConnector g_dbNtf(ASIC_DB, "localhost", 6379, 0);
    swss::NotificationConsumer g_redisNotifications(&g_dbNtf, "NOTIFICATIONS");

    swss::Select s;

    s.addSelectable(&g_redisNotifications);

    int collected = 0;

    while (collected < messages)
    {
        swss::Selectable *sel;

        int fd;

        int result = s.select(&sel, &fd);

        if (result == swss::Select::OBJECT)
        {
            swss::KeyOpFieldsValuesTuple kco;

            std::string op;
            std::string data;
            std::vector<swss::FieldValueTuple> values;

            g_redisNotifications.pop(op, data, values);

            SWSS_LOG_INFO("notification: op = %s, data = %s", op.c_str(), data.c_str());

            collected++;
        }
    }
}

TEST(Notifications, test)
{
    SWSS_LOG_ENTER();

    notification_thread = std::make_shared<std::thread>(std::thread(ntf_thread));

    sleep(1); // give time to subscribe to not miss notification

    swss::DBConnector dbNtf(ASIC_DB, "localhost", 6379, 0);
    swss::NotificationProducer notifications(&dbNtf, "NOTIFICATIONS");

    std::vector<swss::FieldValueTuple> entry;

    for(int i = 0; i < messages; i++)
    {
        std::string s = std::to_string(i+1);

        notifications.send("ntf", s, entry);
    }

    notification_thread->join();
}
