#include "common/performancetimer.h"
#include <nlohmann/json.hpp>
#include "gtest/gtest.h"
#include <thread>

using namespace std;

#define PRINT_ALL 1

TEST(PerformancetimerTest, basic)
{
    // Remove: std::string expected; (we'll declare it when needed)
    static swss::PerformanceTimer timer("basic", PRINT_ALL);

    // First measurement block - KEEP
    timer.start();
    this_thread::sleep_for(chrono::milliseconds(100));
    timer.stop();
    std::string output = timer.inc(1000);

    // CHANGE to use JSON comparison instead of string matching
    auto actualJson = nlohmann::json::parse(output);
    EXPECT_EQ(actualJson["API"], "basic");
    EXPECT_EQ(actualJson["Tasks"], 1000);
    EXPECT_NEAR(actualJson["RPS[k]"].get<double>(), 10.0, 0.1);
    EXPECT_NEAR(actualJson["busy[ms]"].get<uint64_t>(), 100, 10);
    EXPECT_LE(actualJson["idle[ms]"].get<uint64_t>(), 10);

    // Configuration changes - KEEP
    timer.setTimerName("basic_set_name");
    timer.setTimerVerbose(true);
    timer.setTimerThreshold(3000);

    // Second measurement - KEEP but remove string comparison
    timer.start();
    this_thread::sleep_for(chrono::milliseconds(100));
    timer.stop();
    output = timer.inc(1000);
    EXPECT_EQ(output, "");  // KEEP (threshold not reached)

    // Third measurement - KEEP
    this_thread::sleep_for(chrono::milliseconds(200));
    timer.start();
    this_thread::sleep_for(chrono::milliseconds(300));
    timer.stop();
    output = timer.inc(2000);

    // CHANGE to use JSON comparison
    actualJson = nlohmann::json::parse(output);
    EXPECT_EQ(actualJson["API"], "basic_set_name");
    EXPECT_EQ(actualJson["Tasks"], 3000);
    EXPECT_NEAR(actualJson["RPS[k]"].get<double>(), 5.0, 0.1);
    EXPECT_NEAR(actualJson["busy[ms]"].get<uint64_t>(), 400, 20);
    EXPECT_NEAR(actualJson["idle[ms]"].get<uint64_t>(), 200, 20);
    EXPECT_EQ(actualJson["m_incs"], std::vector<uint64_t>({1000, 2000}));
    EXPECT_EQ(actualJson["m_intervals"], std::vector<uint64_t>({100, 300}));
}
// #include "common/performancetimer.h"
// #include <nlohmann/json.hpp>
// #include "gtest/gtest.h"
// #include <thread>

// using namespace std;

// #define PRINT_ALL 1

// TEST(PerformancetimerTest, basic)
// {
//     std::string expected;

//     static swss::PerformanceTimer timer("basic", PRINT_ALL);
//     timer.start();
//     this_thread::sleep_for(chrono::milliseconds(100));
//     timer.stop();
//     std::string output = timer.inc(1000);

//     expected = R"({"API":"basic","RPS[k]":10.0,"Tasks":1000,"Total[ms]":100,"busy[ms]":100,"idle[ms]":0})";
//     EXPECT_EQ(output, expected);

//     timer.setTimerName("basic_set_name");
//     timer.setTimerVerbose(true);
//     timer.setTimerThreshold(3000);

//     timer.start();
//     this_thread::sleep_for(chrono::milliseconds(100));
//     timer.stop();
//     output = timer.inc(1000);
//     EXPECT_EQ(output, "");

//     this_thread::sleep_for(chrono::milliseconds(200));

//     timer.start();
//     this_thread::sleep_for(chrono::milliseconds(300));
//     timer.stop();
//     output = timer.inc(2000);

//     expected = R"({\"API\":\"basic_set_name\",\"RPS[k]\":5.0,\"Tasks\":3000,\"Total[ms]\":600,\"busy[ms]\":400,\"idle[ms]\":200,\"m_gaps\":[0,200],\"m_incs\":[1000,2000],\"m_intervals\":[100,300]})";

//     EXPECT_EQ(output, expected);
// }
