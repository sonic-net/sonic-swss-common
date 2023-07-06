#include "redisstring.h"
#include <hiredis/hiredis.h>

namespace swss {

RedisString::RedisString()
    : m_data(nullptr),
      m_len(0)
{
}

RedisString::~RedisString()
{
    reset();
}

void RedisString::reset(char *str, int len)
{
    if (m_data != nullptr)
    {
        redisFreeCommand(m_data);
    }
    m_data = str;
    m_len = len;
}

const char *RedisString::getStr() const
{
    return m_data;
}

int RedisString::getLen() const
{
    return m_len;
}

}
