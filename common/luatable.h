#pragma once

#include <assert.h>
#include <string>
#include <vector>
#include <queue>
#include <tuple>
#include <utility>
#include <memory>
#include "hiredis/hiredis.h"
#include "dbconnector.h"
#include "redisreply.h"
#include "redisselect.h"
#include "redispipeline.h"
#include "schema.h"
#include "redistran.h"
#include "table.h"


namespace swss {
class LuaTable: public TableBase {
public:
    LuaTable(const DBConnector *db, const std::string &tableName,
             const std::string &lua, const std::vector<std::string> &luaArgv={});
    ~LuaTable();

    bool get(const std::vector<std::string> &luaKeys,
             std::vector<FieldValueTuple> &values);

    bool hget(const std::vector<std::string> &luaKeys,
              const std::string &field, std::string &value);

private:
    std::unique_ptr<DBConnector> m_db;
    std::string m_lua;
    std::vector<std::string> m_luaArgv;
};

}
