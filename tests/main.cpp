#include "gtest/gtest.h"

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    //run multiDB test first, which init the db config
    ::testing::GTEST_FLAG(filter) = "DBConnector.multi_db_test";
    EXPECT_EQ(RUN_ALL_TESTS(), 0);
    //run all other test cases
    ::testing::GTEST_FLAG(filter) = "*.*-DBConnector.multi_db_test";
    return RUN_ALL_TESTS();
}
