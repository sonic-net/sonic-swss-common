#pragma once

#include <functional>
#include <string>

#include "dbconnector.h"

namespace swss
{

class RedisTableWaiter
{
public:
    typedef std::function<bool(const std::string &)> ConditionFunc;
    typedef std::function<bool(const KeyOpFieldsValuesTuple &)> CheckFunc;

    static bool waitUntilFieldSet(DBConnector &db,
                          const std::string &tableName,
                          const std::string &key,
                          const std::string &fieldName,
                          unsigned int maxWaitSec,
                          ConditionFunc &cond);


    static bool waitUntilKeySet(DBConnector &db,
                          const std::string &tableName,
                          const std::string &key,
                          unsigned int maxWaitSec);

    static bool waitUntilKeyDel(DBConnector &db,
                          const std::string &tableName,
                          const std::string &key,
                          unsigned int maxWaitSec);

    static bool waitUntil(
        DBConnector &db,
        const std::string &tableName,
        unsigned int maxWaitSec,
        CheckFunc &checkFunc);

};

}