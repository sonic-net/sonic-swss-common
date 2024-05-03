#pragma once

#include "dbconnector.h"

namespace swss
{

// Helper class to wait for warm/fast reboot done
class RestartWaiter
{
public:
    static bool waitAdvancedBootDone(unsigned int maxWaitSec = 180,
                                     unsigned int dbTimeout = 0,
                                     bool isTcpConn = false);

    static bool waitWarmBootDone(unsigned int maxWaitSec = 180,
                                    unsigned int dbTimeout = 0,
                                    bool isTcpConn = false);

    static bool waitFastBootDone(unsigned int maxWaitSec = 180,
                                    unsigned int dbTimeout = 0,
                                    bool isTcpConn = false);
    
    static bool isAdvancedBootInProgressHelper(swss::DBConnector &stateDb,
                                                bool checkFastBoot = false);
    static bool isAdvancedBootInProgress(swss::DBConnector &stateDb);
    static bool isFastBootInProgress(swss::DBConnector &stateDb);
    static bool isWarmBootInProgress(swss::DBConnector &stateDb);

private:
    static bool doWait(swss::DBConnector &stateDb,
                       unsigned int maxWaitSec);
};

}
