#pragma once

#include <map>
#include <memory>
#include <vector>

namespace swss
{

class DBConnectorInterface
{
  public:
    virtual ~DBConnectorInterface() = default;

    virtual int64_t del(const std::string &key) = 0;

    virtual void del(const std::vector<std::string> &keys) = 0;

    virtual bool exists(const std::string &key) = 0;

    virtual std::unordered_map<std::string, std::string> hgetall(const std::string &glob) = 0;

    virtual std::vector<std::string> keys(const std::string &glob) = 0;

    virtual void hmset(const std::string &key, const std::vector<std::pair<std::string, std::string>> &values) = 0;

    virtual std::shared_ptr<std::string> hget(const std::string &key, const std::string &field) = 0;
};

} // namespace swss
