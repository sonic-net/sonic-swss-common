#pragma once

#include <string>
#include <deque>
#include <memory.h>
#include "dbconnector.h"
#include "consumertablebase.h"

namespace swss {

class SubscriberStateTable : public ConsumerTableBase
{
public:
    SubscriberStateTable(DBConnector *db, std::string tableName);

    /* Get all elements available */
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco, std::string prefix = EMPTY_PREFIX);

    /* Verify if cache contains data */
    int readCache();
    /* Read keyspace event from redis */
    void readMe();

private:
    /* Pop keyspace event from event buffer. Caller should free resources. */
    std::shared_ptr<RedisReply> popEventBuffer();

    std::string m_keyspace;

    std::deque<std::shared_ptr<RedisReply>> m_keyspace_event_buffer;
    Table m_table;
};

}
