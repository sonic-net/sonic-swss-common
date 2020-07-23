#include "common/exec.h"
#include "common/logger.h"
#include "gtest/gtest.h"

using namespace std;
using namespace swss;

TEST(EXEC, result)
{
    string cmd1 = "cat /proc/cpuinfo  | grep processor | wc -l";
    string cmd2 = "nproc --all";
    string cmd3 = "lscpu | egrep '^CPU\\(' | rev | cut -d' ' -f1 | rev";

    string result1;
    int ret = exec(cmd1, result1);
    EXPECT_EQ(ret, 0);

    string result2;
    ret = exec(cmd2, result2);
    EXPECT_EQ(ret, 0);

    string result3;
    ret = exec(cmd3, result3);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(result1, result2);
    EXPECT_EQ(result2, result3);
}

TEST(EXEC, error)
{
    Logger::setMinPrio(Logger::SWSS_DEBUG);
    string cmd = "nproc --aaall > /dev/null 2>&1";

    string result;
    int ret = exec(cmd, result);
    EXPECT_NE(ret, 0);

    ret = exec("exit 0", result);
    EXPECT_EQ(ret, 0);
    ret = exec("exit 132", result);
    EXPECT_EQ(ret, 132);
    ret = exec("exit 255", result);
    EXPECT_EQ(ret, 255);
    ret = exec("exit 254", result);
    EXPECT_EQ(ret, 254);
    Logger::setMinPrio(Logger::SWSS_NOTICE);
}
