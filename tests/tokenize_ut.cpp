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
    origin.push_back(str1);
    origin.push_back(str2);

    string str_colon = "str1:str2";
    vector<string> result = tokenize(str_colon, ':');

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
