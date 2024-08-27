#pragma once

#include <string>
#include <deque>
#include <condition_variable>
#include "asyncdbupdater.h"
#include "consumertablebase.h"
#include "dbconnector.h"
#include "selectableevent.h"
#include "table.h"
#include "zmqserver.h"

namespace swss {

class ZmqConsumerStateTable : public Selectable, public TableBase, public ZmqMessageHandler
{
public:
    /* The default value of pop batch size is 128 */
    static constexpr int DEFAULT_POP_BATCH_SIZE = 128;

    ZmqConsumerStateTable(DBConnector *db, const std::string &tableName, ZmqServer &zmqServer, int popBatchSize = DEFAULT_POP_BATCH_SIZE, int pri = 0, bool dbPersistence = false);
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
        std::lock_guard<std::mutex> lock(m_receivedQueueMutex);
        return !m_receivedOperationQueue.empty();
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

    size_t dbUpdaterQueueSize();

private:
    void handleReceivedData(const std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> &kcos);

    std::mutex m_receivedQueueMutex;

    std::queue<std::shared_ptr<KeyOpFieldsValuesTuple>> m_receivedOperationQueue;

    swss::SelectableEvent m_selectableEvent;

    DBConnector *m_db;

    ZmqServer& m_zmqServer;

    std::unique_ptr<AsyncDBUpdater> m_asyncDBUpdater;
};

}
