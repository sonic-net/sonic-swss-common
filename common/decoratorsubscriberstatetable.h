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
#include "defaultvalueprovider.h"

namespace swss {

class DecoratorSubscriberStateTable : public SubscriberStateTable 
{
public:
    DecoratorSubscriberStateTable(DBConnector *db, const std::string &tableName, int popBatchSize = swss::TableConsumable::DEFAULT_POP_BATCH_SIZE, int pri = 0);

    /* Get all elements available */
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco, const std::string &prefix = EMPTY_PREFIX) override;

private:
    std::shared_ptr<DefaultValueProvider> m_defaultValueProvider;

    void appendDefaultValue(std::string &key, std::string &op, std::vector<FieldValueTuple> &fvs);
};

}