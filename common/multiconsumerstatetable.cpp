#include <string>
#include <deque>
#include <limits>
#include <hiredis/hiredis.h>
#include "dbconnector.h"
#include "multiconsumerstatetable.h"
#include "table.h"
#include "selectable.h"
#include "redisselect.h"
#include "redisapi.h"
#include "tokenize.h"

using namespace std;

namespace swss {

MultiConsumerStateTable::MultiConsumerStateTable(DBConnector *db, string tableName)
    : ConsumerTableBase(db, tableName), m_table(db, tableName, CONFIGDB_TABLE_NAME_SEPARATOR)
{
    m_keyspace = "__keyspace@";

    m_keyspace += to_string(db->getDB()) + "__:" + tableName + CONFIGDB_TABLE_NAME_SEPARATOR + "*";

    psubscribe(m_db, m_keyspace);

    vector<string> keys;
    m_table.getTableKeys(keys);

    for (auto key: keys)
    {
        KeyOpFieldsValuesTuple kco;

        kfvKey(kco) = key;
        kfvOp(kco) = SET_COMMAND;

        if (!m_table.get(key, kfvFieldsValues(kco)))
        {
            continue;
        }

        m_buffer.push_back(kco);
    }
}

int MultiConsumerStateTable::readCache()
{
    redisReply *reply = NULL;

    /* If buffers already contain data notify caller about this */
    if (!m_buffer.empty() || !m_keyspace_event_buffer.empty())
    {
        return Selectable::DATA;
    }

    /* Try to read data from redis cacher.
     * If data exists put it to event buffer.
     * NOTE: Keyspace event is not persistent and it won't
     * be possible to read it second time. If it is not stared in
     * the buffer it will be lost. */
    if (redisGetReplyFromReader(m_subscribe->getContext(),
                                (void**)&reply) != REDIS_OK)
    {
        return Selectable::ERROR;
    }
    else if (reply != NULL)
    {
        m_keyspace_event_buffer.push_back(reply);
        return Selectable::DATA;
    }

    return Selectable::NODATA;
}

void MultiConsumerStateTable::readMe()
{
    redisReply *reply = NULL;

    /* Read data from redis. This call is non blocking. This method
     * is called from Select framework when data is available in socket.
     * NOTE: All data should be stored in event buffer. It won't be possible to
     * read them second time. */
    if (redisGetReply(m_subscribe->getContext(), (void**)&reply) != REDIS_OK)
    {
        throw runtime_error("Unable to read redis reply");
    }

    m_keyspace_event_buffer.push_back(reply);
}

void MultiConsumerStateTable::pops(deque<KeyOpFieldsValuesTuple> &vkco, string /*prefix*/)
{
    vkco.clear();

    if (!m_buffer.empty())
    {
        vkco = m_buffer;
        m_buffer.clear();
        return;
    }

    while (auto event = popEventBuffer())
    {
        /* Use RedisReply as auto_ptr to destroy reply object */
        RedisReply r(event);
        KeyOpFieldsValuesTuple kco;
        /* if the Key-space notification is empty, try next one. */
        if (event->type == REDIS_REPLY_NIL)
        {
            continue;
        }

        assert(event->type == REDIS_REPLY_ARRAY);
        size_t n = event->elements;

        /* Expecting 4 elements for each keyspace pmessage notification */
        if (n != 4)
        {
            SWSS_LOG_ERROR("invalid number of elements %lu for pmessage of %s", n, m_keyspace.c_str());
            continue;
        }
        /* The second element should be the original pattern matched */
        auto ctx = event->element[1];
        if (m_keyspace.compare(ctx->str))
        {
            SWSS_LOG_ERROR("invalid pattern %s returned for pmessage of %s", ctx->str, m_keyspace.c_str());
            continue;
        }

        ctx = event->element[2];
        string msg(ctx->str);
        size_t pos = msg.find(":");
        if (pos == msg.npos)
        {
            SWSS_LOG_ERROR("invalid format %s returned for pmessage of %s", ctx->str, m_keyspace.c_str());
            continue;
        }

        string table_entry = msg.substr(pos + 1);
        pos = table_entry.find(CONFIGDB_TABLE_NAME_SEPARATOR);
        if (pos == table_entry.npos)
        {
            SWSS_LOG_ERROR("invalid key %s returned for pmessage of %s", ctx->str, m_keyspace.c_str());
            continue;
        }
        string key = table_entry.substr(pos + 1);

        ctx = event->element[3];
        if (strcmp("del", ctx->str) == 0)
        {
            kfvKey(kco) = key;
            kfvOp(kco) = DEL_COMMAND;
        }
        else
        {
            if (!m_table.get(key, kfvFieldsValues(kco)))
            {
                SWSS_LOG_ERROR("Failed to get content for table key %s", table_entry.c_str());
                continue;
            }
            kfvKey(kco) = key;
            kfvOp(kco) = SET_COMMAND;
        }

        vkco.push_back(kco);
    }

    m_keyspace_event_buffer.clear();

    return;
}

redisReply *MultiConsumerStateTable::popEventBuffer()
{
    if (m_keyspace_event_buffer.empty())
    {
        return NULL;
    }

    redisReply *reply = m_keyspace_event_buffer.front();
    m_keyspace_event_buffer.pop_front();

    return reply;
}

}
