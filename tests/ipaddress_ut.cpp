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

TEST(IpAddress, isZero)
{
    IpAddress ip1 = IpAddress();
    IpAddress ip2 = IpAddress(0);
    IpAddress ip3 = IpAddress("0.0.0.0");
    IpAddress ip4 = IpAddress("::");
    IpAddress ip5 = IpAddress("192.168.0.0");
    IpAddress ip6 = IpAddress("fe00::1");

    EXPECT_FALSE(ip1.isZero());
    EXPECT_TRUE(ip2.isZero());
    EXPECT_TRUE(ip3.isZero());
    EXPECT_TRUE(ip4.isZero());
    EXPECT_FALSE(ip5.isZero());
    EXPECT_FALSE(ip6.isZero());
}

TEST(IpAddress, getAddrScope)
{
    // IPv4 prefixes
    IpAddress ip1("0.0.0.0");
    IpAddress ip2("169.1.1.1");
    IpAddress ip3("169.254.0.1");
    IpAddress ip4("169.254.255.255");
    IpAddress ip5("169.253.1.1");
    IpAddress ip6("169.255.1.1");
    IpAddress ip7("127.0.0.1");
    IpAddress ip8("224.0.0.1");
    IpAddress ip9("239.255.255.255");

    // IPv6 prefixes
    IpAddress ip11("2001:4898:f0:f153:357c:77b2:49c9:627c");
    IpAddress ip12("fe80::1");
    IpAddress ip13("fe80:1:1:1:1:1:1:1");
    IpAddress ip14("fe99::1");
    IpAddress ip15("feb0:1::1");
    IpAddress ip16("febf:1::1");
    IpAddress ip17("fec0:1::1");
    IpAddress ip18("::1");
    IpAddress ip19("ff00::1");
    IpAddress ip20("ffff:1::1");

    EXPECT_EQ(IpAddress::AddrScope::GLOBAL_SCOPE, ip1.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::GLOBAL_SCOPE, ip2.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::LINK_SCOPE,   ip3.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::LINK_SCOPE,   ip4.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::GLOBAL_SCOPE, ip5.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::GLOBAL_SCOPE, ip6.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::HOST_SCOPE,   ip7.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::MCAST_SCOPE,  ip8.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::MCAST_SCOPE,  ip9.getAddrScope());

    EXPECT_EQ(IpAddress::AddrScope::GLOBAL_SCOPE, ip11.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::LINK_SCOPE,   ip12.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::LINK_SCOPE,   ip13.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::LINK_SCOPE,   ip14.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::LINK_SCOPE,   ip15.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::LINK_SCOPE,   ip16.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::GLOBAL_SCOPE, ip17.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::HOST_SCOPE,   ip18.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::MCAST_SCOPE,  ip19.getAddrScope());
    EXPECT_EQ(IpAddress::AddrScope::MCAST_SCOPE,  ip20.getAddrScope());
}
