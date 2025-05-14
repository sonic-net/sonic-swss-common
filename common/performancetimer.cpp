#include "performancetimer.h"

#include "logger.h"
#include <nlohmann/json.hpp>
#include <fstream>

using namespace swss;

bool PerformanceTimer::m_enable = true;
#define LIMIT 5
#define INDICATOR "/var/log/syslog_notice_flag"

PerformanceTimer::PerformanceTimer(
        _In_ std::string funcName,
        _In_ uint64_t threshold,
        _In_ bool verbose):
        m_name(funcName),
        m_threshold(threshold),
        m_verbose(verbose)
{
    reset();
    m_stop = std::chrono::steady_clock::now();
}

void PerformanceTimer::reset()
{
    SWSS_LOG_ENTER();

    m_tasks = 0;
    m_calls = 0;
    m_busy = 0;
    m_idle = 0;

    m_intervals.clear();
    m_gaps.clear();
    m_incs.clear();
}

void PerformanceTimer::start()
{
    SWSS_LOG_ENTER();

    m_start = std::chrono::steady_clock::now();
    // measures the gap between this start() and the last stop()
    m_gaps.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(m_start-m_stop).count());
}

void PerformanceTimer::stop()
{
    SWSS_LOG_ENTER();
    m_stop = std::chrono::steady_clock::now();
}

std::string PerformanceTimer::inc(uint64_t count)
{
    SWSS_LOG_ENTER();

    std::string output = "";

    m_calls += 1;

    m_tasks += count;

    m_idle += m_gaps.back();

    uint64_t interval = std::chrono::duration_cast<std::chrono::nanoseconds>(m_stop - m_start).count();

    m_busy += interval;

    if (count == 0) {
        m_gaps.pop_back();
        m_calls -= 1;
        return output;
    }

    if (m_incs.size() <= LIMIT) {
        m_incs.push_back(count);
        m_intervals.push_back(interval/1000000);
    } else {
        m_gaps.pop_back();
    }

    if (m_tasks >= m_threshold)
    {
        uint64_t mseconds = m_busy/1000000;

        if (m_enable && mseconds > 0)
        {
            output = getTimerState();
            std::ifstream indicator(INDICATOR);
            if (indicator.good()) {
                SWSS_LOG_NOTICE("%s", output.c_str());
            } else {
                SWSS_LOG_INFO("%s", output.c_str());
            }
        }

        reset();
    }

    return output;
}

std::string PerformanceTimer::getTimerState()
{
    nlohmann::json data;
    data["API"] = m_name;
    data["Tasks"] = m_tasks;
    data["busy[ms]"] = m_busy/1000000;
    data["idle[ms]"] = m_idle;
    data["Total[ms]"] = m_busy/1000000 + m_idle;
    double ratio = static_cast<double>(m_tasks) / static_cast<double>(m_busy/1000000 + m_idle);
    data["RPS[k]"] = std::round(ratio * 10.0) / 10.0f;
    if (m_verbose) {
        data["m_intervals"] = m_intervals;
        data["m_gaps"] = m_gaps;
        data["m_incs"] = m_incs;
    }

    return data.dump();
}

void PerformanceTimer::setTimerName(const std::string& funcName) {
    m_name = funcName;
}

void PerformanceTimer::setTimerThreshold(uint64_t threshold) {
    m_threshold = threshold;
}

void PerformanceTimer::setTimerVerbose(bool verbose) {
    m_verbose = verbose;
}
