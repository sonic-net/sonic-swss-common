
#ifndef __COUNTERTABLE__
#define __COUNTERTABLE__

#include <assert.h>
#include <string>
#include <vector>
#include "table.h"
#include "luatable.h"

namespace swss {
struct Counter {
    virtual const std::string& getLuaScript() {
        return defaultLuaScript;
    }
    virtual std::vector<std::string> getLuaArgv() {
        return {};
    }
    virtual bool usingLuaTable(const std::string &name) {
        return false;
    }
    virtual std::vector<std::string> getLuaKeys(const std::string &name) {
        return {};
    }
    virtual std::string getKey(const std::string &name) = 0;
    virtual ~Counter() = default;

private:
    static const std::string defaultLuaScript;
};

class PortCounter: public Counter {
public:
    enum class Mode {all, asic, systemside, lineside};

    PortCounter(const DBConnector *db, Mode mode=Mode::all);
    ~PortCounter() = default;

    const std::string& getLuaScript() override;
    bool usingLuaTable(const std::string &name) override;
    std::vector<std::string> getLuaKeys(const std::string &name) override;
    std::string getKey(const std::string &name) override;

private:
    Mode m_mode;
    std::unique_ptr<DBConnector> m_countersDB;
    std::unique_ptr<DBConnector> m_gbcountersDB;
    std::string m_luaScript;

    std::unique_ptr<DBConnector>& getGbcountersDB();
};

class MacsecCounter: public Counter {
public:
    MacsecCounter(const DBConnector *db);
    ~MacsecCounter() = default;
    std::string getKey(const std::string &name) override;

private:
    std::unique_ptr<DBConnector> m_countersDB;
};

class CounterTable: public TableBase {

public:
    CounterTable(Counter *counter,
                 const DBConnector *db, const std::string &tableName=COUNTERS_TABLE);

public:
    bool get(const std::string &key, std::vector<FieldValueTuple> &values);
    bool hget(const std::string &key, const std::string &field,  std::string &value);

private:
    std::unique_ptr<LuaTable>& getLuaTable();
    std::unique_ptr<Table>& getTable();

    Counter *m_counter;
    std::unique_ptr<DBConnector> m_countersDB;
    std::unique_ptr<LuaTable> m_luaTable;
    std::unique_ptr<Table> m_table;
};

}

#endif
