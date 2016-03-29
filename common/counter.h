#ifndef __REDIS_COUNTER__
#define __REDIS_COUNTER__

#include <string>
#include <queue>
#include <tuple>
#include "hiredis/hiredis.h"
#include "dbconnector.h"
#include "redisreply.h"
#include "scheme.h"

namespace swss {

class Counter {
public:
    Counter(DBConnector *db, std::string counterName);

    int64_t get();
    void set(int64_t value);
    int64_t incr();
    int64_t decr();


protected:

    std::string formatGet();
    std::string formatSet(int64_t);
    std::string formatIncr();
    std::string formatDecr();

    DBConnector *m_db;
    std::string m_counterName;
};

}

#endif
