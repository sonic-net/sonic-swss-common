#include <stdlib.h>
#include <tuple>
#include <sstream>
#include <utility>
#include <algorithm>
#include "redisreply.h"
#include "table.h"
#include "redisapi.h"
#include "redispipeline.h"
#include "producerstatetable.h"

using namespace std;

namespace swss {

ProducerStateTable::ProducerStateTable(DBConnector *db, const string &tableName)
    : ProducerStateTable(new RedisPipeline(db, 1), tableName, false, false)
{
    m_pipeowned = true;
}

ProducerStateTable::ProducerStateTable(RedisPipeline *pipeline, const string &tableName, bool buffered)
    : ProducerStateTable(pipeline, tableName, buffered, false) {}

ProducerStateTable::ProducerStateTable(RedisPipeline *pipeline, const string &tableName, bool buffered, bool flushPub)
    : TableBase(tableName, SonicDBConfig::getSeparator(pipeline->getDBConnector()))
    , TableName_KeySet(tableName)
    , m_flushPub(flushPub)
    , m_buffered(buffered)
    , m_pipeowned(false)
    , m_tempViewActive(false)
    , m_pipe(pipeline)
{
    reloadRedisScript();

    string luaClear =
        "redis.call('DEL', KEYS[1])\n"
        "local keys = redis.call('KEYS', KEYS[2] .. '*')\n"
        "for i,k in pairs(keys) do\n"
        "    redis.call('DEL', k)\n"
        "end\n"
        "redis.call('DEL', KEYS[3])\n";
    m_shaClear = m_pipe->loadRedisScript(luaClear);

    string luaApplyView = loadLuaScript("producer_state_table_apply_view.lua");
    m_shaApplyView = m_pipe->loadRedisScript(luaApplyView);
}

ProducerStateTable::~ProducerStateTable()
{
    if (m_pipeowned)
    {
        delete m_pipe;
    }
}

void ProducerStateTable::reloadRedisScript()
{
    // Set m_flushPub to remove publish from a single lua string and let pipeline do publish once per flush

    // However, if m_buffered is false, follow the original one publish per lua design
    // Hence we need to check both m_buffered and m_flushPub, and reload the redis script once setBuffered() changes m_buffered

    /* 1. Inform the pipeline of what channel to publish, when flushPub feature is enabled */
    if (m_buffered && m_flushPub)
        m_pipe->addChannel(getChannelName(m_pipe->getDbId()));

    /* 2. Setup lua strings: determine whether to attach luaPub after each lua string */

    // num in luaSet and luaDel means number of elements that were added to the key set,
    // not including all the elements already present into the set.
    string luaSet =
        "local added = redis.call('SADD', KEYS[2], ARGV[2])\n"
        "for i = 0, #KEYS - 3 do\n"
        "    redis.call('HSET', KEYS[3 + i], ARGV[3 + i * 2], ARGV[4 + i * 2])\n"
        "end\n";

    string luaDel =
        "local added = redis.call('SADD', KEYS[2], ARGV[2])\n"
        "redis.call('SADD', KEYS[4], ARGV[2])\n"
        "redis.call('DEL', KEYS[3])\n";

    string luaBatchedSet =
        "local added = 0\n"
        "local idx = 2\n"
        "for i = 0, #KEYS - 4 do\n"
        "    added = added + redis.call('SADD', KEYS[2], KEYS[4 + i])\n"
        "    for j = 0, tonumber(ARGV[idx]) - 1 do\n"
        "        local attr = ARGV[idx + j * 2 + 1]\n"
        "        local val = ARGV[idx + j * 2 + 2]\n"
        "        redis.call('HSET', KEYS[3] .. KEYS[4 + i], attr, val)\n"
        "    end\n"
        "    idx = idx + tonumber(ARGV[idx]) * 2 + 1\n"
        "end\n";

    string luaBatchedDel =
        "local added = 0\n"
        "for i = 0, #KEYS - 5 do\n"
        "    added = added + redis.call('SADD', KEYS[2], KEYS[5 + i])\n"
        "    redis.call('SADD', KEYS[3], KEYS[5 + i])\n"
        "    redis.call('DEL', KEYS[4] .. KEYS[5 + i])\n"
        "end\n";

    if (!m_flushPub || !m_buffered)
    {
        string luaPub =
            "if added > 0 then \n"
            "    redis.call('PUBLISH', KEYS[1], ARGV[1])\n"
            "end\n";
        luaSet += luaPub;
        luaDel += luaPub;
        luaBatchedSet += luaPub;
        luaBatchedDel += luaPub;
    }

    /* 3. load redis script based on the lua string */
    m_shaSet = m_pipe->loadRedisScript(luaSet);
    m_shaDel = m_pipe->loadRedisScript(luaDel);
    m_shaBatchedSet = m_pipe->loadRedisScript(luaBatchedSet);
    m_shaBatchedDel = m_pipe->loadRedisScript(luaBatchedDel);
}

void ProducerStateTable::setBuffered(bool buffered)
{
    m_buffered = buffered;
    reloadRedisScript();
}

void ProducerStateTable::set(const string &key, const vector<FieldValueTuple> &values,
                 const string &op /*= SET_COMMAND*/, const string &prefix)
{
    if (m_tempViewActive)
    {
        // Write to temp view instead of DB
        for (const auto& iv: values)
        {
            m_tempViewState[key][fvField(iv)] = fvValue(iv);
        }
        return;
    }

    // Assembly redis command args into a string vector
    vector<string> args;
    args.emplace_back("EVALSHA");
    args.emplace_back(m_shaSet);
    args.emplace_back(to_string(values.size() + 2));
    args.emplace_back(getChannelName(m_pipe->getDbId()));
    args.emplace_back(getKeySetName());

    args.insert(args.end(), values.size(), getStateHashPrefix() + getKeyName(key));

    args.emplace_back("G");
    args.emplace_back(key);
    for (const auto& iv: values)
    {
        args.emplace_back(fvField(iv));
        args.emplace_back(fvValue(iv));
    }

    // Invoke redis command
    RedisCommand command;
    command.format(args);
    m_pipe->push(command, REDIS_REPLY_NIL);
    if (!m_buffered)
    {
        m_pipe->flush();
    }
}

void ProducerStateTable::del(const string &key, const string &op /*= DEL_COMMAND*/, const string &prefix)
{
    if (m_tempViewActive)
    {
        // Write to temp view instead of DB
        m_tempViewState.erase(key);
        return;
    }

    // Assembly redis command args into a string vector
    vector<string> args;
    args.emplace_back("EVALSHA");
    args.emplace_back(m_shaDel);
    args.emplace_back("4");
    args.emplace_back(getChannelName(m_pipe->getDbId()));
    args.emplace_back(getKeySetName());
    args.emplace_back(getStateHashPrefix() + getKeyName(key));
    args.emplace_back(getDelKeySetName());
    args.emplace_back("G");
    args.emplace_back(key);
    args.emplace_back("''");
    args.emplace_back("''");

    // Invoke redis command
    RedisCommand command;
    command.format(args);
    m_pipe->push(command, REDIS_REPLY_NIL);
    if (!m_buffered)
    {
        m_pipe->flush();
    }
}

void ProducerStateTable::set(const std::vector<KeyOpFieldsValuesTuple>& values)
{
    if (m_tempViewActive)
    {
        // Write to temp view instead of DB
        for (const auto &value : values)
        {
            const std::string &key = kfvKey(value);
            for (const auto &iv : kfvFieldsValues(value))
            {
                m_tempViewState[key][fvField(iv)] = fvValue(iv);
            }
        }
        return;
    }

    // Assembly redis command args into a string vector
    vector<string> args;
    args.emplace_back("EVALSHA");
    args.emplace_back(m_shaBatchedSet);
    args.emplace_back(to_string(values.size() + 3));
    args.emplace_back(getChannelName(m_pipe->getDbId()));
    args.emplace_back(getKeySetName());
    args.emplace_back(getStateHashPrefix() + getTableName() + getTableNameSeparator());
    for (const auto &value : values)
    {
        args.emplace_back(kfvKey(value));
    }
    args.emplace_back("G");
    for (const auto &value : values)
    {
        args.emplace_back(to_string(kfvFieldsValues(value).size()));
        for (const auto &iv : kfvFieldsValues(value))
        {
            args.emplace_back(fvField(iv));
            args.emplace_back(fvValue(iv));
        }
    }

    // Invoke redis command
    RedisCommand command;
    command.format(args);
    m_pipe->push(command, REDIS_REPLY_NIL);
    if (!m_buffered)
    {
        m_pipe->flush();
    }
}

void ProducerStateTable::del(const std::vector<std::string>& keys)
{
    if (m_tempViewActive)
    {
        // Write to temp view instead of DB
        for (const auto &key : keys)
        {
            m_tempViewState.erase(key);
        }
        return;
    }

    // Assembly redis command args into a string vector
    vector<string> args;
    args.emplace_back("EVALSHA");
    args.emplace_back(m_shaBatchedDel);
    args.emplace_back(to_string(keys.size() + 4));
    args.emplace_back(getChannelName(m_pipe->getDbId()));
    args.emplace_back(getKeySetName());
    args.emplace_back(getDelKeySetName());
    args.emplace_back(getStateHashPrefix() + getTableName() + getTableNameSeparator());
    for (const auto &key : keys)
    {
        args.emplace_back(key);
    }
    args.emplace_back("G");

    // Invoke redis command
    RedisCommand command;
    command.format(args);
    m_pipe->push(command, REDIS_REPLY_NIL);
    if (!m_buffered)
    {
        m_pipe->flush();
    }
}

void ProducerStateTable::flush()
{
    m_pipe->flush();
}

int64_t ProducerStateTable::count()
{
    RedisCommand cmd;
    cmd.format("SCARD %s", getKeySetName().c_str());
    RedisReply r = m_pipe->push(cmd);
    r.checkReplyType(REDIS_REPLY_INTEGER);

    return r.getReply<long long int>();
}

// Warning: calling this function will cause all data in keyset and the temporary table to be abandoned.
// ConsumerState may have got the notification from PUBLISH, but will see no data popped.
void ProducerStateTable::clear()
{
    // Assembly redis command args into a string vector
    vector<string> args;
    args.emplace_back("EVALSHA");
    args.emplace_back(m_shaClear);
    args.emplace_back("3");
    args.emplace_back(getKeySetName());
    args.emplace_back(getStateHashPrefix() + getTableName());
    args.emplace_back(getDelKeySetName());

    // Invoke redis command
    RedisCommand cmd;
    cmd.format(args);
    m_pipe->push(cmd, REDIS_REPLY_NIL);
    m_pipe->flush();
}

void ProducerStateTable::create_temp_view()
{
    if (m_tempViewActive)
    {
        SWSS_LOG_WARN("create_temp_view() called for table %s when another temp view is under work, %zd objects in existing temp view will be discarded.", getTableName().c_str(), m_tempViewState.size());
    }
    m_tempViewActive = true;
    m_tempViewState.clear();
}

void ProducerStateTable::apply_temp_view()
{
    if (!m_tempViewActive)
    {
        SWSS_LOG_THROW("apply_temp_view() called for table %s, however no temp view was created.", getTableName().c_str());
    }

    // Drop all pending operation first
    clear();

    TableDump currentState;
    {
        Table mainTable(m_pipe, getTableName(), false);
        mainTable.dump(currentState);
    }

    // Print content of current view and temp view as debug log
    SWSS_LOG_INFO("View switch of table %s required.", getTableName().c_str());
    SWSS_LOG_INFO("Objects in current view:");
    for (auto const & kfvPair : currentState)
    {
        SWSS_LOG_INFO("    %s: %zd fields;", kfvPair.first.c_str(), kfvPair.second.size());
    }
    SWSS_LOG_INFO("Objects in target view:");
    for (auto const & kfvPair : m_tempViewState)
    {
        SWSS_LOG_INFO("    %s: %zd fields;", kfvPair.first.c_str(), kfvPair.second.size());
    }


    std::vector<std::string> keysToSet;
    std::vector<std::string> keysToDel;

    // Compare based on existing objects.
    //     Please note that this comparation is literal not contextual -
    //     e.g. {nexthop: 10.1.1.1, 10.1.1.2} and {nexthop: 10.1.1.2, 10.1.1.1} will be treated as different.
    //     Application will need to handle it, to make sure contextually identical field values also literally identical.
    for (auto const & kfvPair : currentState)
    {
        const string& key = kfvPair.first;
        const TableMap& fieldValueMap = kfvPair.second;
        // DEL is needed if object does not exist in new state, or any field is not presented in new state
        // SET is almost always needed, unless old state and new state exactly match each other
        //     (All old fields exists in new state, values match, and there is no additional field in new state)
        if (m_tempViewState.find(key) == m_tempViewState.end())         // Key does not exist in new view
        {
            keysToDel.emplace_back(key);
            keysToSet.emplace_back(key);
            continue;
        }
        const TableMap& newFieldValueMap = m_tempViewState[key];
        bool needDel = false;
        bool needSet = false;
        for (auto const& fvPair : fieldValueMap)
        {
            const string& field = fvPair.first;
            const string& value = fvPair.second;
            if (newFieldValueMap.find(field) == newFieldValueMap.end()) // Field does not exist in new view
            {
                needDel = true;
                needSet = true;
                break;
            }
            if (newFieldValueMap.at(field) != value)                       // Field value changed
            {
                needSet = true;
            }
        }
        if (newFieldValueMap.size() > fieldValueMap.size())             // New field added
        {
            needSet = true;
        }

        if (needDel)
        {
            keysToDel.emplace_back(key);
        }
        if (needSet)
        {
            keysToSet.emplace_back(key);
        }
        else  // If exactly match, no need to sync new state to StateHash in DB
        {
            m_tempViewState.erase(key);
        }
    }
    // Objects that do not exist currently need to be created
    for (auto const & kfvPair : m_tempViewState)
    {
        const string& key = kfvPair.first;
        if (currentState.find(key) == currentState.end())
        {
            keysToSet.emplace_back(key);
        }
    }

    // Assembly redis command args into a string vector
    // See comment in producer_state_table_apply_view.lua for argument format
    vector<string> args;
    args.emplace_back("EVALSHA");
    args.emplace_back(m_shaApplyView);
    args.emplace_back(to_string(m_tempViewState.size() + 3));
    args.emplace_back(getChannelName(m_pipe->getDbId()));
    args.emplace_back(getKeySetName());
    args.emplace_back(getDelKeySetName());

    vector<string> argvs;
    argvs.emplace_back("G");
    argvs.emplace_back(to_string(keysToSet.size()));
    argvs.insert(argvs.end(), keysToSet.begin(), keysToSet.end());
    argvs.emplace_back(to_string(keysToDel.size()));
    argvs.insert(argvs.end(), keysToDel.begin(), keysToDel.end());
    for (auto const & kfvPair : m_tempViewState)
    {
        const string& key = kfvPair.first;
        const TableMap& fieldValueMap = kfvPair.second;
        args.emplace_back(getStateHashPrefix() + getKeyName(key));
        argvs.emplace_back(to_string(fieldValueMap.size()));
        for (auto const& fvPair : fieldValueMap)
        {
            const string& field = fvPair.first;
            const string& value = fvPair.second;
            argvs.emplace_back(field);
            argvs.emplace_back(value);
        }
    }
    args.insert(args.end(), argvs.begin(), argvs.end());

    // Log arguments for debug
    {
        std::stringstream ss;
        for (auto const & item : args)
        {
            ss << item << " ";
        }
        SWSS_LOG_DEBUG("apply_view.lua is called with following argument list: %s", ss.str().c_str());
    }

    // Invoke redis command
    RedisCommand command;
    command.format(args);
    m_pipe->push(command, REDIS_REPLY_NIL);
    m_pipe->flush();

    // Clear state, temp view operation is now finished
    m_tempViewState.clear();
    m_tempViewActive = false;
}

}
