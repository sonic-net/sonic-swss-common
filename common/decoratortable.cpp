#include <boost/algorithm/string.hpp>

#include "common/decoratortable.h"

using namespace std;
using namespace swss;

DecoratorTable::DecoratorTable(const DBConnector *db, const string &tableName)
    : Table(db, tableName)
{
    m_defaultValueProvider = std::make_shared<DefaultValueProvider>();
}

DecoratorTable::DecoratorTable(RedisPipeline *pipeline, const string &tableName, bool buffered)
:Table(pipeline, tableName, buffered)
{
    m_defaultValueProvider = std::make_shared<DefaultValueProvider>();
}

/* Get all the field-value tuple of the table entry with the key */
bool DecoratorTable::get(const string &key, vector<pair<string, string>> &ovalues)
{
    bool result = Table::get(key, ovalues);
    auto table = getTableName();

    // Append default values
    m_defaultValueProvider->appendDefaultValues(table, key, ovalues);

    return result;
}

bool DecoratorTable::hget(const string &key, const string &field,  string &value)
{
    auto result = Table::hget(key,
                        field,
                        value);
    if (result)
    {
        return true;
    }

    auto table = getTableName();

    // Try append default values
    auto default_value = m_defaultValueProvider->getDefaultValue(table, key, field);
    if (default_value != nullptr)
    {
        value = *default_value;
        return true;
    }

    return false;
}