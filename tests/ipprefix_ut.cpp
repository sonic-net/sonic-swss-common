#include <gtest/gtest.h>
#include <string>
#include <iostream>
#include <stdexcept>
#include "common/ipprefix.h"

using namespace std;
using namespace swss;

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
