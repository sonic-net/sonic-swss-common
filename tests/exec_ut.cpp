#include "common/exec.h"
#include "gtest/gtest.h"

using namespace std;
using namespace swss;

TEST(EXEC, empty)
{
    string cmd1 = "cat /proc/cpuinfo  | grep processor | wc -l";
    string cmd2 = "nproc --all";
    string cmd3 = "lscpu | egrep '^CPU\\(' | rev | cut -d' ' -f1 | rev";

    string result1 = exec(cmd1.c_str());
    string result2 = exec(cmd2.c_str());
    string result3 = exec(cmd3.c_str());

    EXPECT_EQ(result1, result2);

    EXPECT_EQ(result2, result3);
}
