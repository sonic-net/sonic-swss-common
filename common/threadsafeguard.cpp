#include <iostream>
#include <sstream>
#include <system_error>

#include "common/logger.h"
#include "common/threadsafeguard.h"

using namespace std;
using namespace swss;

ThreadSafeGuard::ThreadSafeGuard(atomic<bool> &running, const string &info)
:m_running(running)
{
    auto currentlyRunning = m_running.exchange(true);
    if (currentlyRunning)
    {
        string errmsg = "Current thread '" + info + "' conflict with other thread.";
        SWSS_LOG_ERROR("%s", errmsg.c_str());
        throw system_error(make_error_code(errc::operation_in_progress), errmsg.c_str());
    }
}

ThreadSafeGuard::~ThreadSafeGuard()
{
    m_running = false;
}