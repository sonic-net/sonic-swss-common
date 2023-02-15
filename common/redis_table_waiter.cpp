#include "redis_table_waiter.h"
#include "select.h"
#include "subscriberstatetable.h"

using namespace swss;

bool RedisTableWaiter::waitUntil(
    DBConnector &db,
    const std::string &tableName,
    unsigned int maxWaitSec,
    CheckFunc &checkFunc)
{
    if (maxWaitSec == 0)
    {
        SWSS_LOG_ERROR("Error: invalid maxWaitSec value 0, must be larger than 0");
        return false;
    }

    SubscriberStateTable table(&db, tableName);
    Select s;
    s.addSelectable(&table);

    int maxWaitMs = static_cast<int>(maxWaitSec) * 1000;
    int selectTimeout = maxWaitMs;
    auto start = std::chrono::steady_clock::now();
    while(1)
    {
        Selectable *sel = NULL;
        int ret = s.select(&sel, selectTimeout, true);
        if (ret == Select::OBJECT)
        {
            KeyOpFieldsValuesTuple kco;
            table.pop(kco);
            if (checkFunc(kco))
            {
                return true;
            }
        }
        else if (ret == Select::ERROR)
        {
            SWSS_LOG_NOTICE("Error: wait redis table got error - %s!", strerror(errno));
        }
        else if (ret == Select::TIMEOUT)
        {
            SWSS_LOG_INFO("Timeout: wait redis table got select timeout");
        }
        else if (ret == Select::SIGNALINT)
        {
            return false;
        }

        auto end = std::chrono::steady_clock::now();
        int delay = static_cast<int>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

        if (delay >= maxWaitMs)
        {
            return false;
        }

        selectTimeout = maxWaitMs - delay;
    }

    return false;
}

bool RedisTableWaiter::waitUntilFieldSet(
    DBConnector &db,
    const std::string &tableName,
    const std::string &key,
    const std::string &fieldName,
    unsigned int maxWaitSec,
    ConditionFunc &cond)
{
    auto sep = SonicDBConfig::getSeparator(&db);
    auto value = db.hget(tableName + sep + key, fieldName);
    if (value && cond(*value.get()))
    {
        return true;
    }

    CheckFunc checkFunc = [&](const KeyOpFieldsValuesTuple &kco) -> bool {
        if (SET_COMMAND == kfvOp(kco))
        {
            if (key == kfvKey(kco))
            {
                auto& values = kfvFieldsValues(kco);
                for (auto& fvt: values)
                {
                    if (fieldName == fvField(fvt))
                    {
                        return cond(fvValue(fvt));
                    }
                }
            }
        }

        return false;
    };
    return waitUntil(db, tableName, maxWaitSec, checkFunc);
}

bool RedisTableWaiter::waitUntilKeySet(
    DBConnector &db,
    const std::string &tableName,
    const std::string &key,
    unsigned int maxWaitSec)
{
    auto sep = SonicDBConfig::getSeparator(&db);
    if (db.exists(tableName + sep + key))
    {
        return true;
    }

    CheckFunc checkFunc = [&](const KeyOpFieldsValuesTuple &kco) -> bool {
        if (SET_COMMAND == kfvOp(kco))
        {
            return key == kfvKey(kco);
        }
        return false;
    };
    return waitUntil(db, tableName, maxWaitSec, checkFunc);
}

bool RedisTableWaiter::waitUntilKeyDel(
    DBConnector &db,
    const std::string &tableName,
    const std::string &key,
    unsigned int maxWaitSec)
{
    auto sep = SonicDBConfig::getSeparator(&db);
    if (!db.exists(tableName + sep + key))
    {
        return true;
    }

    CheckFunc checkFunc = [&](const KeyOpFieldsValuesTuple &kco) -> bool {
        if (DEL_COMMAND == kfvOp(kco))
        {
            return key == kfvKey(kco);
        }
        return false;
    };
    return waitUntil(db, tableName, maxWaitSec, checkFunc);
}
