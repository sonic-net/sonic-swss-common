#pragma once

#include <string>
#include <deque>
#include <condition_variable>
#include "dbconnector.h"
#include "table.h"
#include "consumertablebase.h"
#include "selectableevent.h"

#define MQ_RESPONSE_MAX_COUNT (4*1024*1024)
#define MQ_SIZE 100
#define MQ_MAX_RETRY 10
#define MQ_POLL_TIMEOUT (1000)

namespace swss {

class ZmqConsumerStateTable : public Selectable
{
public:
    /* The default value of pop batch size is 128 */
    static constexpr int DEFAULT_POP_BATCH_SIZE = 128;

    ZmqConsumerStateTable(DBConnector *db, const std::string &tableName, const std::string& endpoint, int popBatchSize = DEFAULT_POP_BATCH_SIZE, int pri = 0);
    ~ZmqConsumerStateTable();

    /* Get multiple pop elements */
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco, const std::string &prefix = EMPTY_PREFIX);

    /* return file handler for the Selectable */
    int getFd() override
    {
        return m_selectableEvent.getFd();
    }

    /* Read all data from the fd assicaited with Selectable */
    uint64_t readData() override
    {
        return m_selectableEvent.readData();
    }

    /*
       true if Selectable has data in it for immediate read
       This method is intended to be implemented in subclasses of Selectable
       which also implement the hasCachedData() method
       it's possible to have a case, when an application read all cached data after a signal,
       and we have a Selectable with no data in the m_ready queue (Select class).
       The class without hasCachedData never is going to be in m_ready state without the data
    */
    bool hasData() override
    {
        std::lock_guard<std::mutex> lock(m_mqPoolDataQueueMutex);
        return !m_mqPoolDataQueue.empty();
    }

    /* true if Selectable has data in its cache */
    bool hasCachedData() override
    {
        return hasData();
    }

    /* true if Selectable was initialized with data */
    bool initializedWithData() override
    {
        // TODO, if we need load data from redis during initialize, don't forget change code here.
        return false;
    }

    const DBConnector* getDbConnector() const
    {
        return m_db;
    }
    
    std::string getTableName() const
    {
        return m_tableName;
    }

    std::string getTableNameSeparator() const
    {
        return m_tableSeparator;
    }

private:
    void handleReceivedData(const char* buffer, const size_t size);
 
    static inline std::shared_ptr<KeyOpFieldsValuesTuple> deserializeReceivedData(const char* buffer, const size_t size);

    void mqPollThread();

    void dbUpdateThread();

    volatile bool m_runThread;

    std::shared_ptr<std::thread> m_mqPollThread;

    std::mutex m_mqPoolDataQueueMutex;

    std::queue<std::shared_ptr<KeyOpFieldsValuesTuple>> m_mqPoolDataQueue;

    swss::SelectableEvent m_selectableEvent;

    std::shared_ptr<std::thread> m_dbUpdateThread;

    std::mutex m_dbUpdateDataQueueMutex;

    std::condition_variable m_dbUpdateDataNotifyCv;

    std::queue<std::shared_ptr<KeyOpFieldsValuesTuple>> m_dbUpdateDataQueue;

    DBConnector *m_db;
    
    std::string m_tableName;

    std::string m_endpoint;
    
    std::string m_tableSeparator;
};

}
