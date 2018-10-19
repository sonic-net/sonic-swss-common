#ifndef SWSS_WARM_RESTART_H
#define SWSS_WARM_RESTART_H

#include <string>
#include "dbconnector.h"
#include "table.h"

#define MAXIMUM_WARMRESTART_TIMER_VALUE 9999

namespace swss {

class WarmStart
{
public:
    enum WarmStartState
    {
        INITIALIZED,
        RESTORED,
        RECONCILED,
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
                           const std::string &docker_name = "swss",
                           unsigned int db_timeout = 0,
                           const std::string &db_hostname = "",
                           int db_port = 6379);

    static bool checkWarmStart(const std::string &app_name,
                               const std::string &docker_name = "swss");

    static bool isWarmStart(void);

    static void setWarmStartState(const std::string &app_name,
                                  WarmStartState     state);

    static uint32_t getWarmStartTimer(const std::string &app_name,
                                      const std::string &docker_name ="swss");

    static void setDataCheckState(const std::string &app_name,
                                  DataCheckStage stage,
                                  DataCheckState state);

    static DataCheckState getDataCheckState(const std::string &app_name,
                                                       DataCheckStage stage);
private:
    std::shared_ptr<swss::DBConnector>   m_stateDb;
    std::shared_ptr<swss::DBConnector>   m_cfgDb;
    std::unique_ptr<Table>               m_stateWarmRestartTable;
    std::unique_ptr<Table>               m_cfgWarmRestartTable;
    bool                                 m_initialized;
    bool                                 m_enabled;
};

}

#endif
