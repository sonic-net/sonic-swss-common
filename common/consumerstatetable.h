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
        // if pop one key from the set
        // return the key-op-fields
        // else op<-DEL
        RedisReply rk(m_db, "SPOP " + getKeySetName(), REDIS_REPLY_STRING);
        std::string key = rk.getReply<std::string>();
        
        RedisReply r(m_db, "HGETALL " + getKeyName(key), REDIS_REPLY_ARRAY);
        redisReply *reply = r.getContext();
        auto& values = kfvFieldsValues(kco);
        values.clear();

        if (reply->elements == 0)
        {
            kfvOp(kco) = DEL_COMMAND;
            values.clear();
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
