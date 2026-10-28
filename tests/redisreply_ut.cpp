#include "common/redisreply.h"

#include "gtest/gtest.h"

#include <cstdlib>
#include <system_error>

TEST(RedisReply, check_status_handles_null_status_string)
{
    auto reply = static_cast<redisReply *>(std::calloc(1, sizeof(redisReply)));
    ASSERT_NE(reply, nullptr);

    reply->type = REDIS_REPLY_STATUS;
    reply->str = nullptr;

    swss::RedisReply statusReply(reply);

    EXPECT_THROW(statusReply.checkStatusOK(), std::system_error);
}
