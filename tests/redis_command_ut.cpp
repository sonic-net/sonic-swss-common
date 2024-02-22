#include "gtest/gtest.h"
#include "common/rediscommand.h"

TEST(RedisCommand, invalid_redis_command)
{
    swss::RedisCommand cmd;
    EXPECT_THROW(cmd.format("Invalid redis command %l^", 1), std::invalid_argument);
    EXPECT_EQ(cmd.c_str(), nullptr);
    EXPECT_EQ(cmd.length(), 0);
    EXPECT_EQ(cmd.len, 0);
    EXPECT_EQ(cmd.temp, nullptr);
}
