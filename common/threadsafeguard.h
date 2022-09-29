#pragma once

namespace swss {

class ThreadSafeGuard
{
public:
    ThreadSafeGuard(std::atomic<bool> &running, const std::string &info);
    ~ThreadSafeGuard();

private:
    std::atomic<bool> &m_running;
};

}
