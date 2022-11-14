#pragma once

#include <string>
#include <deque>
#include "dbconnector.h"
#include "consumertablebase.h"

#include <boost/interprocess/ipc/message_queue.hpp>

namespace swss {

class ShmConsumerStateTable : public Selectable
{
public:
    ShmConsumerStateTable(DBConnector *db, const std::string &tableName, int popBatchSize = DEFAULT_POP_BATCH_SIZE, int pri = 0);
    ~ShmConsumerStateTable();

    /* Get multiple pop elements */
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco, const std::string &prefix = EMPTY_PREFIX);

    void pop(KeyOpFieldsValuesTuple &kco, const std::string &prefix = EMPTY_PREFIX);

    void pop(std::string &key, std::string &op, std::vector<FieldValueTuple> &fvs, const std::string &prefix = EMPTY_PREFIX);

    bool empty() const { return m_buffer.empty(); };
    
    /* return file handler for the Selectable */
    virtual int getFd() = 0;

    /* Read all data from the fd assicaited with Selectable */
    virtual uint64_t readData() = 0;

    /*
       true if Selectable has data in it for immediate read
       This method is intended to be implemented in subclasses of Selectable
       which also implement the hasCachedData() method
       it's possible to have a case, when an application read all cached data after a signal,
       and we have a Selectable with no data in the m_ready queue (Select class).
       The class without hasCachedData never is going to be in m_ready state without the data
    */
    virtual bool hasData()
    {
        return true;
    }

    /* true if Selectable has data in its cache */
    virtual bool hasCachedData()
    {
        return false;
    }

    /* true if Selectable was initialized with data */
    virtual bool initializedWithData()
    {
        return false;
    }

    /* run this function after every read */
    virtual void updateAfterRead()
    {
    }

    virtual int getPri() const
    {
        return m_priority;
    }
private:
    void mqPollThread();

    std::string m_queueName;

    std::shared_ptr<boost::interprocess::message_queue> m_queue;

    volatile bool m_runThread;

    std::shared_ptr<std::thread> m_mqPollThread;

    swss::SelectableEvent m_selectableEvent;

    std::mutex m_dataQueueMutex;

    std::queue<std::string> m_dataQueue;
};

}
