#include <gtest/gtest.h>
#include "common/macaddress.h"

using namespace swss;
using namespace std;

TEST(MacAddress, to_string)
{
    auto a = MacAddress("52:54:00:ac:3a:99");
    auto b = MacAddress("02:09:87:65:43:21");

    EXPECT_EQ(a.to_string(), "52:54:00:ac:3a:99");
    EXPECT_EQ(b.to_string(), "02:09:87:65:43:21");
}

TEST(MacAddress, comparison)
{
    auto a = MacAddress("52:54:00:ac:3a:99");
    auto b = MacAddress("02:09:87:65:43:21");

    EXPECT_EQ(a == a, true);
    EXPECT_EQ(a != a, false);

    EXPECT_EQ(a == b, false);
    EXPECT_EQ(a != b, true);

    EXPECT_LT(b, a);

    EXPECT_EQ(a < b, false);
    EXPECT_EQ(b < a, true);

    EXPECT_EQ(!a, false);
    EXPECT_EQ(!b, false);

    auto c = MacAddress("00:00:00:00:00:00");

    EXPECT_EQ(!c, true);
}

TEST(MacAddress, parse)
{
    uint8_t mac[6];
    bool suc = MacAddress::parseMacString("52:54:00:25::E9", mac);
    EXPECT_FALSE(suc);

    EXPECT_THROW(MacAddress("52:54:00:25:E9"), invalid_argument);
}
