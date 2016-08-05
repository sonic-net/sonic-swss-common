#include "common/ipaddress.h"
#include "common/ipaddresses.h"
#include "common/tokenize.h"

#include "gtest/gtest.h"

using namespace std;
using namespace swss;

TEST(TOKENIZE, basic)
{
    string str1 = "str1";
    string str2 = "str2";

    vector<string> origin;
    vector<string> result;

    origin.push_back(str1);
    result = tokenize("str1", ':');

    // Test1: check tokenize works on single token
    EXPECT_EQ(origin, result);

    origin.push_back(str2);
    result = tokenize("str1:str2", ':');

    // Test2: check tokenize works on multiple tokens
    EXPECT_EQ(origin, result);
}

TEST(TOKENIZE, IP)
{
    IpAddresses origin("192.168.0.1,192.168.0.2");

    IpAddress ip1("192.168.0.1");
    IpAddress ip2("192.168.0.2");

    IpAddresses result;
    result.add(ip1);
    result.add(ip2);

    EXPECT_EQ(origin, result);
}

TEST(TOKENIZE, IP_2)
{
    IpAddresses origin("192.168.0.1");

    IpAddress ip("192.168.0.1");

    IpAddresses result;
    result.add(ip);

    EXPECT_EQ(origin, result);
}
