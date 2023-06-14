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

class DecoratorTable : public Table  
{
public:
    DecoratorTable(const DBConnector *db, const std::string &tableName);
    DecoratorTable(RedisPipeline *pipeline, const std::string &tableName, bool buffered);

    /* Get all the field-value tuple of the table entry with the key */
    bool get(const std::string &key, std::vector<std::pair<std::string, std::string>> &ovalues) override;

    /* Get an entry field-value from the table */
    bool hget(const std::string &key, const std::string &field,  std::string &value) override;

private:
    std::shared_ptr<DefaultValueProvider> m_defaultValueProvider;
};

}
