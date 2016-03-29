#include <hiredis/hiredis.h>
#include <system_error>
#include <iostream>

#include "common/counter.h"
#include "common/logger.h"
#include "common/redisreply.h"

namespace swss {

Counter::Counter(DBConnector *db, std::string counterName):
    m_db(db),
    m_counterName(counterName)
{
}

int64_t Counter::get()
{
    std::string cmd = formatGet();

    RedisReply r(m_db, cmd, REDIS_REPLY_INTEGER, true);

    if (r.getContext()->type != REDIS_REPLY_INTEGER);
        throw std::runtime_error("invalid counter response");

    return r.getContext()->integer;
}

void Counter::set(int64_t value)
{
    std::string cmd = formatSet(value);

    RedisReply r(m_db, cmd, REDIS_REPLY_INTEGER, true);

    if (r.getContext()->type != REDIS_REPLY_INTEGER)
        throw std::runtime_error("invalid counter response");
}

int64_t Counter::incr()
{
    std::string cmd = formatIncr();

    RedisReply r(m_db, cmd, REDIS_REPLY_INTEGER, true);

    if (r.getContext()->type != REDIS_REPLY_INTEGER)
        throw std::runtime_error("invalid counter response");

    return r.getContext()->integer;
}

int64_t Counter::decr()
{
    std::string cmd = formatDecr();

    RedisReply r(m_db, cmd, REDIS_REPLY_INTEGER, true);

    if (r.getContext()->type != REDIS_REPLY_INTEGER)
        throw std::runtime_error("invalid counter response");

    return r.getContext()->integer;
}

std::string Counter::formatGet()
{
        char *temp;
        int len = redisFormatCommand(&temp, "GET %s", m_counterName.c_str());

        std::string formated(temp, len);
        free(temp);
        return formated;
}

std::string Counter::formatSet(int64_t value)
{
        char *temp;
        int len = redisFormatCommand(&temp, "SET %s %s", m_counterName.c_str(), std::to_string(value).c_str());

        std::string formated(temp, len);
        free(temp);
        return formated;
}

std::string Counter::formatIncr()
{
        char *temp;
        int len = redisFormatCommand(&temp, "INCR %s", m_counterName.c_str());

        std::string formated(temp, len);
        free(temp);
        return formated;
}

std::string Counter::formatDecr()
{
        char *temp;
        int len = redisFormatCommand(&temp, "DECR %s", m_counterName.c_str());

        std::string formated(temp, len);
        free(temp);
        return formated;
}

}
