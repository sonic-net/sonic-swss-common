#pragma once

#include "swss/dbconnectorinterface.h"
#include "swss/table.h"
#include "gmock/gmock.h"

namespace swss
{

class MockDBConnector final : public DBConnectorInterface
{
  public:
    MOCK_METHOD(int64_t, del, (const std::string &key), (override));

    MOCK_METHOD(void, del, (const std::vector<std::string> &keys), (override));

    MOCK_METHOD(bool, exists, (const std::string &key), (override));

    MOCK_METHOD((std::unordered_map<std::string, std::string>), hgetall, (const std::string &regex), (override));

    MOCK_METHOD(std::vector<std::string>, keys, (const std::string &regex), (override));

    MOCK_METHOD(void, hmset, (const std::string &key, const std::vector<FieldValueTuple> &values), (override));

    MOCK_METHOD(std::shared_ptr<std::string>, hget, (const std::string &key, const std::string &field), (override));
};

} // namespace swss
