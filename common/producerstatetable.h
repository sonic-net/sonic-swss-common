#pragma once

#include <stdlib.h>
#include <tuple>
#include "redisreply.h"
#include "json.h"
#include "json.hpp"
#include "table.h"

using namespace std;
using json = nlohmann::json;

namespace swss {
    
class ProducerStateTable : public RedisChannel, public TableName_KeySet, public TableEntryWritable
{
public:
    ProducerStateTable(DBConnector *db, std::string tableName)
        : TableName_KeySet(tableName)
        , RedisChannel(db, tableName)
    {
    }

    /* Implements set() and del() commands using notification messages */
    virtual void set(std::string key, std::vector<FieldValueTuple> &values,
                     std::string op = SET_COMMAND)
    {
        multi();
        
        enqueKeySet(key);
        
        for (FieldValueTuple &i : values)
            enqueue(RedisFormatter::formatHSET(getKeyName(key), fvField(i), fvValue(i)),
                    REDIS_REPLY_INTEGER, true);
                    
        exec();
    }

    virtual void del(std::string key, std::string op = DEL_COMMAND)
    {
        multi();
        
        enqueKeySet(key);
        
        std::string del("DEL ");
        del += getKeyName(key);

        enqueue(del, REDIS_REPLY_INTEGER);
        
        exec();
    }
    
private:
    void enqueKeySet(std::string key)
    {
        enqueue("SADD " + getKeySetName() + " " + key, REDIS_REPLY_INTEGER);
        
        publish();
    }
};

}

