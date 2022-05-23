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

PortCounter::PortCounter(const DBConnector *db, PortCounter::Mode mode)
    : m_mode(mode)
    , m_countersDB(db->newConnector(0))
    , m_gbcountersDB(nullptr)
{
    m_luaScript = loadLuaScript("portcounter.lua");
}

const std::string&
PortCounter::getLuaScript()
{
    return m_luaScript;
}

unique_ptr<DBConnector>&
PortCounter::getGbcountersDB()
{
    if (!m_gbcountersDB)
    {
        unique_ptr<DBConnector> ptr(new DBConnector(GB_COUNTERS_DB, *m_countersDB));
        m_gbcountersDB = std::move(ptr);
    }

    return m_gbcountersDB;
}

bool
PortCounter::usingLuaTable(const std::string &name)
{
    if (m_mode != Mode::all)
        return false;

    // Whether gearbox port exists
    auto oidPtr = getGbcountersDB()->hget(COUNTERS_PORT_NAME_MAP, name + "_line");
    return (bool)oidPtr;
}


std::vector<std::string>
PortCounter::getLuaKeys(const std::string &name)
{
    if (m_mode != Mode::all)
        return {};

    auto oidLinesidePtr = getGbcountersDB()->hget(COUNTERS_PORT_NAME_MAP, name + "_line");
    auto oidSystemsidePtr = getGbcountersDB()->hget(COUNTERS_PORT_NAME_MAP, name + "_system");
    auto oidAsicPtr = m_countersDB->hget(COUNTERS_PORT_NAME_MAP, name);

    if (oidAsicPtr && oidSystemsidePtr && oidLinesidePtr)
    {
        return {*oidAsicPtr, *oidSystemsidePtr, *oidLinesidePtr};
    }

    return {};
}

std::string
PortCounter::getKey(const std::string &name)
{
    shared_ptr<std::string> oidPtr = nullptr;

    if (m_mode == Mode::all)
        return "";

    if (m_mode == Mode::asic)
    {
        oidPtr = m_countersDB->hget(COUNTERS_PORT_NAME_MAP, name);
    }
    else if (m_mode == Mode::systemside)
    {
        oidPtr = getGbcountersDB()->hget(COUNTERS_PORT_NAME_MAP, name + "_system");
    }
    else
    {
        oidPtr = getGbcountersDB()->hget(COUNTERS_PORT_NAME_MAP, name + "_line");
    }

    return oidPtr == nullptr ? "" : *oidPtr;
}


MacsecCounter::MacsecCounter(const DBConnector *db)
    : m_countersDB(db->newConnector(0))
{
}

std::string
MacsecCounter::getKey(const std::string &name)
{
    auto oidPtr = m_countersDB->hget(COUNTERS_MACSEC_NAME_MAP, name);

    if (oidPtr == nullptr)
    {
        DBConnector db(GB_COUNTERS_DB, *m_countersDB);
        oidPtr = db.hget(COUNTERS_MACSEC_NAME_MAP, name);
    }

    return oidPtr == nullptr ? "" : *oidPtr;
}

CounterTable::CounterTable(Counter *counter, const DBConnector *db, const string &tableName)
    : TableBase(tableName, SonicDBConfig::getSeparator(db))
    , m_counter(counter)
    , m_countersDB(db->newConnector(0))
{
}

std::unique_ptr<LuaTable>& CounterTable::getLuaTable()
{
    if (!m_luaTable)
    {
        unique_ptr<LuaTable> ptr(new LuaTable(m_countersDB.get(), getTableName(), m_counter->getLuaScript()));
        m_luaTable = std::move(ptr);
    }

    return m_luaTable;
}

std::unique_ptr<Table>& CounterTable::getTable()
{
    if (!m_table)
    {
        unique_ptr<Table> ptr(new Table(m_countersDB.get(), getTableName()));
        m_table = std::move(ptr);
    }

    return m_table;
}

bool CounterTable::get(const std::string &name, std::vector<FieldValueTuple> &values)
{
    if (m_counter->usingLuaTable(name))
    {
        return getLuaTable()->get(m_counter->getLuaKeys(name), values);
    }
    else
    {
        return getTable()->get(m_counter->getKey(name), values);
    }
}


bool CounterTable::hget(const std::string &name, const std::string &field, std::string &value)
{
    if (m_counter->usingLuaTable(name))
    {
        return getLuaTable()->hget(m_counter->getLuaKeys(name), field, value);
    }
    else
    {
        return getTable()->hget(m_counter->getKey(name), field, value);
    }
}
