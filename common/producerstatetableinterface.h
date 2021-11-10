#pragma once

#include <memory>
#include <vector>

#include "table.h"

namespace swss
{

class ProducerStateTableInterface
{
  public:
    virtual ~ProducerStateTableInterface() = default;

    virtual void set(const std::string &key, const std::vector<FieldValueTuple> &values,
                     const std::string &op = SET_COMMAND, const std::string &prefix = EMPTY_PREFIX) = 0;

    virtual void del(const std::string &key, const std::string &op = DEL_COMMAND,
                     const std::string &prefix = EMPTY_PREFIX) = 0;

    virtual std::string get_table_name() const = 0;
};

} // namespace swss
