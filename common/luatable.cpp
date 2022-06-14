#include <hiredis/hiredis.h>
#include <system_error>

#include "common/logger.h"
#include "common/redisreply.h"
#include "common/rediscommand.h"
#include "common/redisapi.h"
#include "common/json.hpp"
#include "common/schema.h"
#include "common/luatable.h"

using namespace std;
using namespace swss;

LuaTable::LuaTable(const DBConnector *db, const string &tableName,
                   const string &lua, const vector<string> &luaArgv)
    : TableBase(tableName, SonicDBConfig::getSeparator(db))
    , m_db(db->newConnector(0))
    , m_lua(lua)
    , m_luaArgv(luaArgv)
{
}

LuaTable::~LuaTable()
{
}

bool LuaTable::get(const vector<string> &luaKeys, vector<FieldValueTuple> &values)
{
    if (m_lua.empty() || luaKeys.empty())
    {
        values.clear();
        return false;
    }

    // Assembly redis command args into a string vector
    vector<string> args;
    args.emplace_back("EVALSHA");
    args.emplace_back(loadRedisScript(m_db.get(), m_lua));
    args.emplace_back(to_string(luaKeys.size()));
    for (const auto& k: luaKeys)
    {
        args.emplace_back(k);
    }

    // ARGV[1..4]: counter db, gearbox counter db, table name, separator
    args.emplace_back(to_string(COUNTERS_DB));
    args.emplace_back(to_string(GB_COUNTERS_DB));
    args.emplace_back(getTableName());
    args.emplace_back(getTableNameSeparator());
    args.emplace_back("HGETALL");  // ARGV[5] get all fields
    for (const auto& v: m_luaArgv) // ARGV[6...] extra user-defined
    {
        args.emplace_back(v);
    }

    // Transform data structure
    vector<const char *> args1;
    transform(args.begin(), args.end(), back_inserter(args1), [](const string &s) { return s.c_str(); } );

    // Invoke redis command
    RedisCommand command;
    command.formatArgv((int)args1.size(), &args1[0], NULL);
    RedisReply r(m_db.get(), command, REDIS_REPLY_ARRAY);
    redisReply *reply = r.getContext();

    if (!reply->elements)
        return false;

    if (reply->elements & 1)
        throw system_error(make_error_code(errc::address_not_available),
                           "Unable to connect netlink socket");

    values.clear();
    for (unsigned int i = 0; i < reply->elements; i += 2)
    {
        values.emplace_back(reply->element[i]->str, reply->element[i + 1]->str);
    }
    return true;
}

bool LuaTable::hget(const vector<string> &luaKeys, const string &field, string &value)
{
    if (m_lua.empty() || luaKeys.empty() || field.empty())
    {
        value.clear();
        return false;
    }

    // Assembly redis command args into a string vector
    vector<string> args;
    args.emplace_back("EVALSHA");
    args.emplace_back(loadRedisScript(m_db.get(), m_lua));
    args.emplace_back(to_string(luaKeys.size()));
    for (const auto& k: luaKeys)
    {
        args.emplace_back(k);
    }

    // ARGV[1..4]: counter db, gearbox counter db, table name, separator
    args.emplace_back(to_string(COUNTERS_DB));
    args.emplace_back(to_string(GB_COUNTERS_DB));
    args.emplace_back(getTableName());
    args.emplace_back(getTableNameSeparator());
    args.emplace_back("HGET");  // ARGV[5] get one field
    args.emplace_back(field);    // ARGV[6] field name
    for (const auto& v: m_luaArgv) // ARGV[7...] extra user-defined
    {
        args.emplace_back(v);
    }

    // Transform data structure
    vector<const char *> args1;
    transform(args.begin(), args.end(), back_inserter(args1), [](const string &s) { return s.c_str(); } );

    // Invoke redis command
    RedisCommand command;
    command.formatArgv((int)args1.size(), &args1[0], NULL);
    RedisReply r(m_db.get(), command);
    redisReply *reply = r.getContext();

    if (reply->type == REDIS_REPLY_NIL)
    {
        value.clear();
        return false;
    }

    if (reply->type != REDIS_REPLY_STRING)
        throw system_error(make_error_code(errc::io_error),
                "Got unexpected reply type");

    value = reply->str;
    return true;
}
