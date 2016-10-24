#pragma once

#include <string>
#include <vector>
#include <limits>
#include <hiredis/hiredis.h>
#include "dbconnector.h"
#include "table.h"
#include "selectable.h"
#include "redisselect.h"

namespace swss {

class ConsumerStateTable : public RedisSelect, public TableName_KeySet, public TableEntryPopable
{
public:
    ConsumerStateTable(DBConnector *db, std::string tableName)
        : TableName_KeySet(tableName)
        , RedisSelect(RedisChannel(db, tableName))
    {
        for (;;)
        {
            try
            {
                RedisReply watch(m_db, string("WATCH ") + getKeySetName(), REDIS_REPLY_STATUS);
                watch.checkStatusOK();
                multi();
                enqueue(string("SCARD ") + getKeySetName(), REDIS_REPLY_INTEGER);
                
                subscribe();

                exec();
                break;
            }
            catch (...)
            {
                // TODO: log
                continue;
            }
        }

        setQueueLength(queueResultsFront()->integer);
        /* No need for that since we have WATCH gurantee on the transaction */
    }

    /* Get a singlesubsribe channel rpop */
    void pop(KeyOpFieldsValuesTuple &kco)
    {
        auto& values = kfvFieldsValues(kco);
        values.clear();
        
        // try pop one key from the set
        RedisReply rk(m_db, "SPOP " + getKeySetName());
        
        // if the set is empty, return an empty kco object
        if (rk.getContext()->type == REDIS_REPLY_NIL)
        {
            kfvKey(kco).clear();
            kfvOp(kco).clear();
            return;
        }

        std::string key = rk.getReply<std::string>();
        kfvKey(kco) = key;
        
        RedisReply r(m_db, "HGETALL " + getKeyName(key), REDIS_REPLY_ARRAY);
        redisReply *reply = r.getContext();

        // if there is no field-value pair, the key is already deleted
        if (reply->elements == 0)
        {
            kfvOp(kco) = DEL_COMMAND;
        }
        else
        {
            kfvOp(kco) = SET_COMMAND;
            for (unsigned int i = 0; i < reply->elements - 1; i += 2)
            {
                values.push_back(make_tuple(reply->element[i]->str,
                                            reply->element[i + 1]->str));
            }
        }
    }
};

}
