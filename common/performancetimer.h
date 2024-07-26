#pragma once

#include "sal.h"
#include <cstdint>

#include <iostream>
#include <chrono>
#include <string>
#include <deque>
namespace swss
{
    class PerformanceTimer
    {
        public:

            PerformanceTimer(
                _In_ std::string funcName = "",
                _In_ uint64_t threshold = 10000,
                _In_ bool verbose = false
            );

            ~PerformanceTimer() = default;

        public:

            void start();

            void stop();

            std::string inc(uint64_t count = 1);

            void reset();

            std::string getTimerState();

            static bool m_enable;

            void setTimerName(const std::string& funcName);
            void setTimerThreshold(uint64_t threshold);
            void setTimerVerbose(bool verbose);

        private:

            std::string m_name; // records what this timer measures about
            uint64_t m_threshold; // reset the timer when the m_tasks reachs m_threshold
            bool m_verbose; // decides whether to print in verbose when m_threshold is reached

            std::chrono::time_point<std::chrono::steady_clock> m_start;
            std::chrono::time_point<std::chrono::steady_clock> m_stop;

            /* records how long the timer has idled between last stop and this start */
            std::deque<uint64_t> m_gaps;
            /* records how long each call takes */
            std::deque<uint64_t> m_intervals;
            /* records how many tasks each call finishes */
            std::deque<uint64_t> m_incs;

            uint64_t m_tasks; // sum of m_incs
            uint64_t m_calls; // how many times the timer is used
            uint64_t m_busy;  // sum of m_intervals
            uint64_t m_idle;  // sum of m_gaps
    };
}
