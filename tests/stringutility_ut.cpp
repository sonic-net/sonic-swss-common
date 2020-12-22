#include "common/stringutility.h"
#include "common/ipaddress.h"

#include "gtest/gtest.h"

#include <array>
#include <string>

TEST(STRINGUTILITY, split_int)
{
    int i;

    EXPECT_TRUE(swss::split("123", ':', i));
    EXPECT_EQ(i, 123);

    EXPECT_TRUE(swss::split("0", ':', i));
    EXPECT_EQ(i, 0);

    EXPECT_TRUE(swss::split("-123", ':', i));
    EXPECT_EQ(i, -123);

    EXPECT_FALSE(swss::split("123:", ':', i));
}

TEST(STRINGUTILITY, split_bool)
{
    bool b;

    EXPECT_TRUE(swss::split("true", ':', b));
    EXPECT_EQ(b, true);

    EXPECT_TRUE(swss::split("TRUE", ':', b));
    EXPECT_EQ(b, true);

    EXPECT_TRUE(swss::split("1", ':', b));
    EXPECT_EQ(b, true);

    EXPECT_TRUE(swss::split("false", ':', b));
    EXPECT_EQ(b, false);

    EXPECT_TRUE(swss::split("0", ':', b));
    EXPECT_EQ(b, false);

    EXPECT_THROW(swss::split("123", ':', b), std::invalid_argument);
}

TEST(STRINGUTILITY, split_mix)
{
    int i;
    bool b;
    std::string s;

    EXPECT_TRUE(swss::split("123:true:name", ':', i, b, s));
    EXPECT_EQ(i, 123);
    EXPECT_EQ(b, true);
    EXPECT_EQ("name", s);
}

TEST(STRINGUTILITY, join)
{
    auto str = swss::join(':', 123, true, std::string("name"));
    EXPECT_EQ(str, "123:1:name");
}

TEST(STRINGUTILITY, hex_to_binary)
{
    std::array<std::uint8_t, 5> a;

    EXPECT_TRUE(swss::hex_to_binary("01020aff05", a.data(), a.size()));
    EXPECT_EQ(a, (std::array<std::uint8_t, 5>{0x1, 0x2, 0x0a, 0xff, 0x5}));

    EXPECT_TRUE(swss::hex_to_binary("01020AFF05", a.data(), a.size()));
    EXPECT_EQ(a, (std::array<std::uint8_t, 5>{0x1, 0x2, 0x0A, 0xFF, 0x5}));

    EXPECT_FALSE(swss::hex_to_binary("01020", a.data(), a.size()));
    EXPECT_FALSE(swss::hex_to_binary("xxxx", a.data(), a.size()));
}

TEST(STRINGUTILITY, binary_to_hex)
{
    std::array<std::uint8_t, 5> a{0x1, 0x2, 0x0a, 0xff, 0x5};
    EXPECT_EQ(swss::binary_to_hex(a.data(), a.size()), "01020AFF05");
}
