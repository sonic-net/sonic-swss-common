#ifndef SWSS_WARM_RESTART_H
#define SWSS_WARM_RESTART_H

#include <string>
#include "dbconnector.h"
#include "table.h"

#define MAXIMUM_WARMRESTART_TIMER_VALUE 9999
#define DISABLE_WARMRESTART_TIMER_VALUE MAXIMUM_WARMRESTART_TIMER_VALUE

namespace swss {

class WarmStart
{
public:
    enum WarmStartState
    {
        INITIALIZED,
        RESTORED,
        REPLAYED,
        RECONCILED,
        WSDISABLED,
        WSUNKNOWN,
    };

    enum DataCheckState
    {
        CHECK_IGNORED,
        CHECK_PASSED,
        CHECK_FAILED,
    };

    enum DataCheckStage
    {
        STAGE_SHUTDOWN,
        STAGE_RESTORE,
    };

    typedef std::map<WarmStartState, std::string>  WarmStartStateNameMap;
    static const WarmStartStateNameMap warmStartStateNameMap;

    typedef std::map<DataCheckState, std::string>  DataCheckStateNameMap;
    static const DataCheckStateNameMap dataCheckStateNameMap;

    static WarmStart &getInstance(void);

    static void initialize(const std::string &app_name,
                           const std::string &docker_name,
                           unsigned int db_timeout = 0,
                           bool isTcpConn = false);

    static bool checkWarmStart(const std::string &app_name,
                               const std::string &docker_name,
                               const bool incr_restore_cnt = true);

    static bool isWarmStart(void);

    static bool isSystemWarmRebootEnabled(void);

    static void getWarmStartState(const std::string &app_name,
                                  WarmStartState    &state);

    static void setWarmStartState(const std::string &app_name,
                                  WarmStartState     state);

    static uint32_t getWarmStartTimer(const std::string &app_name,
                                      const std::string &docker_name);

    static void setDataCheckState(const std::string &app_name,
                                  DataCheckStage stage,
                                  DataCheckState state);

    static DataCheckState getDataCheckState(const std::string &app_name,
                                                       DataCheckStage stage);
private:
    std::shared_ptr<swss::DBConnector>   m_stateDb;
    std::shared_ptr<swss::DBConnector>   m_cfgDb;
    std::unique_ptr<Table>               m_stateWarmRestartEnableTable;
    std::unique_ptr<Table>               m_stateWarmRestartTable;
    std::unique_ptr<Table>               m_cfgWarmRestartTable;
    bool                                 m_initialized;
    bool                                 m_enabled;
    bool                                 m_systemWarmRebootEnabled;
};

}

#endif
