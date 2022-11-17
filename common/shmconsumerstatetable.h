#pragma once

#include <string>
#include <deque>
#include "dbconnector.h"
#include "table.h"
#include "consumertablebase.h"
#include "selectableevent.h"

#define MQ_RESPONSE_BUFFER_SIZE (4*1024*1024)
#define MQ_SIZE 100
#define MQ_MAX_RETRY 10
#define MQ_POLL_TIMEOUT (1000)

namespace swss {

class ShmConsumerStateTable : public Selectable
{
public:
    /* The default value of pop batch size is 128 */
    static constexpr int DEFAULT_POP_BATCH_SIZE = 128;

    ShmConsumerStateTable(DBConnector *db, const std::string &tableName, int popBatchSize = DEFAULT_POP_BATCH_SIZE, int pri = 0);
    ~ShmConsumerStateTable();
    
    static bool TryRemoveShmQueue(const std::string &queueName);
    static std::string GetQueueName(const std::string &dbName, const std::string &tableName);

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
        return 0;
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
        return !m_dataQueue.empty();
    }

    /* true if Selectable has data in its cache */
    bool hasCachedData() override
    {
        return !m_dataQueue.empty();
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
    void mqPollThread();

    std::string m_queueName;

    volatile bool m_runThread;

    std::shared_ptr<std::thread> m_mqPollThread;

    swss::SelectableEvent m_selectableEvent;

    std::mutex m_dataQueueMutex;

    std::queue<std::string> m_dataQueue;

    DBConnector *m_db;
    
    std::string m_tableName;
    
    std::string m_tableSeparator;
};

}
