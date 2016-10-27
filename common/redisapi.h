#pragma once
#include <stdexcept>
#include "common/logger.h"
#include "common/rediscommand.h"

namespace swss {

static inline std::string loadRedisScript(DBConnector* db, const std::string& script)
{
    SWSS_LOG_ENTER();

    RedisCommand loadcmd;
    loadcmd.format("SCRIPT LOAD %s", script.c_str());
    RedisReply r(db, loadcmd, REDIS_REPLY_STRING);
    return r.getReply<std::string>();
}

}
