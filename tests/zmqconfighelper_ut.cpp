#include <iostream>
#include "gtest/gtest.h"
#include "common/zmqconfighelper.h"

using namespace std;
using namespace swss;

static const string testAppName = "TestApp";
static const string testDockerName = "TestDocker";

TEST(ZmqConfigHelper, configNotExist)
{
    bool exception_happen = false;
    try
    {
        ZmqConfigHelper::GetSocketPort("TEST_DB", "TEST_TABLE", "NotExistFile");
    }
    catch (const exception& e)
    {
        exception_happen = true;
    }

    EXPECT_EQ(true, exception_happen);
}

TEST(ZmqConfigHelper, getSocketPort)
{
    auto port = ZmqConfigHelper::GetSocketPort("TEST_DB", "TEST_TABLE", "./tests/zmqconfig/zmp_port_mapping.json");
    EXPECT_EQ(1234, port);
}


TEST(ZmqConfigHelper, getSocketPortFailed)
{
    bool exception_happen = false;
    try
    {
        ZmqConfigHelper::GetSocketPort("TEST_DB", "TEST_TABLE_NO_CONFIG", "./tests/zmqconfig/zmp_port_mapping.json");
    }
    catch (const exception& e)
    {
        exception_happen = true;
    }

    EXPECT_EQ(true, exception_happen);
}
