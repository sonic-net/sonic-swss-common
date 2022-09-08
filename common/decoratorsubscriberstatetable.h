#pragma once

#include <assert.h>
#include <string>
#include <queue>
#include <tuple>
#include <utility>
#include <map>
#include <memory>
#include <deque>
#include "table.h"
#include "subscriberstatetable.h"

namespace swss {

class DecoratorSubscriberStateTable : public SubscriberStateTable 
{
public:
    DecoratorSubscriberStateTable(DBConnector *db, const std::string &tableName, int popBatchSize = swss::TableConsumable::DEFAULT_POP_BATCH_SIZE, int pri = 0);

    /* Get all elements available */
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco, const std::string &prefix = EMPTY_PREFIX) override;

private:
    std::string m_profile_keyspace;

    std::string m_profile_keyprefix;

    /* Handle SubscriberStateTable unknown pattern */
    void onPopUnknownPattern(RedisMessage& message, std::deque<KeyOpFieldsValuesTuple> &vkco) override;

    void appendDefaultValue(std::string &key, std::string &op, std::vector<FieldValueTuple> &fvs);
};

}