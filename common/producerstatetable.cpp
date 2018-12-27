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
    : ProducerStateTable(new RedisPipeline(db, 1), tableName, false)
{
    m_pipeowned = true;
}

ProducerStateTable::ProducerStateTable(RedisPipeline *pipeline, const string &tableName, bool buffered)
    : TableBase(pipeline->getDbId(), tableName)
    , TableName_KeySet(tableName)
    , m_buffered(buffered)
    , m_pipeowned(false)
    , m_tempViewActive(false)
    , m_pipe(pipeline)
{
    string luaSet =
        "redis.call('SADD', KEYS[2], ARGV[2])\n"
        "for i = 0, #KEYS - 3 do\n"
        "    redis.call('HSET', KEYS[3 + i], ARGV[3 + i * 2], ARGV[4 + i * 2])\n"
        "end\n"
        "redis.call('PUBLISH', KEYS[1], ARGV[1])\n";
    m_shaSet = m_pipe->loadRedisScript(luaSet);

    string luaDel =
        "redis.call('SADD', KEYS[2], ARGV[2])\n"
        "redis.call('SADD', KEYS[4], ARGV[2])\n"
        "redis.call('DEL', KEYS[3])\n"
        "redis.call('PUBLISH', KEYS[1], ARGV[1])\n";
    m_shaDel = m_pipe->loadRedisScript(luaDel);

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

void ProducerStateTable::setBuffered(bool buffered)
{
    m_buffered = buffered;
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
    args.emplace_back(getChannelName());
    args.emplace_back(getKeySetName());

    args.insert(args.end(), values.size(), getStateHashPrefix() + getKeyName(key));

    args.emplace_back("G");
    args.emplace_back(key);
    for (const auto& iv: values)
    {
        args.emplace_back(fvField(iv));
        args.emplace_back(fvValue(iv));
    }

    // Transform data structure
    vector<const char *> args1;
    transform(args.begin(), args.end(), back_inserter(args1), [](const string &s) { return s.c_str(); } );

    // Invoke redis command
    RedisCommand command;
    command.formatArgv((int)args1.size(), &args1[0], NULL);
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
    args.emplace_back(getChannelName());
    args.emplace_back(getKeySetName());
    args.emplace_back(getStateHashPrefix() + getKeyName(key));
    args.emplace_back(getDelKeySetName());
    args.emplace_back("G");
    args.emplace_back(key);
    args.emplace_back("''");
    args.emplace_back("''");

    // Transform data structure
    vector<const char *> args1;
    transform(args.begin(), args.end(), back_inserter(args1), [](const string &s) { return s.c_str(); } );

    // Invoke redis command
    RedisCommand command;
    command.formatArgv((int)args1.size(), &args1[0], NULL);
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

    // Transform data structure
    vector<const char *> args1;
    transform(args.begin(), args.end(), back_inserter(args1), [](const string &s) { return s.c_str(); } );

    // Invoke redis command
    RedisCommand cmd;
    cmd.formatArgv((int)args1.size(), &args1[0], NULL);
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
    args.emplace_back(getChannelName());
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

    // Transform data structure
    vector<const char *> args1;
    transform(args.begin(), args.end(), back_inserter(args1), [](const string &s) { return s.c_str(); } );

    // Invoke redis command
    RedisCommand command;
    command.formatArgv((int)args1.size(), &args1[0], NULL);
    m_pipe->push(command, REDIS_REPLY_NIL);
    m_pipe->flush();

    // Clear state, temp view operation is now finished
    m_tempViewState.clear();
    m_tempViewActive = false;
}

}
