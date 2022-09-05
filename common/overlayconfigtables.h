#pragma once

#include <assert.h>
#include <string>
#include <queue>
#include <tuple>
#include <utility>
#include <map>
#include <memory>
#include <deque>
#include "table.h"
#include "subscriberstatetable.h"

namespace swss {

class OverlayTable : public Table  
{
public:
    OverlayTable(const DBConnector *db, const std::string &tableName);
    OverlayTable(RedisPipeline *pipeline, const std::string &tableName, bool buffered);
    ~OverlayTable() override;

    /* Get all the field-value tuple of the table entry with the key */
    bool get(const std::string &key, std::vector<std::pair<std::string, std::string>> &ovalues) override;

    bool hget(const std::string &key, const std::string &field,  std::string &value) override;

    /* get all the keys in the table */
    void getKeys(std::vector<std::string> &keys) override;

    /* Set an entry in the DB directly and configure ttl for it (op not in use) */
    void set(const std::string &key,
                     const std::vector<FieldValueTuple> &values, 
                     const std::string &op,
                     const std::string &prefix,
                     const int64_t &ttl) override;

    /* Delete an entry in the table */
    void del(const std::string &key,
                     const std::string &op = "",
                     const std::string &prefix = EMPTY_PREFIX) override;

    void hset(const std::string &key,
                     const std::string &field,
                     const std::string &value,
                     const std::string &op = "",
                     const std::string &prefix = EMPTY_PREFIX) override;
};

}
