
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

static const std::string nullkey = "null";

class KeyCache {
private:
    std::function<void (const CounterTable& t)>  m_cachingFunc;
    std::unordered_map<std::string, std::string> m_keyMap;
    bool m_enabled;
    KeyCache (const KeyCache&) = delete;

public:
    KeyCache(const std::function<void (const CounterTable& t)> &f)
        :m_cachingFunc(f), m_enabled(false) {
    }

    bool enabled() const {
        return m_enabled;
    }

    void enable(const CounterTable& t) {
        if(!m_enabled)
        {
            refresh(t);
            m_enabled = true;
        }
    }

    void disable() {
        if(m_enabled)
        {
            m_keyMap.clear();
            m_enabled = false;
        }
    }

    bool empty() const {
        return m_keyMap.empty();
    }

    void clear() {
        m_keyMap.clear();
    }

    const std::string& at(const std::string &name) const {
        try {
            return m_keyMap.at(name);
        }
        catch (const std::out_of_range& oor) {
            return nullkey;
        }
    }

    template <class InputIterator>
    void add(InputIterator first, InputIterator last) {
        m_keyMap.insert(first, last);
    }

    void refresh(const CounterTable& t) {
        m_keyMap.clear();
        m_cachingFunc(t);
    }
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
    virtual KeyCache& keyCacheInstance(void) const = 0;
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
    KeyCache& keyCacheInstance(void) const;

private:
    Mode m_mode;
    std::string m_luaScript;

    static std::unique_ptr<KeyCache> keyCachePtr;
};

class MacsecCounter: public Counter {
public:
    MacsecCounter() = default;
    ~MacsecCounter() = default;
    KeyPair getKey(const CounterTable&, const std::string &name) const override;
};

}

#endif
