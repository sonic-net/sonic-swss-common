#include "restart_waiter.h"
#include "redispipeline.h"
#include "select.h"
#include "schema.h"
#include "subscriberstatetable.h"
#include "table.h"
#include <string>

using namespace swss;

static const std::string STATE_DB_NAME = "STATE_DB";
static const std::string STATE_DB_SEPARATOR = "|";
static const std::string RESTART_KEY = "system";
static const std::string RESTART_ENABLE_FIELD = "enable";
static const std::string FAST_REBOOT_TABLE_NAME = "FAST_REBOOT";

bool RestartWaiter::waitRestartDone(
    unsigned int maxWaitSec,
    unsigned int dbTimeout,
    bool isTcpConn)
{
    DBConnector stateDb(STATE_DB_NAME, dbTimeout, isTcpConn);
    return isWarmOrFastRestartInProgress(stateDb) ? doWait(stateDb, maxWaitSec) : true;
}

bool RestartWaiter::waitWarmRestartDone(unsigned int maxWaitSec,
                                        unsigned int dbTimeout,
                                        bool isTcpConn)
{
    DBConnector stateDb(STATE_DB_NAME, dbTimeout, isTcpConn);
    if (isFastRestartInProgress(stateDb))
    {
        // It is fast boot, just return
        return true;
    }

    return isWarmOrFastRestartInProgress(stateDb) ? doWait(stateDb, maxWaitSec) : true;
}

bool RestartWaiter::waitFastRestartDone(unsigned int maxWaitSec,
                                        unsigned int dbTimeout,
                                        bool isTcpConn)
{
    DBConnector stateDb(STATE_DB_NAME, dbTimeout, isTcpConn);
    if (!isFastRestartInProgress(stateDb))
    {
        // Fast boot is not in progress
        return true;
    }

    return isWarmOrFastRestartInProgress(stateDb) ? doWait(stateDb, maxWaitSec) : true;
}

bool RestartWaiter::doWait(DBConnector &stateDb,
                           unsigned int maxWaitSec)
{
    if (maxWaitSec == 0)
    {
        SWSS_LOG_ERROR("Error: invalid maxWaitSec value 0, must be larger than 0");
        return false;
    }

    int selectTimeout = static_cast<int>(maxWaitSec) * 1000;

    SubscriberStateTable restartEnableTable(&stateDb, STATE_WARM_RESTART_ENABLE_TABLE_NAME);
    Select s;
    s.addSelectable(&restartEnableTable);

    auto start = std::chrono::steady_clock::now();
    while (1)
    {
        Selectable *sel = NULL;
        int ret = s.select(&sel, selectTimeout, true);

        if (ret == Select::OBJECT)
        {
            KeyOpFieldsValuesTuple kco;
            restartEnableTable.pop(kco);
            auto &key = kfvKey(kco);
            if (key == RESTART_KEY)
            {
                auto& values = kfvFieldsValues(kco);
                for (auto& fvt: values)
                {
                    auto& field = fvField(fvt);
                    auto& value = fvValue(fvt);
                    if (field == RESTART_ENABLE_FIELD)
                    {
                        if (value == "false")
                        {
                            return true;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
        }
        else if (ret == Select::ERROR)
        {
            SWSS_LOG_NOTICE("Error: wait restart done got error - %s!", strerror(errno));
        }
        else if (ret == Select::TIMEOUT)
        {
            SWSS_LOG_INFO("Timeout: wait restart done got select timeout");
        }
        else if (ret == Select::SIGNALINT)
        {
            return false;
        }

        auto end = std::chrono::steady_clock::now();
        int delay = static_cast<int>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

        selectTimeout -= delay;
        if (selectTimeout <= 0)
        {
            return false;
        }
    }
}

bool RestartWaiter::isWarmOrFastRestartInProgress(DBConnector &stateDb)
{
    auto ret = stateDb.hget(STATE_WARM_RESTART_ENABLE_TABLE_NAME + STATE_DB_SEPARATOR + RESTART_KEY, RESTART_ENABLE_FIELD);
    return ret && *ret.get() == "true";
}

bool RestartWaiter::isFastRestartInProgress(DBConnector &stateDb)
{
    auto ret = stateDb.get(FAST_REBOOT_TABLE_NAME + STATE_DB_SEPARATOR + RESTART_KEY);
    return ret.get() != nullptr;
}
