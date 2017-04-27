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
