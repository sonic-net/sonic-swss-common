#include <string>
#include <climits>

#include "advanced_restart.h"
#include "logger.h"
#include "schema.h"

namespace swss {

const AdvancedStart::AdvancedStartStateNameMap AdvancedStart::advancedStartStateNameMap =
{
    {INITIALIZED,   "initialized"},
    {RESTORED,      "restored"},
    {REPLAYED,      "replayed"},
    {RECONCILED,    "reconciled"},
    {WSDISABLED,    "disabled"},
    {WSUNKNOWN,     "unknown"}
};

const AdvancedStart::DataCheckStateNameMap AdvancedStart::dataCheckStateNameMap =
{
    {CHECK_IGNORED,   "ignored"},
    {CHECK_PASSED,    "passed"},
    {CHECK_FAILED,    "failed"}
};

AdvancedStart &AdvancedStart::getInstance(void)
{
    static AdvancedStart m_advancedStart;
    return m_advancedStart;
}

/*
 * AdvancedStart's initialization method -- to be invoked once per application.
 */
void AdvancedStart::initialize(const std::string &app_name,
                               const std::string &docker_name,
                               unsigned int db_timeout,
                               bool isTcpConn)
{
    auto& advancedStart = getInstance();

    if (advancedStart.m_initialized)
    {
        return;
    }

    /* Use unix socket for db connection by default */
    advancedStart.m_stateDb =
        std::make_shared<swss::DBConnector>("STATE_DB", db_timeout, isTcpConn);
    advancedStart.m_cfgDb =
        std::make_shared<swss::DBConnector>("CONFIG_DB", db_timeout, isTcpConn);

    advancedStart.m_stateAdvancedRestartEnableTable =
            std::unique_ptr<Table>(new Table(advancedStart.m_stateDb.get(), STATE_ADVANCED_RESTART_ENABLE_TABLE_NAME));
    advancedStart.m_stateAdvancedRestartTable =
            std::unique_ptr<Table>(new Table(advancedStart.m_stateDb.get(), STATE_ADVANCED_RESTART_TABLE_NAME));
    advancedStart.m_cfgAdvancedRestartTable =
            std::unique_ptr<Table>(new Table(advancedStart.m_cfgDb.get(), CFG_ADVANCED_RESTART_TABLE_NAME));

    advancedStart.m_initialized = true;
}

/*
 * <1> Upon system reboot, the system enable knob will be checked.
 * If enabled, database data will be preserved, if not, database will be flushed.
 * No need to check docker level knobs in this case since the whole system is being rebooted .

 * <2> Upon docker service start, first to check system knob.
 * if enabled, docker advanced-start should be performed, otherwise system advanced-reboot will be ruined.
 * If system knob disabled, while docker knob enabled, this is likely an individual docker
 * advanced-restart request.

 * Within each application which should take care advanced start case,
 * when the system knob or docker knob enabled, we do further check on the
 * actual advanced-start state ( restore_count), if no advanced-start state data available,
 * the database has been flushed, do cold start. Otherwise advanced-start.
 */

/*
 * Method to verify/obtain the state of Advanced-Restart feature for any advanced-reboot
 * capable component. Typically, this function will be called during initialization of
 * SONiC modules; however, method could be invoked at any given point to verify the
 * latest state of Advanced-Restart functionality and to update the restore_count value.
 * A flag of incr_restore_cnt is used to increase the restore cnt or not. Default: true
 */
bool AdvancedStart::checkAdvancedStart(const std::string &app_name,
                                       const std::string &docker_name,
                                       const bool incr_restore_cnt)
{
    std::string value;

    auto& advancedStart = getInstance();

    // Check system level advanced-restart config first
    advancedStart.m_stateAdvancedRestartEnableTable->hget("system", "enable", value);
    if (value == "true")
    {
        advancedStart.m_enabled = true;
        advancedStart.m_systemAdvancedRebootEnabled = true;
    }

    // Check docker level advanced-restart configuration
    advancedStart.m_stateAdvancedRestartEnableTable->hget(docker_name, "enable", value);
    if (value == "true")
    {
        advancedStart.m_enabled = true;
    }

    // For cold start, the whole state db will be flushed including advanced start table.
    // Create the entry for this app here.
    if (!advancedStart.m_enabled)
    {
        advancedStart.m_stateAdvancedRestartTable->hset(app_name, "restore_count", "0");
        return false;
    }

    uint32_t restore_count = 0;
    advancedStart.m_stateAdvancedRestartTable->hget(app_name, "restore_count", value);
    if (value == "")
    {
        SWSS_LOG_WARN("%s doing advanced start, but restore_count not found in stateDB %s table, fall back to cold start",
                app_name.c_str(), STATE_ADVANCED_RESTART_TABLE_NAME);
        advancedStart.m_enabled = false;
        advancedStart.m_systemAdvancedRebootEnabled = false;
        advancedStart.m_stateAdvancedRestartTable->hset(app_name, "restore_count", "0");
        return false;
    }

    if (incr_restore_cnt)
    {
        restore_count = (uint32_t)stoul(value);
        restore_count++;
        advancedStart.m_stateAdvancedRestartTable->hset(app_name, "restore_count",
                                                std::to_string(restore_count));
    }
    SWSS_LOG_NOTICE("%s doing advanced start, restore count %d", app_name.c_str(),
                    restore_count);

    return true;
}

/*
 * Obtain the time-interval defined by a advanced-restart-capable application
 * corresponding to the amount of time required to complete a full-restart cycle.
 * This time-duration (advancedStartTimer) will be taken into account by the
 * advanced-restart logic to kick-off the reconciliation process of this application.
 * A returned value of '0' implies that no valid interval was found.
 */
uint32_t AdvancedStart::getAdvancedStartTimer(const std::string &app_name,
                                              const std::string &docker_name)
{
    auto& advancedStart = getInstance();
    std::string timer_name = app_name + "_timer";
    std::string timer_value_str;

    advancedStart.m_cfgAdvancedRestartTable->hget(docker_name, timer_name, timer_value_str);

    unsigned long int temp_value = strtoul(timer_value_str.c_str(), NULL, 0);

    if (temp_value != 0 && temp_value != ULONG_MAX &&
        temp_value <= MAXIMUM_ADVANCEDRESTART_TIMER_VALUE)
    {
        SWSS_LOG_NOTICE("Getting advancedStartTimer for docker: %s, app: %s, value: %lu",
                        docker_name.c_str(), app_name.c_str(), temp_value);
        return (uint32_t)temp_value;
    }

    SWSS_LOG_NOTICE("advancedStartTimer is not configured or invalid for docker: %s, app: %s",
                    docker_name.c_str(), app_name.c_str());
    return 0;
}

bool AdvancedStart::isAdvancedStart(void)
{
    auto& advancedStart = getInstance();

    return advancedStart.m_enabled;
}

bool AdvancedStart::isSystemAdvancedRebootEnabled(void)
{
    auto& advancedStart = getInstance();

    return advancedStart.m_systemAdvancedRebootEnabled;
}

void AdvancedStart::getAdvancedStartState(const std::string &app_name, AdvancedStartState &state)
{
    std::string statestr;

    auto& advancedStart = getInstance();

    state = RECONCILED;

    if (!isAdvancedStart())
    {
        return;
    }

    advancedStart.m_stateAdvancedRestartTable->hget(app_name, "state", statestr);

    /* If advanced-start is enabled, state cannot be assumed as Reconciled
     * It should be set to unknown
     */
    state = WSUNKNOWN;

    for (auto it = advancedStartStateNameMap.begin(); it != advancedStartStateNameMap.end(); it++)
    {
        if (it->second == statestr)
        {
            state = it->first;
            break;
        }
    }

    SWSS_LOG_INFO("%s advanced start state get %s(%d)",
                    app_name.c_str(), statestr.c_str(), state);

    return;
}

// Set the advancedStart FSM state for a particular application.
void AdvancedStart::setAdvancedStartState(const std::string &app_name, AdvancedStartState state)
{
    auto& advancedStart = getInstance();

    advancedStart.m_stateAdvancedRestartTable->hset(app_name,
                                                    "state",
                                                    advancedStartStateNameMap.at(state).c_str());

    SWSS_LOG_NOTICE("%s advance start state changed to %s",
                    app_name.c_str(),
                    advancedStartStateNameMap.at(state).c_str());
}

// Set the advancedStart data check state for a particular application.
void AdvancedStart::setDataCheckState(const std::string &app_name, DataCheckStage stage, DataCheckState state)
{
    auto& advancedStart = getInstance();

    std::string stageField = "restore_check";

    if (stage == STAGE_SHUTDOWN)
    {
        stageField = "shutdown_check";
    }
    advancedStart.m_stateAdvancedRestartTable->hset(app_name,
                                                    stageField,
                                                    dataCheckStateNameMap.at(state).c_str());

    SWSS_LOG_NOTICE("%s %s result %s",
                    app_name.c_str(),
                    stageField.c_str(),
                    dataCheckStateNameMap.at(state).c_str());
}

AdvancedStart::DataCheckState AdvancedStart::getDataCheckState(const std::string &app_name, DataCheckStage stage)
{
    auto& advancedStart = getInstance();

    std::string stateStr;
    std::string stageField = "restore_check";

    if (stage == STAGE_SHUTDOWN)
    {
        stageField = "shutdown_check";
    }
    advancedStart.m_stateAdvancedRestartTable->hget(app_name,
                                                    stageField,
                                                    stateStr);

    DataCheckState state = CHECK_IGNORED;

    for (auto it = dataCheckStateNameMap.begin(); it != dataCheckStateNameMap.end(); it++)
    {
        if (it->second == stateStr)
        {
            state = it->first;
            break;
        }
    }

    SWSS_LOG_NOTICE("Getting %s %s result %s",
                    app_name.c_str(),
                    stageField.c_str(),
                    stateStr.c_str());

    return state;
}

}
