#include <hiredis/hiredis.h>
#include <system_error>

#include "common/logger.h"
#include "common/redisreply.h"
#include "common/rediscommand.h"
#include "common/redisapi.h"
#include "common/json.hpp"
#include "common/schema.h"
#include "common/countertable.h"

using namespace std;
using namespace swss;

const std::string Counter::defaultLuaScript = "return nil";

/*
 * Port counter type
 */
unique_ptr<KeyCache> PortCounter::keyCachePtr = nullptr;

PortCounter::PortCounter(PortCounter::Mode mode)
    : m_mode(mode)
{
    m_luaScript = loadLuaScript("portcounter.lua");
}

const std::string&
PortCounter::getLuaScript() const
{
    return m_luaScript;
}

bool
PortCounter::usingLuaTable(const CounterTable& t, const std::string &name) const
{
    if (m_mode != Mode::all)
        return false;

    // Whether gearbox port exists
    auto oidPtr = t.getGbcountersDB()->hget(COUNTERS_PORT_NAME_MAP, name + "_line");
    return (bool)oidPtr;
}


std::vector<std::string>
PortCounter::getLuaKeys(const CounterTable& t, const std::string &name) const
{
    if (m_mode != Mode::all)
        return {};

    KeyCache &cache = keyCacheInstance();
    if (cache.enabled())
    {
        return {cache.at(name), cache.at(name + "_system"), cache.at(name + "_line")};
    }

    auto oidLinesidePtr = t.getGbcountersDB()->hget(COUNTERS_PORT_NAME_MAP, name + "_line");
    auto oidSystemsidePtr = t.getGbcountersDB()->hget(COUNTERS_PORT_NAME_MAP, name + "_system");
    auto oidAsicPtr = t.getCountersDB()->hget(COUNTERS_PORT_NAME_MAP, name);

    if (oidAsicPtr && oidSystemsidePtr && oidLinesidePtr)
    {
        return {*oidAsicPtr, *oidSystemsidePtr, *oidLinesidePtr};
    }

    return {};
}

Counter::KeyPair
PortCounter::getKey(const CounterTable& t, const std::string &name) const
{
    if (m_mode == Mode::all)
        return {-1,""};

    int dbId = COUNTERS_DB;
    string portName = name;
    if (m_mode != Mode::asic)
    {
        dbId = GB_COUNTERS_DB;
        if (m_mode == Mode::systemside)
        {
            portName = name + "_system";
        }
        else
        {
            portName = name + "_line";
        }
    }

    KeyCache &cache = keyCacheInstance();
    if (cache.enabled())
    {
        return {dbId, cache.at(portName)};
    }

    shared_ptr<std::string> oidPtr = nullptr;
    if (m_mode == Mode::asic)
    {
        oidPtr = t.getCountersDB()->hget(COUNTERS_PORT_NAME_MAP, portName);
    }
    else
    {
        oidPtr = t.getGbcountersDB()->hget(COUNTERS_PORT_NAME_MAP, portName);
    }

    if (oidPtr == nullptr)
        return {-1, ""};
    return {dbId, *oidPtr};
}

KeyCache&
PortCounter::keyCacheInstance(void) const
{
    if (keyCachePtr == nullptr)
    {
        auto f = [](const CounterTable& t) {
            auto fvs = t.getCountersDB()->hgetall(COUNTERS_PORT_NAME_MAP);
            keyCachePtr->add(fvs.begin(), fvs.end());

            fvs = t.getGbcountersDB()->hgetall(COUNTERS_PORT_NAME_MAP);
            keyCachePtr->add(fvs.begin(), fvs.end());
        };
        unique_ptr<KeyCache> ptr(new KeyCache(f));
        keyCachePtr = std::move(ptr);
    }
    return *keyCachePtr;
}

/*
 * MACSEC counter type
 */
Counter::KeyPair
MacsecCounter::getKey(const CounterTable& t, const std::string &name) const
{
    int dbId = COUNTERS_DB;
    auto oidPtr = t.getCountersDB()->hget(COUNTERS_MACSEC_NAME_MAP, name);

    if (oidPtr == nullptr)
    {
        dbId = GB_COUNTERS_DB;
        oidPtr = t.getGbcountersDB()->hget(COUNTERS_MACSEC_NAME_MAP, name);
    }

    if (oidPtr == nullptr)
        return {-1, ""};
    return {dbId, *oidPtr};
}


CounterTable::CounterTable(const DBConnector *db, const string &tableName)
    : TableBase(tableName, SonicDBConfig::getSeparator(db))
    , m_countersDB(db->newConnector(0))
{
    unique_ptr<DBConnector> ptr(new DBConnector(GB_COUNTERS_DB, *m_countersDB));
    m_gbcountersDB = std::move(ptr);
}

bool CounterTable::get(const Counter &counter, const std::string &name, std::vector<FieldValueTuple> &values)
{
    if (counter.usingLuaTable(*this, name))
    {
        LuaTable luaTable(m_countersDB.get(), getTableName(), counter.getLuaScript());
        return luaTable.get(counter.getLuaKeys(*this, name), values);
    }
    else
    {
        auto keyPair = counter.getKey(*this, name);
        if (keyPair.second.empty())
        {
            values.clear();
            return false;
        }

        if (keyPair.first == GB_COUNTERS_DB)
        {
            Table table(m_gbcountersDB.get(), getTableName());
            return table.get(keyPair.second, values);
        }
        else
        {
            Table table(m_countersDB.get(), getTableName());
            return table.get(keyPair.second, values);
        }
    }
}


bool CounterTable::hget(const Counter &counter, const std::string &name, const std::string &field, std::string &value)
{
    if (counter.usingLuaTable(*this, name))
    {
        LuaTable luaTable(m_countersDB.get(), getTableName(), counter.getLuaScript());
        return luaTable.hget(counter.getLuaKeys(*this, name), field, value);
    }
    else
    {
        auto keyPair = counter.getKey(*this, name);
        if (keyPair.second.empty())
        {
            value.clear();
            return false;
        }

        if (keyPair.first == GB_COUNTERS_DB)
        {
            Table table(getGbcountersDB().get(), getTableName());
            return table.hget(keyPair.second, field, value);
        }
        else
        {
            Table table(m_countersDB.get(), getTableName());
            return table.hget(keyPair.second, field, value);
        }
    }
}
