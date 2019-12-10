/*
 * Copyright 2019 Broadcom.  The term Broadcom refers to Broadcom Inc. and/or
 * its subsidiaries.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SWSS_ERRORLISTENER_H
#define SWSS_ERRORLISTENER_H

#include "notificationconsumer.h"
#include "selectable.h"
#include "table.h"
#include "dbconnector.h"

// Error notifications of interest to the error listener
typedef enum _error_notify_flags_t
{
    ERR_NOTIFY_FAIL = 1,
    ERR_NOTIFY_POSITIVE_ACK
} error_notify_flags_t;

namespace swss {

    class ErrorListener : public Selectable
    {
        public:
            ErrorListener(std::string appTableName, int errFlags);
            std::string getErrorChannelName(std::string& appTableName);
            int getFd() { return m_errorNotificationConsumer->getFd(); }
            uint64_t readData() { return m_errorNotificationConsumer->readData(); }
            bool hasData() { return m_errorNotificationConsumer->hasData(); }
            bool hasCachedData() override { return m_errorNotificationConsumer->hasCachedData(); }
            bool initializedWithData() override { return m_errorNotificationConsumer->initializedWithData(); }
            void updateAfterRead() override { m_errorNotificationConsumer->updateAfterRead(); }
            int getError(std::string &key, std::string &opCode, std::vector<swss::FieldValueTuple> &attrs);

        private:
            std::unique_ptr<swss::NotificationConsumer> m_errorNotificationConsumer;
            std::unique_ptr<swss::DBConnector> m_errorNotificationsDb;
            int                          m_errorFlags;
            std::string                  m_errorChannelName;
    };

}
#endif /* SWSS_ERRORLISTENER_H */
