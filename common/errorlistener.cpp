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

#include <vector>
#include "errorlistener.h"
#include "common/json.h"
#include "common/json.hpp"
using json = nlohmann::json;

namespace swss {

    /**
     * Function Description:
     *    @brief To create Error listener object that listens for h/w programming 
     *    errors on table entries in the APP_DB table
     *
     * Arguments:
     *    @param[in] appTableName - APP DB table name
     *    @param[in] errFlags     - flags
     *
     */
    ErrorListener::ErrorListener(std::string appTableName, int errFlags)
    {
        SWSS_LOG_ENTER();

        m_errorFlags                = errFlags;
        m_errorChannelName          = getErrorChannelName(appTableName);
        m_errorNotificationsDb      = std::unique_ptr<swss::DBConnector>(new DBConnector(ERROR_DB, DBConnector::DEFAULT_UNIXSOCKET, 0));
        m_errorNotificationConsumer = std::unique_ptr<swss::NotificationConsumer>
            (new swss::NotificationConsumer (m_errorNotificationsDb.get(), m_errorChannelName));
    }

    /**
     * Function Description:
     *    @brief Get error channel name for a given APP DB table
     *
     * Arguments:
     *    @param[in] appTableName - APP DB table name
     *
     * Return Values:
     *    @return Error channel name
     */
    std::string ErrorListener::getErrorChannelName(std::string& appTableName)
    {
        SWSS_LOG_ENTER();
        std::string errorChannel = "ERROR_";
        errorChannel += appTableName + "_CHANNEL";

        return errorChannel;
    }

    /**
     * Function Description:
     *    @brief Get the notification entry details
     *
     * Error listener object instantiated by the application gets selected on 
     * failure/success notitication from error handling framework. Then, the 
     * application uses this function to fetch more details about the notification.

     * Arguments:
     *    @param[out] key    - Key corresponding to the notification entry
     *    @param[out] opCode - operation corresponding to the entry
     *    @param[out] attrs  - Attributes corresponding to the entry
     *
     * Return Values:
     *    @return 0,  if the entry is read successfully
     *    @return -1, if failed to fetch the entry or the entry is not 
     *                of interest to the caller
     */
    int ErrorListener::getError(std::string &key, std::string &opCode,
            std::vector<swss::FieldValueTuple> &attrs)
    {
        SWSS_LOG_ENTER();

        std::string op, data;
        std::vector<swss::FieldValueTuple> values;

        m_errorNotificationConsumer->pop(op, data, values);
        SWSS_LOG_DEBUG("ErrorListener op: %s data: %s", op.c_str(), data.c_str());

        json j;
        try
        {
            j = json::parse(data);
        }
        catch (...)
        {
            SWSS_LOG_ERROR("Parse error on error handling data : %s", data.c_str());
            return -2;
        }

        // Filter the error notifications that the caller is not interested in.
        if ((j["rc"] == "SWSS_RC_SUCCESS") && !(m_errorFlags & ERR_NOTIFY_POSITIVE_ACK))
        {
            return -1;
        }
        if ((j["rc"] != "SWSS_RC_SUCCESS") && !(m_errorFlags & ERR_NOTIFY_FAIL))
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
