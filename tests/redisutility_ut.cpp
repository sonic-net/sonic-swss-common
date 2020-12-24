#include "common/redisutility.h"

#include "gtest/gtest.h"

TEST(REDISUTILITY, fvtGetValue)
{
    std::vector<swss::FieldValueTuple> fvt;
    fvt.push_back(std::make_pair("int", "123"));
    fvt.push_back(std::make_pair("bool", "true"));
    fvt.push_back(std::make_pair("string", "name"));

    auto si = swss::fvtGetValue(fvt, "int");
    EXPECT_TRUE(si);
    auto sb = swss::fvtGetValue(fvt, "bool");
    EXPECT_TRUE(sb);
    auto ss = swss::fvtGetValue(fvt, "string");
    EXPECT_TRUE(ss);

    int i;
    bool b;
    std::string s;
    ASSERT_NO_THROW(swss::cast({si.get(), sb.get(), ss.get()}, i, b, s));
    EXPECT_EQ(i, 123);
    EXPECT_EQ(b, true);
    EXPECT_EQ(s, "name");

    EXPECT_FALSE(swss::fvtGetValue(fvt, "double"));
}
