#include "common/exec.h"
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
    EXPECT_TRUE(ret == 0);

    string result2;
    ret = exec(cmd2, result2);
    EXPECT_TRUE(ret == 0);

    string result3;
    ret = exec(cmd3, result3);
    EXPECT_TRUE(ret == 0);

    EXPECT_EQ(result1, result2);
    EXPECT_EQ(result2, result3);
}

TEST(EXEC, error)
{
    string cmd = "nproc --aaall > /dev/null 2>&1";

    string result;
    int ret = exec(cmd, result);
    EXPECT_FALSE(ret == 0);
}