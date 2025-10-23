#include <string>
#include <climits>
#include "logger.h"
#include "schema.h"
#include "timestamp.h"
#include "warm_restart.h"

namespace swss {

const std::string WarmStart::kNsfManagerNotificationChannel =
    "NSF_MANAGER_COMMON_NOTIFICATION_CHANNEL";
const std::string WarmStart::kRegistrationFreezeKey = "freeze";
const std::string WarmStart::kRegistrationCheckpointKey = "checkpoint";
const std::string WarmStart::kRegistrationReconciliationKey = "reconciliation";
const std::string WarmStart::kRegistrationTimestampKey = "timestamp";

const WarmStart::WarmStartStateNameMap* WarmStart::warmStartStateNameMap()
{
    static const auto* const warmStartStateNameMap =
        new WarmStartStateNameMap({
            {INITIALIZED,   "initialized"},
            {RESTORED,      "restored"},
            {REPLAYED,      "replayed"},
            {RECONCILED,    "reconciled"},
            {WSDISABLED,    "disabled"},
            {WSUNKNOWN,     "unknown"},
            {FROZEN,        "frozen"},
            {QUIESCENT,     "quiescent"},
            {CHECKPOINTED,  "checkpointed"},
            {FAILED,        "failed"}
        });
    return warmStartStateNameMap;
}

const WarmStart::DataCheckStateNameMap* WarmStart::dataCheckStateNameMap()
{
    static const auto* const dataCheckStateNameMap =
        new DataCheckStateNameMap({
            {CHECK_IGNORED,   "ignored"},
            {CHECK_PASSED,    "passed"},
            {CHECK_FAILED,    "failed"}
        });
    return dataCheckStateNameMap;
}

const WarmStart::WarmBootNotificationNameMap* WarmStart::warmBootNotificationNameMap()
{
    static const auto* const  warmBootNotificationNameMap =
        new WarmBootNotificationNameMap({
          {WarmBootNotification::kFreeze,         "freeze"},
          {WarmBootNotification::kUnfreeze,       "unfreeze"},
          {WarmBootNotification::kCheckpoint,     "checkpoint"},
        });
    return warmBootNotificationNameMap;
}

const WarmStart::WarmBootNotificationReverseMap* WarmStart::warmBootNotificationReverseMap()
{
    static const auto* const  warmBootNotificationReverseMap =
        new WarmBootNotificationReverseMap({
          {"freeze",     WarmBootNotification::kFreeze},
          {"unfreeze",   WarmBootNotification::kUnfreeze},
          {"checkpoint", WarmBootNotification::kCheckpoint},
        });
    return warmBootNotificationReverseMap;
}

WarmStart &WarmStart::getInstance(void)
{
    static WarmStart m_warmStart;
    return m_warmStart;
}

/*
 * WarmStart's initialization method -- to be invoked once per application.
 */
void WarmStart::initialize(const std::string &app_name,
                           const std::string &docker_name,
                           unsigned int db_timeout,
                           bool isTcpConn)
{
    auto& warmStart = getInstance();

    if (warmStart.m_initialized)
    {
        return;
    }

    warmStart.m_appName = app_name;
    warmStart.m_dockerName = docker_name;

    /* Use unix socket for db connection by default */
    warmStart.m_stateDb =
        std::make_shared<swss::DBConnector>("STATE_DB", db_timeout, isTcpConn);
    warmStart.m_cfgDb =
        std::make_shared<swss::DBConnector>("CONFIG_DB", db_timeout, isTcpConn);

    warmStart.m_stateWarmRestartEnableTable =
            std::unique_ptr<Table>(new Table(warmStart.m_stateDb.get(), STATE_WARM_RESTART_ENABLE_TABLE_NAME));
    warmStart.m_stateWarmRestartTable =
            std::unique_ptr<Table>(new Table(warmStart.m_stateDb.get(), STATE_WARM_RESTART_TABLE_NAME));
    warmStart.m_cfgWarmRestartTable =
            std::unique_ptr<Table>(new Table(warmStart.m_cfgDb.get(), CFG_WARM_RESTART_TABLE_NAME));

    warmStart.m_initialized = true;
}

/*
 * registerWarmBootInfo
 *
 * Register an application with NSF Manager.
 *
 * Returns: true on success, false otherwise.
 *
 * wait_for_freeze: if true, NSF Manager waits for application to freeze
 *                  and become quiescent before proceeding to state
 *                  verification and checkpointing
 * wait_for_checkpoint: if true, NSF Manager waits for application to
 *                      complete checkpointing before reboot
 * wait_for_reconciliation: if true, NSF Manager waits for application to
 *                          complete reconciliation before unfreeze
 */
bool WarmStart::registerWarmBootInfo(bool wait_for_freeze,
                                     bool wait_for_checkpoint,
                                     bool wait_for_reconciliation) {
    auto& warmStart = getInstance();

    if (!warmStart.m_initialized) {
        SWSS_LOG_ERROR("registerWarmBootInfo called before initialized");
        return false;
    }

    if (warmStart.m_dockerName.empty()) {
        SWSS_LOG_ERROR("registerWarmBootInfo: m_dockerName is empty");
        return false;
    }

    if (warmStart.m_appName.empty()) {
        SWSS_LOG_ERROR("registerWarmBootInfo: m_appName is empty");
        return false;
    }

    std::unique_ptr<Table> stateWarmRestartRegistrationTable =
        std::unique_ptr<Table>(
            new Table(warmStart.m_stateDb.get(),
                      STATE_WARM_RESTART_REGISTRATION_TABLE_NAME));

    std::string separator =
        TableBase::getTableSeparator(warmStart.m_stateDb->getDbId());
    std::string tableName =
        warmStart.m_dockerName + separator + warmStart.m_appName;

    std::vector<FieldValueTuple> values;

    values.push_back(swss::FieldValueTuple(WarmStart::kRegistrationFreezeKey,
                                           wait_for_freeze ? "true" : "false"));
    values.push_back(swss::FieldValueTuple(
        WarmStart::kRegistrationCheckpointKey,
        wait_for_checkpoint ? "true" : "false"));
    values.push_back(swss::FieldValueTuple(
        WarmStart::kRegistrationReconciliationKey,
        wait_for_reconciliation ? "true" : "false"));
    values.push_back(swss::FieldValueTuple(
        WarmStart::kRegistrationTimestampKey,
        getTimestamp()));

    stateWarmRestartRegistrationTable->set(tableName, values);

    return true;
}

/*
 * <1> Upon system reboot, the system enable knob will be checked.
 * If enabled, database data will be preserved, if not, database will be flushed.
 * No need to check docker level knobs in this case since the whole system is being rebooted .

 * <2> Upon docker service start, first to check system knob.
 * if enabled, docker warm-start should be performed, otherwise system warm-reboot will be ruined.
 * If system knob disabled, while docker knob enabled, this is likely an individual docker
 * warm-restart request.

 * Within each application which should take care warm start case,
 * when the system knob or docker knob enabled, we do further check on the
 * actual warm-start state ( restore_count), if no warm-start state data available,
 * the database has been flushed, do cold start. Otherwise warm-start.
 */

/*
 * Method to verify/obtain the state of Warm-Restart feature for any warm-reboot
 * capable component. Typically, this function will be called during initialization of
 * SONiC modules; however, method could be invoked at any given point to verify the
 * latest state of Warm-Restart functionality and to update the restore_count value.
 * A flag of incr_restore_cnt is used to increase the restore cnt or not. Default: true
 */
bool WarmStart::checkWarmStart(const std::string &app_name,
                               const std::string &docker_name,
                               const bool incr_restore_cnt)
{
    std::string value;

    auto& warmStart = getInstance();

    // Check system level warm-restart config first
    warmStart.m_stateWarmRestartEnableTable->hget("system", "enable", value);
    if (value == "true")
    {
        warmStart.m_enabled = true;
        warmStart.m_systemWarmRebootEnabled = true;
    }

    // Check docker level warm-restart configuration
    warmStart.m_stateWarmRestartEnableTable->hget(docker_name, "enable", value);
    if (value == "true")
    {
        warmStart.m_enabled = true;
    }

    // For cold start, the whole state db will be flushed including warm start table.
    // Create the entry for this app here.
    if (!warmStart.m_enabled)
    {
        warmStart.m_stateWarmRestartTable->hset(app_name, "restore_count", "0");
        return false;
    }

    uint32_t restore_count = 0;
    warmStart.m_stateWarmRestartTable->hget(app_name, "restore_count", value);
    if (value == "")
    {
        SWSS_LOG_WARN("%s doing warm start, but restore_count not found in stateDB %s table, fall back to cold start",
                app_name.c_str(), STATE_WARM_RESTART_TABLE_NAME);
        warmStart.m_enabled = false;
        warmStart.m_systemWarmRebootEnabled = false;
        warmStart.m_stateWarmRestartTable->hset(app_name, "restore_count", "0");
        return false;
    }

    if (incr_restore_cnt)
    {
        restore_count = (uint32_t)stoul(value);
        restore_count++;
        warmStart.m_stateWarmRestartTable->hset(app_name, "restore_count",
                                                std::to_string(restore_count));
    }
    SWSS_LOG_NOTICE("%s doing warm start, restore count %d", app_name.c_str(),
                    restore_count);

    return true;
}

/*
 * Obtain the time-interval defined by a warm-restart-capable application
 * corresponding to the amount of time required to complete a full-restart cycle.
 * This time-duration (warmStartTimer) will be taken into account by the
 * warm-restart logic to kick-off the reconciliation process of this application.
 * A returned value of '0' implies that no valid interval was found.
 */
uint32_t WarmStart::getWarmStartTimer(const std::string &app_name,
                                      const std::string &docker_name)
{
    auto& warmStart = getInstance();
    std::string timer_name = app_name + "_timer";
    std::string timer_value_str;

    warmStart.m_cfgWarmRestartTable->hget(docker_name, timer_name, timer_value_str);

    unsigned long int temp_value = strtoul(timer_value_str.c_str(), NULL, 0);

    if (temp_value != 0 && temp_value != ULONG_MAX &&
        temp_value <= MAXIMUM_WARMRESTART_TIMER_VALUE)
    {
        SWSS_LOG_NOTICE("Getting warmStartTimer for docker: %s, app: %s, value: %lu",
                        docker_name.c_str(), app_name.c_str(), temp_value);
        return (uint32_t)temp_value;
    }

    SWSS_LOG_NOTICE("warmStartTimer is not configured or invalid for docker: %s, app: %s",
                    docker_name.c_str(), app_name.c_str());
    return 0;
}

bool WarmStart::isWarmStart(void)
{
    auto& warmStart = getInstance();

    return warmStart.m_enabled;
}

bool WarmStart::isSystemWarmRebootEnabled(void)
{
    auto& warmStart = getInstance();

    return warmStart.m_systemWarmRebootEnabled;
}

void WarmStart::getWarmStartState(const std::string &app_name, WarmStartState &state)
{
    std::string statestr;

    auto& warmStart = getInstance();

    state = RECONCILED;

    if (!isWarmStart())
    {
        return;
    }

    warmStart.m_stateWarmRestartTable->hget(app_name, "state", statestr);

    /* If warm-start is enabled, state cannot be assumed as Reconciled
     * It should be set to unknown
     */
    state = WSUNKNOWN;

    for (auto it = warmStartStateNameMap()->begin(); it != warmStartStateNameMap()->end(); it++)
    {
        if (it->second == statestr)
        {
            state = it->first;
            break;
        }
    }
        
    SWSS_LOG_INFO("%s warm start state get %s(%d)",
                    app_name.c_str(), statestr.c_str(), state);

    return;
}

// Set the WarmStart FSM state for a particular application.
void WarmStart::setWarmStartState(const std::string &app_name, WarmStartState state)
{
    auto& warmStart = getInstance();

    warmStart.m_stateWarmRestartTable->hset(app_name,
                                            "state",
                                            warmStartStateNameMap()->at(state).c_str());

    SWSS_LOG_NOTICE("%s warm start state changed to %s",
                    app_name.c_str(),
                    warmStartStateNameMap()->at(state).c_str());
}

// Set the WarmStart data check state for a particular application.
void WarmStart::setDataCheckState(const std::string &app_name, DataCheckStage stage, DataCheckState state)
{
    auto& warmStart = getInstance();

    std::string stageField = "restore_check";

    if (stage == STAGE_SHUTDOWN)
    {
        stageField = "shutdown_check";
    }
    warmStart.m_stateWarmRestartTable->hset(app_name,
                                            stageField,
                                            dataCheckStateNameMap()->at(state).c_str());

    SWSS_LOG_NOTICE("%s %s result %s",
                    app_name.c_str(),
                    stageField.c_str(),
                    dataCheckStateNameMap()->at(state).c_str());
}

WarmStart::DataCheckState WarmStart::getDataCheckState(const std::string &app_name, DataCheckStage stage)
{
    auto& warmStart = getInstance();

    std::string stateStr;
    std::string stageField = "restore_check";

    if (stage == STAGE_SHUTDOWN)
    {
        stageField = "shutdown_check";
    }
    warmStart.m_stateWarmRestartTable->hget(app_name,
                                            stageField,
                                            stateStr);

    DataCheckState state = CHECK_IGNORED;

    for (auto it = dataCheckStateNameMap()->begin(); it != dataCheckStateNameMap()->end(); it++)
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

} // namespace swss
