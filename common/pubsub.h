#pragma once
#include <map>

#include "dbconnector.h"
#include "redisselect.h"

namespace swss {

// This class is to emulate python redis-py class PubSub
// After SWIG wrapping, it should be used in the same way
class PubSub : protected DBConnector//, protected RedisSelect
{
public:
    PubSub(const DBConnector& other);

    using DBConnector::psubscribe; //(const std::string &pattern);

    std::map<std::string, std::string> get_message();
};

}
