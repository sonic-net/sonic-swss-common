#include <gtest/gtest.h>
#include "common/ipaddresses.h"

using namespace std;
using namespace swss;

TEST(IpAddresses, empty)
{
    IpAddresses ips("");
    EXPECT_EQ((size_t)0, ips.getSize());
    EXPECT_EQ("", ips.to_string());
}

TEST(IpAddresses, contains)
{
    IpAddresses ips("1.1.1.1,2.2.2.2,3.3.3.3");

    string str_1("1.1.1.1");
    string str_2("4.4.4.4");

    EXPECT_TRUE(ips.contains(str_1));
    EXPECT_FALSE(ips.contains(str_2));

    IpAddress ip_1("1.1.1.1");
    IpAddress ip_2("4.4.4.4");

    EXPECT_TRUE(ips.contains(ip_1));
    EXPECT_FALSE(ips.contains(ip_2));

    IpAddresses ips_1("1.1.1.1,2.2.2.2");
    IpAddresses ips_2("1.1.1.1,4.4.4.4");

    EXPECT_TRUE(ips.contains(ips_1));
    EXPECT_FALSE(ips.contains(ips_2));
}
