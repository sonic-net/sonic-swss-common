/*
 * Copyright 2019 Broadcom Inc.
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

#include <vector>
#include "errorlistener.h"
#include "common/json.h"
#include "common/json.hpp"
using json = nlohmann::json;

namespace swss {

    /* Creates an error listener object that listens for h/w errors on table entries
     * in the APP_DB table that the caller is intereted in. */

    ErrorListener::ErrorListener(std::string appTableName, int errFlags)
    {
        SWSS_LOG_ENTER();

        m_errorFlags                = errFlags;
        m_errorChannelName          = getErrorChannelName(appTableName);
        m_errorNotificationsDb      = new DBConnector(ERROR_DB, DBConnector::DEFAULT_UNIXSOCKET, 0);
        m_errorNotificationConsumer = new swss::NotificationConsumer(m_errorNotificationsDb,
                m_errorChannelName);
    }

    ErrorListener::~ErrorListener()
    {
        SWSS_LOG_ENTER();
        delete m_errorNotificationConsumer;
        delete m_errorNotificationsDb;
    }

    /* Returns the Error notification corresponding to an entry in the APP_DB.
     * Owner of the error listener object calls this function whenever there is
     * a select event on the error listerner.
     * 0 is returned if a failure or postive ack of interest is read successfully.
     * -1 is returned if there are no errors of interest to the caller. */
    int ErrorListener::getError(std::string &key, std::string &opCode,
            std::vector<swss::FieldValueTuple> &attrs)
    {
        std::string op, data;
        std::vector<swss::FieldValueTuple> values;

        SWSS_LOG_ENTER();

        m_errorNotificationConsumer->pop(op, data, values);
        SWSS_LOG_DEBUG("ErrorListener op: %s data: %s", op.c_str(), data.c_str());

        json j = json::parse(data);

        // Filter the error notifications that the caller is not interested in.
        if (!(((m_errorFlags & ERR_NOTIFY_POSITIVE_ACK) && (j["rc"] == "SWSS_RC_SUCCESS")) ||
                    ((m_errorFlags & ERR_NOTIFY_FAIL) && (j["rc"] != "SWSS_RC_SUCCESS"))))
        {
            return -1;
        }

        key = j["key"];
        opCode = j["operation"];
        for (json::iterator it = j.begin(); it != j.end(); ++it)
        {
            if(it.key() != "key" && it.key() != "operation")
            {
                attrs.emplace_back(it.key(), it.value());
            }
        }
        return 0;
    }
}
