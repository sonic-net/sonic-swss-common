#pragma once

#include "dbconnector.h"

namespace swss
{

// Helper class to wait for warm/fast reboot done
class RestartWaiter
{
public:
    static bool waitRestartDone(unsigned int maxWaitSec = 180,
                                unsigned int dbTimeout = 0,
                                bool isTcpConn = false);

    static bool waitWarmRestartDone(unsigned int maxWaitSec = 180,
                                    unsigned int dbTimeout = 0,
                                    bool isTcpConn = false);

    static bool waitFastRestartDone(unsigned int maxWaitSec = 180,
                                    unsigned int dbTimeout = 0,
                                    bool isTcpConn = false);

private:
    static bool doWait(swss::DBConnector &stateDb,
                       unsigned int maxWaitSec);

    static bool isWarmOrFastRestartInProgress(swss::DBConnector &stateDb);
    static bool isFastRestartInProgress(swss::DBConnector &stateDb);
};

}
