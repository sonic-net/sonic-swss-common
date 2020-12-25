#include "common/boolean.h"

#include "gtest/gtest.h"

#include <sstream>

TEST(BOOLEAN, alpha_boolean)
{
    swss::AlphaBoolean bt(true);
    swss::AlphaBoolean bf(false);

    EXPECT_TRUE(bt);
    EXPECT_FALSE(bf);

    std::ostringstream ost;
    ost << bt;
    EXPECT_EQ(ost.str(), "true");

    std::ostringstream osf;
    osf << bf;
    EXPECT_EQ(osf.str(), "false");

    std::istringstream is;

    bt = false;
    is.str("true");
    is >> bt;
    EXPECT_TRUE(bt);
    EXPECT_FALSE(is.fail());
    is.clear();

    bt = false;
    is.str("true123");
    int i;
    is >> bt >> i;
    EXPECT_TRUE(bt);
    EXPECT_EQ(i, 123);
    EXPECT_FALSE(is.fail());
    is.clear();

    bf = true;
    is.str("false");
    is >> bf;
    EXPECT_FALSE(bf);
    EXPECT_FALSE(is.fail());
    is.clear();

    bf = true;
    is.str("abdef");
    EXPECT_FALSE(is >> bt);
    EXPECT_TRUE(is.fail());
    is.clear();
}
