#pragma once

#include <string>
#include <deque>
#include "dbconnector.h"
#include "consumertablebase.h"

namespace swss {

class MultiConsumerStateTable : public ConsumerTableBase
{
public:
    MultiConsumerStateTable(DBConnector *db, std::string tableName);

    /* Get all elements available */
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco, std::string prefix = EMPTY_PREFIX);

    /* Verify if cache contains data */
    int readCache();
    /* Read keyspace event from redis */
    void readMe();

private:
    /* Pop keyspace event from event buffer. Caller should free resources. */
    redisReply *popEventBuffer();

    std::string m_keyspace;

    std::deque<redisReply *> m_keyspace_event_buffer;
    Table m_table;
};

}
