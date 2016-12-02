#include <gtest/gtest.h>
#include "common/ipprefix.h"

using namespace std;
using namespace swss;

TEST(IpPrefix, ipv4)
{
    IpPrefix ip1("1.1.1.1/0");
    IpAddress mask1("0.0.0.0");
    EXPECT_EQ(ip1.getMask(), mask1);

    IpPrefix ip2("1.1.1.1/32");
    IpAddress mask2("255.255.255.255");
    EXPECT_EQ(ip2.getMask(), mask2);
}

TEST(IpPrefix, ipv6)
{
    IpAddress ip("2001:4898:f0:f153:357c:77b2:49c9:627c");
    EXPECT_EQ(ip.to_string(), "2001:4898:f0:f153:357c:77b2:49c9:627c");
    EXPECT_TRUE(!ip.isV4());
    
    IpPrefix ipp0("2001:4898:f0:f153:357c:77b2:49c9:627c/0");
    EXPECT_EQ(ipp0.getIp().to_string(), "2001:4898:f0:f153:357c:77b2:49c9:627c");
    EXPECT_EQ(ipp0.getMask().to_string(), "::");
    
    IpPrefix ipp1("2001:4898:f0:f153:357c:77b2:49c9:627c/1");
    EXPECT_EQ(ipp1.getIp().to_string(), "2001:4898:f0:f153:357c:77b2:49c9:627c");
    EXPECT_EQ(ipp1.getMask().to_string(), "8000::");
    
    IpPrefix ipp63("2001:4898:f0:f153:357c:77b2:49c9:627c/63");
    EXPECT_EQ(ipp63.getIp().to_string(), "2001:4898:f0:f153:357c:77b2:49c9:627c");
    EXPECT_EQ(ipp63.getMask().to_string(), "ffff:ffff:ffff:fffe::");
    
    IpPrefix ipp64("2001:4898:f0:f153:357c:77b2:49c9:627c/64");
    EXPECT_EQ(ipp64.getIp().to_string(), "2001:4898:f0:f153:357c:77b2:49c9:627c");
    EXPECT_EQ(ipp64.getMask().to_string(), "ffff:ffff:ffff:ffff::");
    
    IpPrefix ipp65("2001:4898:f0:f153:357c:77b2:49c9:627c/65");
    EXPECT_EQ(ipp65.getIp().to_string(), "2001:4898:f0:f153:357c:77b2:49c9:627c");
    EXPECT_EQ(ipp65.getMask().to_string(), "ffff:ffff:ffff:ffff:8000::");
    
    IpPrefix ipp127("2001:4898:f0:f153:357c:77b2:49c9:627c/127");
    EXPECT_EQ(ipp127.getIp().to_string(), "2001:4898:f0:f153:357c:77b2:49c9:627c");
    EXPECT_EQ(ipp127.getMask().to_string(), "ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffe");
    
    IpPrefix ipp128("2001:4898:f0:f153:357c:77b2:49c9:627c/128");
    EXPECT_EQ(ipp128.getIp().to_string(), "2001:4898:f0:f153:357c:77b2:49c9:627c");
    EXPECT_EQ(ipp128.getMask().to_string(), "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
    
    EXPECT_THROW(IpPrefix("2001:4898:f0:f153:357c:77b2:49c9:627c/-1"), invalid_argument);
    EXPECT_THROW(IpPrefix("2001:4898:f0:f153:357c:77b2:49c9:627c/-2"), invalid_argument);
    EXPECT_THROW(IpPrefix("2001:4898:f0:f153:357c:77b2:49c9:627c/129"), invalid_argument);
}

TEST(IpPrefix, compare)
{
    IpPrefix ip1("0.0.0.0/0");
    IpPrefix ip2("::/0");

    EXPECT_TRUE(ip1 < ip2);
    EXPECT_TRUE(!(ip2 < ip1));
    EXPECT_TRUE(!(ip1 == ip2));
}

TEST(IpPrefix, subnet)
{
    IpPrefix prefix1("0.0.0.0/0");
    IpPrefix prefix2("2.2.2.0/24");
    IpPrefix prefix3("::/0");
    IpPrefix prefix4("2001:4898:f0:f153:357c:77b2:49c9:627c/64");
    IpPrefix prefix5("3.3.3.3/32");
    IpPrefix prefix6("2001:4898:f0:f153:357c:77b2:49c9:627c/128");

    IpAddress ip1("1.1.1.1");
    IpAddress ip2("2.2.2.2");
    IpAddress ip3("4002:4898:f0:f153:357c:77b2:0:627c");
    IpAddress ip4("2001:4898:f0:f153:357c:77b2:0:627c");
    IpAddress ip5("3.3.3.3");
    IpAddress ip6("3.3.3.4");
    IpAddress ip7("2001:4898:f0:f153:357c:77b2:49c9:627c");
    IpAddress ip8("2001:4898:f0:f153:357c:77b2:49c9:627d");

    // IPv4 address in IPv4 subnet
    EXPECT_TRUE(prefix1.isAddressInSubnet(ip1));
    EXPECT_FALSE(prefix2.isAddressInSubnet(ip1));
    EXPECT_TRUE(prefix2.isAddressInSubnet(ip2));
    EXPECT_TRUE(prefix5.isAddressInSubnet(ip5));
    EXPECT_FALSE(prefix5.isAddressInSubnet(ip6));

    // IPv6 address in IPv6 subnet
    EXPECT_TRUE(prefix3.isAddressInSubnet(ip3));
    EXPECT_FALSE(prefix4.isAddressInSubnet(ip3));
    EXPECT_TRUE(prefix4.isAddressInSubnet(ip4));
    EXPECT_TRUE(prefix6.isAddressInSubnet(ip7));
    EXPECT_FALSE(prefix6.isAddressInSubnet(ip8));

    // IPv4 address in IPv6 subnet
    EXPECT_FALSE(prefix3.isAddressInSubnet(ip2));
    EXPECT_FALSE(prefix4.isAddressInSubnet(ip2));

    // IPv6 address in IPv4 subnet
    EXPECT_FALSE(prefix1.isAddressInSubnet(ip3));
    EXPECT_FALSE(prefix2.isAddressInSubnet(ip3));
}
