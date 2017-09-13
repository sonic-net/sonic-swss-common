#include <string>
#include <limits>
#include <tuple>
#include <system_error>
#include <hiredis/hiredis.h>
#include "logger.h"
#include "tokenize.h"
#include "dbconnector.h"
#include "table.h"
#include "selectable.h"
#include "redisselect.h"
#include "redisapi.h"
#include "subscriber.h"

namespace swss {

Subscriber::Subscriber(DBConnector *db, std::string tableName)
    : RedisTransactioner(db)
    , TableBase(tableName)
{
    m_keyspace = "__keyspace@";

    m_keyspace += std::to_string(db->getDB()) + "__:" + tableName + ":*";

    psubscribe(m_db, m_keyspace);
    /*
     * Asynchronous keyspace notification is fire and forget.
     * For any existing keys, they should be fetched explicitely.
     * No initial pending notification in the cache.
     */
    setQueueLength(0);
}

void Subscriber::pop(KeyOpFieldsValuesTuple &kco, std::string prefix)
{
    if (m_buffer.empty())
    {
        pops(m_buffer, prefix);
    }

    if (m_buffer.empty())
    {
        auto& values = kfvFieldsValues(kco);
        values.clear();
        kfvKey(kco).clear();
        kfvOp(kco).clear();
        return;
    }

    kco = m_buffer.front();
    m_buffer.pop_front();
}

/*
 * handle redis keyspace pmessage notification.
 * Expecting pmessage with 4 elements like:
 * 0) pmessage
 * 1) __keyspace@4__*
 * 2) __keyspace@4__:CFG_VLAN_TABLE:Vlan3000
 * 3) hset
 */
void Subscriber::pops(std::deque<KeyOpFieldsValuesTuple> &vkco, std::string /*prefix*/)
{
    RedisReplyAsync r(getSubscribeDBC());
    /* Keep fetching the reply object until all of them are read from redisReader buf */
    while(auto ctx0 = r.getContext())
    {
        KeyOpFieldsValuesTuple kco;
        /* if the Key-space notification is empty, try next one. */
        if (ctx0->type == REDIS_REPLY_NIL)
        {
            continue;
        }

        assert(ctx0->type == REDIS_REPLY_ARRAY);
        size_t n = ctx0->elements;

        /* Expecting 4 elements for each keyspace pmessage notification */
        if (n != 4)
        {
            SWSS_LOG_ERROR("invalid number of elements %lu for pmessage of %s", n, m_keyspace.c_str());
            continue;
        }
        /* The second element should be the original pattern matched */
        auto ctx = ctx0->element[1];
        if (m_keyspace.compare(ctx->str))
        {
            SWSS_LOG_ERROR("invalid pattern %s returned for pmessage of %s", ctx->str, m_keyspace.c_str());
            continue;
        }

        ctx = ctx0->element[2];
        std::vector<std::string> keys = tokenize(ctx->str, ':');
        if (keys.size() < 3)
        {
            SWSS_LOG_ERROR("invalid key %s returned for pmessage of %s", ctx->str, m_keyspace.c_str());
            continue;
        }

        std::string key  = keys[2];
        for(unsigned int i=3; i< keys.size(); i++)
        {
            key += ":" + keys[i];
        }
        std::string table_key = keys[1] + ":" + key;

        ctx = ctx0->element[3];
        if (strcmp("del", ctx->str) == 0)
        {
            kfvKey(kco) = key;
            kfvOp(kco) = DEL_COMMAND;
        }
        else
        {
            if (!get(table_key, kfvFieldsValues(kco)))
            {
                SWSS_LOG_ERROR("Failed to get content for table key %s", table_key.c_str());
                continue;
            }
            kfvKey(kco) = key;
            kfvOp(kco) = SET_COMMAND;
        }
        vkco.push_back(kco);
    }
    return;
}

bool Subscriber::get(std::string key, std::vector<FieldValueTuple> &values)
{
    std::string hgetall_key("HGETALL ");

    hgetall_key += key;
    RedisReply r(m_db, hgetall_key, REDIS_REPLY_ARRAY);
    redisReply *reply = r.getContext();
    values.clear();

    if (!reply->elements)
        return false;

    if (reply->elements & 1)
        throw std::system_error(make_error_code(std::errc::result_out_of_range),
                           "Invalid redis reply");

    for (unsigned int i = 0; i < reply->elements; i += 2)
        values.push_back(std::make_tuple(reply->element[i]->str,
                                    reply->element[i + 1]->str));

    return true;
}

}
