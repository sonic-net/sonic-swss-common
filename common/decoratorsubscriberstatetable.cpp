#include <boost/algorithm/string.hpp>
#include "common/decoratorsubscriberstatetable.h"
#include "common/defaultvalueprovider.h"

using namespace std;
using namespace swss;

DecoratorSubscriberStateTable::DecoratorSubscriberStateTable(DBConnector *db, const string &tableName, int popBatchSize, int pri)
:SubscriberStateTable(db, tableName, popBatchSize, pri)
{
    m_defaultValueProvider = std::make_shared<DefaultValueProvider>();
}

/* Get multiple pop elements */
void DecoratorSubscriberStateTable::pops(deque<KeyOpFieldsValuesTuple> &vkco, const string &prefix)
{
    SubscriberStateTable::pops(vkco, prefix);
    for (auto& kco : vkco)
    {
        appendDefaultValue(kfvKey(kco), kfvOp(kco), kfvFieldsValues(kco));
    }
}

void DecoratorSubscriberStateTable::appendDefaultValue(std::string &key, std::string &op, std::vector<FieldValueTuple> &fvs)
{
    if (op != SET_COMMAND)
    {
        // When pops, 'set' operation means read data, only read need decorate.
        return;
    }

    auto table = getTableName();
    m_defaultValueProvider->appendDefaultValues(table, key, fvs);
}
