#include <gtest/gtest.h>
#include <string>
#include <iostream>
#include "ipprefix.h"

using namespace std;
using namespace swss;

TEST(sample_test_case, sample_test)
{
    IpAddress ip("2001:4898:f0:f153:357c:77b2:49c9:627c");
    EXPECT_EQ(ip.to_string(), "2001:4898:f0:f153:357c:77b2:49c9:627c");
    EXPECT_TRUE(!ip.isV4());
    
    IpPrefix ipp64("2001:4898:f0:f153:357c:77b2:49c9:627c/64");
    EXPECT_EQ(ipp64.getIp().to_string(), "2001:4898:f0:f153:357c:77b2:49c9:627c");
    EXPECT_EQ(ipp64.getMask().to_string(), "ffff:ffff:ffff:ffff::");
    
    IpPrefix ipp63("2001:4898:f0:f153:357c:77b2:49c9:627c/63");
    EXPECT_EQ(ipp63.getIp().to_string(), "2001:4898:f0:f153:357c:77b2:49c9:627c");
    EXPECT_EQ(ipp63.getMask().to_string(), "ffff:ffff:ffff:fffe::");
    
    IpPrefix ipp65("2001:4898:f0:f153:357c:77b2:49c9:627c/65");
    EXPECT_EQ(ipp65.getIp().to_string(), "2001:4898:f0:f153:357c:77b2:49c9:627c");
    EXPECT_EQ(ipp65.getMask().to_string(), "ffff:ffff:ffff:ffff:8000::");
}
