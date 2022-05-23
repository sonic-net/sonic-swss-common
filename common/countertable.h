
#ifndef __COUNTERTABLE__
#define __COUNTERTABLE__

#include <assert.h>
#include <string>
#include <vector>
#include "table.h"
#include "luatable.h"

namespace swss {
struct Counter;

class CounterTable: public TableBase {
public:
    CounterTable(const DBConnector *db, const std::string &tableName=COUNTERS_TABLE);

public:
    bool get(const Counter &counter, const std::string &key, std::vector<FieldValueTuple> &values);
    bool hget(const Counter &counter, const std::string &key, const std::string &field,  std::string &value);

    const std::unique_ptr<DBConnector>& getCountersDB() const {
        return m_countersDB;
    }
    const std::unique_ptr<DBConnector>& getGbcountersDB() const {
        return m_gbcountersDB;
    }

private:
    std::unique_ptr<DBConnector> m_countersDB;
    std::unique_ptr<DBConnector> m_gbcountersDB;
};

struct Counter {
    typedef std::pair<int, std::string> KeyPair;

    virtual const std::string& getLuaScript() const {
        return defaultLuaScript;
    }
    virtual std::vector<std::string> getLuaArgv() const {
        return {};
    }
    virtual bool usingLuaTable(const CounterTable&, const std::string &name) const {
        return false;
    }
    virtual std::vector<std::string> getLuaKeys(const CounterTable&, const std::string &name) const {
        return {};
    }
    virtual KeyPair getKey(const CounterTable&, const std::string &name) const = 0;
    virtual ~Counter() = default;

private:
    static const std::string defaultLuaScript;
};

class PortCounter: public Counter {
public:
    enum class Mode {all, asic, systemside, lineside};

    PortCounter(Mode mode=Mode::all);
    ~PortCounter() = default;

    const std::string& getLuaScript() const override;
    bool usingLuaTable(const CounterTable&, const std::string &name) const override;
    std::vector<std::string> getLuaKeys(const CounterTable&, const std::string &name) const override;
    KeyPair getKey(const CounterTable&, const std::string &name) const override;

private:
    Mode m_mode;
    std::string m_luaScript;
};

class MacsecCounter: public Counter {
public:
    MacsecCounter() = default;
    ~MacsecCounter() = default;
    KeyPair getKey(const CounterTable&, const std::string &name) const override;
};

}

#endif
