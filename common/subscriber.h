#pragma once

#include <string>
#include <deque>
#include <vector>
#include "dbconnector.h"
#include "table.h"
#include "redisselect.h"

namespace swss {

class Subscriber : public RedisTransactioner, public RedisSelect, public TableEntryPoppable, public TableBase
{
public:

    Subscriber(DBConnector *db, std::string tableName);

    /* Get a single subscribe channel pop */
    /* If there is nothing to pop, the output paramter will have empty key and op */
    void pop(KeyOpFieldsValuesTuple &kco, std::string prefix = EMPTY_PREFIX);

    /* Get all elements available */
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco, std::string prefix = EMPTY_PREFIX);

private:
    std::string m_keyspace;
    std::deque<KeyOpFieldsValuesTuple> m_buffer;

    /* Read a value from the DB directly via synchronous channel */
    /* Returns false if the key doesn't exist */
    bool get(std::string key, std::vector<FieldValueTuple> &values);
};

}
