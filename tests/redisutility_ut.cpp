#include "common/redisutility.h"

#include "gtest/gtest.h"

TEST(REDISUTILITY, fvGetValue)
{
    std::vector<swss::FieldValueTuple> fvt;
    fvt.push_back(std::make_pair("int", "123"));
    fvt.push_back(std::make_pair("bool", "true"));
    fvt.push_back(std::make_pair("string", "name"));

    int i;
    EXPECT_TRUE(swss::fvGetValue(fvt, "int", i));
    EXPECT_EQ(i, 123);

    bool b;
    EXPECT_TRUE(swss::fvGetValue(fvt, "bool", b));
    EXPECT_EQ(b, true);

    std::string s;
    EXPECT_TRUE(swss::fvGetValue(fvt, "string", s));
    EXPECT_EQ(s, "name");

    double d;
    EXPECT_FALSE(swss::fvGetValue(fvt, "double", d));
}
