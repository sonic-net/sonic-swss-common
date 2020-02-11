#include <net/ethernet.h>
#include <gtest/gtest.h>
#include <stdexcept>
#include "common/macaddress.h"

using namespace swss;
using namespace std;

TEST(MacAddress, to_string)
{
    const uint8_t a_bin[] = {0x52, 0x54, 0x00, 0xac, 0x3a, 0x99};
    auto a = MacAddress(a_bin);
    EXPECT_EQ(a.to_string(), "52:54:00:ac:3a:99");
    EXPECT_EQ(MacAddress::to_string(a_bin), "52:54:00:ac:3a:99");

    const uint8_t b_bin[] = {0xa0, 0xf9, 0x0a, 0x9f, 0x43, 0x21};
    auto b = MacAddress(b_bin);
    EXPECT_EQ(b.to_string(), "a0:f9:0a:9f:43:21");
    EXPECT_EQ(MacAddress::to_string(b_bin), "a0:f9:0a:9f:43:21");
}

TEST(MacAddress, comparison)
{
    auto a = MacAddress("52:54:00:ac:3a:99");
    auto b = MacAddress("02:09:87:65:43:21");

    EXPECT_EQ(a, a);
    EXPECT_NE(a, b);
    EXPECT_LT(b, a);

    EXPECT_FALSE(!a);
    EXPECT_FALSE(!b);

    auto c = MacAddress("00:00:00:00:00:00");

    EXPECT_TRUE(!c);
}

TEST(MacAddress, parse)
{
    uint8_t mac[6];

    uint8_t ex_mac[] = {0xa0, 0xf9, 0xaa, 0xff, 0x0a, 0x9f};
    EXPECT_TRUE(MacAddress::parseMacString("A0:F9:aA:fF:0a:9f", mac));   // correct mac address which uses all edge values
    EXPECT_EQ(memcmp(mac, ex_mac, sizeof(mac)), 0);                      // check that we got correct result

    EXPECT_TRUE(MacAddress::parseMacString("A0-F9-aA-fF-0a-9f", mac));   // correct mac address with '-' as delimiter

    EXPECT_FALSE(MacAddress::parseMacString("52:34:51:42:42:42", NULL)); // wrong destination parameter
    EXPECT_FALSE(MacAddress::parseMacString("52:54:00:25::E9",    mac)); // wrong length 1
    EXPECT_FALSE(MacAddress::parseMacString("52:54:00:25:12:E9 ", mac)); // wrong length 2
    EXPECT_FALSE(MacAddress::parseMacString(" 52:54:00:25:12:E",  mac)); // wrong address
    EXPECT_FALSE(MacAddress::parseMacString("52:54:00:25:Z9:E9",  mac)); // wrong hex number 1
    EXPECT_FALSE(MacAddress::parseMacString("52:54:00:25:aZ:E9",  mac)); // wrong hex number 2
    EXPECT_FALSE(MacAddress::parseMacString("52:54:00:25:z9:E9",  mac)); // wrong hex number 3
    EXPECT_FALSE(MacAddress::parseMacString("52:54:00:25:az:E9",  mac)); // wrong hex number 4

    EXPECT_FALSE(MacAddress::parseMacString("52_34:51:42:42:42",  mac)); // wrong delimiter 1
    EXPECT_FALSE(MacAddress::parseMacString("52:34_51:42:42:42",  mac)); // wrong delimiter 2
    EXPECT_FALSE(MacAddress::parseMacString("52:34:51_42:42:42",  mac)); // wrong delimiter 3
    EXPECT_FALSE(MacAddress::parseMacString("52:34:51:42_42:42",  mac)); // wrong delimiter 4
    EXPECT_FALSE(MacAddress::parseMacString("52:34:51:42:42_42",  mac)); // wrong delimiter 5

    EXPECT_THROW(MacAddress("52:54:00:25:E9"), invalid_argument);
}
