#ifndef SWSS_ADVANCED_RESTART_H
#define SWSS_ADVANCED_RESTART_H

#include <string>
#include "dbconnector.h"
#include "table.h"

#define MAXIMUM_ADVANCEDRESTART_TIMER_VALUE 9999
#define DISABLE_ADVANCEDRESTART_TIMER_VALUE MAXIMUM_ADVANCEDRESTART_TIMER_VALUE

namespace swss {

class AdvancedStart
{
public:
    enum AdvancedStartState
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

    typedef std::map<AdvancedStartState, std::string>  AdvancedStartStateNameMap;
    static const AdvancedStartStateNameMap advancedStartStateNameMap;

    typedef std::map<DataCheckState, std::string>  DataCheckStateNameMap;
    static const DataCheckStateNameMap dataCheckStateNameMap;

    static AdvancedStart &getInstance(void);

    static void initialize(const std::string &app_name,
                           const std::string &docker_name,
                           unsigned int db_timeout = 0,
                           bool isTcpConn = false);

    static bool checkAdvancedStart(const std::string &app_name,
                                   const std::string &docker_name,
                                   const bool incr_restore_cnt = true);

    static bool isAdvancedStart(void);

    static bool isSystemAdvancedRebootEnabled(void);

    static void getAdvancedStartState(const std::string &app_name,
                                      AdvancedStartState    &state);

    static void setAdvancedStartState(const std::string &app_name,
                                      AdvancedStartState     state);

    static uint32_t getAdvancedStartTimer(const std::string &app_name,
                                          const std::string &docker_name);

    static void setDataCheckState(const std::string &app_name,
                                  DataCheckStage stage,
                                  DataCheckState state);

    static DataCheckState getDataCheckState(const std::string &app_name,
                                                       DataCheckStage stage);
private:
    std::shared_ptr<swss::DBConnector>   m_stateDb;
    std::shared_ptr<swss::DBConnector>   m_cfgDb;
    std::unique_ptr<Table>               m_stateAdvancedRestartEnableTable;
    std::unique_ptr<Table>               m_stateAdvancedRestartTable;
    std::unique_ptr<Table>               m_cfgAdvancedRestartTable;
    bool                                 m_initialized;
    bool                                 m_enabled;
    bool                                 m_systemAdvancedRebootEnabled;
};

}

#endif
