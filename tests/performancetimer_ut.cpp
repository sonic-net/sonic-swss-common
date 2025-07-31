#include "common/performancetimer.h"
#include <nlohmann/json.hpp>
#include "gtest/gtest.h"
#include <thread>

using namespace std;

#define PRINT_ALL 1

TEST(PerformancetimerTest, basic)
{
    static swss::PerformanceTimer timer("basic", PRINT_ALL);
    
    // First measurement
    timer.start();
    this_thread::sleep_for(chrono::milliseconds(100));
    timer.stop();
    std::string output = timer.inc(1000);

    auto actualJson = nlohmann::json::parse(output);
    EXPECT_EQ(actualJson["API"], "basic");
    EXPECT_EQ(actualJson["Tasks"], 1000);
    EXPECT_NEAR(actualJson["RPS[k]"].get<double>(), 10.0, 0.1);
    EXPECT_NEAR(static_cast<double>(actualJson["busy[ms]"].get<uint64_t>()), 100.0, 10.0);
    EXPECT_LE(actualJson["idle[ms]"].get<uint64_t>(), 10u);

    // Configuration changes
    timer.setTimerName("basic_set_name");
    timer.setTimerVerbose(true);
    timer.setTimerThreshold(3000);

    // Second measurement
    timer.start();
    this_thread::sleep_for(chrono::milliseconds(100));
    timer.stop();
    output = timer.inc(1000);
    EXPECT_EQ(output, "");

    // Third measurement
    this_thread::sleep_for(chrono::milliseconds(200));
    timer.start();
    this_thread::sleep_for(chrono::milliseconds(300));
    timer.stop();
    output = timer.inc(2000);

    actualJson = nlohmann::json::parse(output);
    EXPECT_EQ(actualJson["API"], "basic_set_name");
    EXPECT_EQ(actualJson["Tasks"], 3000);
    EXPECT_NEAR(actualJson["RPS[k]"].get<double>(), 5.0, 0.1);
    EXPECT_NEAR(static_cast<double>(actualJson["busy[ms]"].get<uint64_t>()), 400.0, 20.0);
    EXPECT_NEAR(static_cast<double>(actualJson["idle[ms]"].get<uint64_t>()), 200.0, 20.0);
    EXPECT_EQ(actualJson["m_incs"].get<std::vector<uint64_t>>(), std::vector<uint64_t>({1000, 2000}));
    EXPECT_EQ(actualJson["m_intervals"].get<std::vector<uint64_t>>(), std::vector<uint64_t>({100, 300}));
}