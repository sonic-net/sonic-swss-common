
#include <boost/algorithm/string.hpp>

#include "common/decoratorsubscriberstatetable.h"
#include "common/defaultvalueprovider.h"
#include "common/staticconfigprovider.h"

using namespace std;
using namespace swss;

DecoratorSubscriberStateTable::DecoratorSubscriberStateTable(DBConnector *db, const string &tableName, int popBatchSize, int pri)
:SubscriberStateTable(db, tableName, popBatchSize, pri)
{
    // subscribe delete table for profile delete event
    const string separator = SonicDBConfig::getSeparator(db);
    m_profile_keyspace = "__keyspace@";
    m_profile_keyprefix = StaticConfigProvider::Instance().getDeletedKeyName(tableName, "", db);
    m_profile_keyspace += to_string(db->getDbId()) + "__:" + m_profile_keyprefix + "*";

    m_subscribe->psubscribe(m_profile_keyspace);
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
        return;
    }

    // Not append profile config, because 'SET' command will overwrite profile config.
    auto table_name = getTableName();
    DefaultValueProvider::Instance().AppendDefaultValues(table_name, key, fvs);
}

void DecoratorSubscriberStateTable::onPopUnknownPattern(RedisMessage& message, deque<KeyOpFieldsValuesTuple> &vkco)
{
    if (message.pattern != m_profile_keyspace)
    {
        SWSS_LOG_ERROR("invalid pattern %s returned for pmessage of %s", message.pattern.c_str(), m_profile_keyspace.c_str());
        SubscriberStateTable::onPopUnknownPattern(message, vkco);
        return;
    }

    string op = message.data;
    if ("del" == op)
    {
        // 'DEL' from delete table is revert profile config, ignore this because there will always be a user config 'SET' command.
        return;
    }

    string msg = message.channel;
    size_t pos = msg.find(m_profile_keyprefix);
    if (pos == msg.npos)
    {
        SWSS_LOG_ERROR("invalid key returned for pmessage of %s", m_profile_keyspace.c_str());
        return;
    }
    
    // 'SET' to delete table is delete profile config
    string key = msg.substr(pos + m_profile_keyprefix.length());
    KeyOpFieldsValuesTuple kco;
    kfvKey(kco) = key;
    kfvOp(kco) = DEL_COMMAND;
    vkco.push_back(kco);
}