#pragma once

#include <string>
#include <deque>
#include "dbconnector.h"
#include "table.h"
#include "consumertablebase.h"

#include <boost/interprocess/ipc/message_queue.hpp>

namespace swss {

class ShmConsumerStateTable : public Selectable
{
public:
    /* The default value of pop batch size is 128 */
    static constexpr int DEFAULT_POP_BATCH_SIZE = 128;

    ShmConsumerStateTable(DBConnector *db, const std::string &tableName, int popBatchSize = DEFAULT_POP_BATCH_SIZE, int pri = 0);
    ~ShmConsumerStateTable();

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

private:
    void mqPollThread();

    std::string m_queueName;

    std::shared_ptr<boost::interprocess::message_queue> m_msgQueue;

    volatile bool m_runThread;

    std::shared_ptr<std::thread> m_mqPollThread;

    swss::SelectableEvent m_selectableEvent;

    std::mutex m_dataQueueMutex;

    std::queue<std::string> m_dataQueue;
};

}
