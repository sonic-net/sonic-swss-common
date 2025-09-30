#pragma once

#include <string>
#include <deque>
#include <list>
#include <condition_variable>
#include "dbconnector.h"
#include "table.h"

#define MQ_SIZE 100
#define MQ_MAX_RETRY 10
#define MQ_POLL_TIMEOUT (1000)

namespace swss {

class AsyncDBUpdater
{
public:
    AsyncDBUpdater(DBConnector *db, const std::string &tableName);
    ~AsyncDBUpdater();

    void update(std::shared_ptr<KeyOpFieldsValuesTuple> pkco);

    size_t queueSize();
private:
    void dbUpdateThread();

    volatile bool m_runThread;

    std::shared_ptr<std::thread> m_dbUpdateThread;

    std::mutex m_dbUpdateDataQueueMutex;

    std::condition_variable m_dbUpdateDataNotifyCv;

    std::queue<std::shared_ptr<KeyOpFieldsValuesTuple>, std::list<std::shared_ptr<KeyOpFieldsValuesTuple>>> m_dbUpdateDataQueue;

    DBConnector *m_db;

    std::string m_tableName;
};

}
