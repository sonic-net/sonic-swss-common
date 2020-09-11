#include <stdexcept>

#include "dbconnector.h"
#include "redisclient.h"

namespace swss
{

class DBInterface : public RedisContext
{
public:
    void connnect(int dbId, bool retry = true)
    {
        if (retry)
        {
            throw std::invalid_argument("retry");
        }
        else
        {
            m_dbs.emplace(std::piecewise_construct
                    , std::forward_as_tuple(dbId)
                    , std::forward_as_tuple(dbId, *this));
        }
    }

    void close(int dbId);
    DBConnector *at(int dbId);

private:
    std::unordered_map<int, DBConnector> m_dbs;
    std::unordered_map<int, RedisClient> m_redisClients;
};

}
