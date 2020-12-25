#include "common/stringutility.h"
#include "common/ipaddress.h"
#include "common/schema.h"

#include "gtest/gtest.h"

#include <array>
#include <string>

TEST(STRINGUTILITY, cast_int)
{
    int i;

    EXPECT_NO_THROW(swss::lexical_convert("123", i));
    EXPECT_EQ(i, 123);

    EXPECT_NO_THROW(swss::lexical_convert("0", i));
    EXPECT_EQ(i, 0);

    EXPECT_NO_THROW(swss::lexical_convert("-123", i));
    EXPECT_EQ(i, -123);

    EXPECT_THROW(swss::lexical_convert("123:", i), boost::bad_lexical_cast);
}

TEST(STRINGUTILITY, cast_bool)
{
    bool b;

    EXPECT_NO_THROW(swss::lexical_convert(TRUE_STRING, b));
    EXPECT_EQ(b, true);

    EXPECT_NO_THROW(swss::lexical_convert("1", b));
    EXPECT_EQ(b, true);

    EXPECT_NO_THROW(swss::lexical_convert(FALSE_STRING, b));
    EXPECT_EQ(b, false);

    EXPECT_NO_THROW(swss::lexical_convert("0", b));
    EXPECT_EQ(b, false);

    ASSERT_NE("True", TRUE_STRING);
    EXPECT_THROW(swss::lexical_convert("True", b), boost::bad_lexical_cast);

    EXPECT_THROW(swss::lexical_convert("abcdefg", b), boost::bad_lexical_cast);
}

TEST(STRINGUTILITY, cast_mix)
{
    int i;
    bool b;
    std::string s;

    EXPECT_NO_THROW(swss::lexical_convert(swss::tokenize("123:" TRUE_STRING ":name", ':'), i, b, s));
    EXPECT_EQ(i, 123);
    EXPECT_EQ(b, true);
    EXPECT_EQ("name", s);

    EXPECT_NO_THROW(swss::lexical_convert({"123", TRUE_STRING, "name"}, i, b, s));
    EXPECT_EQ(i, 123);
    EXPECT_EQ(b, true);
    EXPECT_EQ("name", s);

    std::vector<std::string> attrs{"123", TRUE_STRING, "name"};
    EXPECT_NO_THROW(swss::lexical_convert(attrs, i, b, s));

    EXPECT_THROW(swss::lexical_convert({"123"}, i, b), std::runtime_error);
    EXPECT_THROW(swss::lexical_convert(attrs, i, i, i), boost::bad_lexical_cast);
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
    EXPECT_FALSE(swss::hex_to_binary("0101010101010101", a.data(), a.size()));
    EXPECT_FALSE(swss::hex_to_binary("xxxx", a.data(), a.size()));
}

TEST(STRINGUTILITY, binary_to_hex)
{
    std::array<std::uint8_t, 5> a{0x1, 0x2, 0x0a, 0xff, 0x5};
    EXPECT_EQ(swss::binary_to_hex(a.data(), a.size()), "01020AFF05");

    EXPECT_EQ(swss::binary_to_hex(nullptr, 0), "");
}
