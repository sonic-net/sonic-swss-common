#include "restart_waiter.h"
#include "redis_table_waiter.h"
#include "redispipeline.h"
#include "schema.h"
#include <boost/algorithm/string.hpp>
#include <string>

using namespace swss;

static const std::string STATE_DB_NAME = "STATE_DB";
static const std::string STATE_DB_SEPARATOR = "|";
static const std::string RESTART_KEY = "system";
static const std::string RESTART_ENABLE_FIELD = "enable";
static const std::string FAST_REBOOT_TABLE_NAME = "FAST_RESTART_ENABLE_TABLE";

// waitAdvancedBootDone
bool RestartWaiter::waitAdvancedBootDone(
    unsigned int maxWaitSec,
    unsigned int dbTimeout,
    bool isTcpConn)
{
    DBConnector stateDb(STATE_DB_NAME, dbTimeout, isTcpConn);
    return isAdvancedBootInProgress(stateDb) ? doWait(stateDb, maxWaitSec) : true;
}

bool RestartWaiter::waitWarmBootDone(
    unsigned int maxWaitSec,
    unsigned int dbTimeout,
    bool isTcpConn)
{
    DBConnector stateDb(STATE_DB_NAME, dbTimeout, isTcpConn);
    if (isFastBootInProgress(stateDb))
    {
        // It is fast boot, just return
        return true;
    }

    return isAdvancedBootInProgress(stateDb) ? doWait(stateDb, maxWaitSec) : true;
}

bool RestartWaiter::waitFastBootDone(
    unsigned int maxWaitSec,
    unsigned int dbTimeout,
    bool isTcpConn)
{
    DBConnector stateDb(STATE_DB_NAME, dbTimeout, isTcpConn);
    if (!isFastBootInProgress(stateDb))
    {
        // Fast boot is not in progress
        return true;
    }

    return isAdvancedBootInProgress(stateDb) ? doWait(stateDb, maxWaitSec) : true;
}

bool RestartWaiter::doWait(DBConnector &stateDb,
                           unsigned int maxWaitSec)
{
    RedisTableWaiter::ConditionFunc condFunc = [](const std::string &value) -> bool {
        std::string copy = value;
        boost::to_lower(copy);
        return copy == "false";
    };
    return RedisTableWaiter::waitUntilFieldSet(stateDb,
                                               STATE_WARM_RESTART_ENABLE_TABLE_NAME,
                                               RESTART_KEY,
                                               RESTART_ENABLE_FIELD,
                                               maxWaitSec,
                                               condFunc);
}

bool RestartWaiter::isAdvancedBootInProgress(DBConnector &stateDb)
{
    return isAdvancedBootInProgressHelper(stateDb);
}

bool RestartWaiter::isAdvancedBootInProgressHelper(DBConnector &stateDb,
                                                   bool checkFastBoot)
{
    std::string table_name = checkFastBoot ? FAST_REBOOT_TABLE_NAME : STATE_WARM_RESTART_ENABLE_TABLE_NAME;
    auto ret = stateDb.hget(table_name + STATE_DB_SEPARATOR + RESTART_KEY, RESTART_ENABLE_FIELD);
    if (ret) {
        std::string value = *ret.get();
        boost::to_lower(value);
        return value == "true";
    }
    return false;
}

bool RestartWaiter::isFastBootInProgress(DBConnector &stateDb)
{
    return isAdvancedBootInProgressHelper(stateDb, true);
}

bool RestartWaiter::isWarmBootInProgress(swss::DBConnector &stateDb)
{
    return isAdvancedBootInProgress(stateDb) && !isFastBootInProgress(stateDb);
}
