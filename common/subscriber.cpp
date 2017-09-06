#include <string>
#include <limits>
#include <tuple>
#include <system_error>
#include <hiredis/hiredis.h>
#include "logger.h"
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
    /* Don't read existing data implicitly here */
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
 * handle redis keyspace pmessage notification
 */
void Subscriber::pops(std::deque<KeyOpFieldsValuesTuple> &vkco, std::string /*prefix*/)
{
    RedisReplyAsync r(getSubscribeDBC());
    // Keep fetching the reply object until all of them are read
    while(auto ctx0 = r.getContext())
    {
        KeyOpFieldsValuesTuple kco;
        // if the Key-space notification is empty, try next one.
        if (ctx0->type == REDIS_REPLY_NIL)
        {
            continue;
        }

        assert(ctx0->type == REDIS_REPLY_ARRAY);
        size_t n = ctx0->elements;

        //Expecting 4 elements for each keyspace pmessage notification
        if(n != 4)
        {
            SWSS_LOG_ERROR("invalid number of elements %lu for pmessage of %s", n, m_keyspace.c_str());
            continue;
        }
        // TODO: more validation and debug log
        auto ctx = ctx0->element[1];
        if(m_keyspace.compare(ctx->str))
        {
            SWSS_LOG_ERROR("invalid pattern %s returned for pmessage of %s", ctx->str, m_keyspace.c_str());
            continue;
        }

        ctx = ctx0->element[2];
        std::string table_key = ctx->str;
        size_t found = table_key.find(':');
        // Return if the format of key is wrong
        if (found == std::string::npos)
        {
            SWSS_LOG_ERROR("invalid keyspace notificaton %s", table_key.c_str());
            continue;
        }
        //strip off  the keyspace db string
        table_key = table_key.substr(found+1);

        found = table_key.find(':');
         // Return if the format of key is wrong
        if (found == std::string::npos)
        {
            SWSS_LOG_ERROR("invalid table key %s", table_key.c_str());
            continue;
        }
        // get the key in subscription table
        std::string key  = table_key.substr(found+1);

        ctx = ctx0->element[3];
        if (strcmp("del", ctx->str) == 0)
        {
            kfvKey(kco) = key;
            kfvOp(kco) = DEL_COMMAND;
        }
        else
        {
            if(!get(table_key, kfvFieldsValues(kco)))
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
