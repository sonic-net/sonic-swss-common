#include "common/redisutility.h"
#include "common/stringutility.h"
#include "common/boolean.h"

#include "gtest/gtest.h"

TEST(REDISUTILITY, fvsGetValue)
{
    std::vector<swss::FieldValueTuple> fvt;
    fvt.push_back(std::make_pair("int", "123"));
    fvt.push_back(std::make_pair("bool", "true"));
    fvt.push_back(std::make_pair("string", "name"));

    auto si = swss::fvsGetValue(fvt, "int");
    EXPECT_TRUE(si);
    auto sb = swss::fvsGetValue(fvt, "bool");
    EXPECT_TRUE(sb);
    auto ss = swss::fvsGetValue(fvt, "string");
    EXPECT_TRUE(ss);

    int i;
    swss::AlphaBoolean b;
    std::string s;
    ASSERT_NO_THROW(swss::lexical_convert({*si, *sb, *ss}, i, b, s));
    EXPECT_EQ(i, 123);
    EXPECT_EQ(b, true);
    EXPECT_EQ(s, "name");

    EXPECT_FALSE(swss::fvsGetValue(fvt, "Int"));
    EXPECT_TRUE(swss::fvsGetValue(fvt, "Int", true));
    EXPECT_FALSE(swss::fvsGetValue(fvt, "double"));
}
