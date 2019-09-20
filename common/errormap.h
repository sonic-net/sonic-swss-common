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

#ifndef SWSS_COMMON_ERRORMAP_H
#define SWSS_COMMON_ERRORMAP_H

#include <string>
#include <map>
#include <vector>

namespace swss {

    class ErrorMap
    {
        public:
            enum SwssRC
            {
                SWSS_RC_SUCCESS,
                SWSS_RC_INVALID_PARAM,
                SWSS_RC_UNAVAIL,
                SWSS_RC_NOT_FOUND,
                SWSS_RC_NO_MEMORY,
                SWSS_RC_EXISTS,
                SWSS_RC_TABLE_FULL,
                SWSS_RC_IN_USE,
                SWSS_RC_NOT_IMPLEMENTED,
                SWSS_RC_FAILURE,
                SWSS_RC_UNKNOWN
            };

            typedef std::map<std::string, std::string> SaiToSwssRCMap;
            typedef std::vector<std::pair<std::string, SwssRC>> SwssStrToRCMap;

            static const SwssStrToRCMap m_swssStrToRC;
            static const SaiToSwssRCMap m_saiToSwssRC;

            static ErrorMap &getInstance();
            static std::string getSwssRCStr(const std::string &saiRCStr);
            static SwssRC getSwssRC(const std::string &swssRCStr);
            static std::string getSaiRCStr(SwssRC rc);

        private:
            ErrorMap() = default;
            ~ErrorMap();
    };

}

#endif /* SWSS_COMMON_ERRORMAP_H */
