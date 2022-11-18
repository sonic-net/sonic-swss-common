#include <stdlib.h>
#include <tuple>
#include <sstream>
#include <utility>
#include <algorithm>
#include <chrono>
#include "redisreply.h"
#include "table.h"
#include "redisapi.h"
#include "redispipeline.h"
#include "shmproducerstatetable.h"
#include "shmconsumerstatetable.h"
#include "json.h"

#include <boost/interprocess/ipc/message_queue.hpp>

using namespace std;

using namespace boost::interprocess;

namespace swss {

ShmProducerStateTable::ShmProducerStateTable(DBConnector *db, const string &tableName)
    : ProducerStateTable(db, tableName)
{
    initialize();
}

ShmProducerStateTable::ShmProducerStateTable(RedisPipeline *pipeline, const string &tableName, bool buffered)
    : ProducerStateTable(pipeline, tableName, buffered)
{
    initialize();
}

ShmProducerStateTable::~ShmProducerStateTable()
{
    m_runUpdateTableThread = false;
    m_updateTableThread->join();
    
    ShmConsumerStateTable::TryRemoveShmQueue(m_queueName);

    delete (message_queue*)m_queue;
}
    
void ShmProducerStateTable::initialize()
{
    m_runUpdateTableThread = true;
    m_updateTableThread = std::make_shared<std::thread>(&ShmProducerStateTable::updateTableThreadFunction, this);
    
    // every table can only have 1 queue.
    m_queueName =  ShmConsumerStateTable::GetQueueName(m_pipe->getDbName(), getTableName());
    
    try
    {
        m_queue = new message_queue(open_or_create,
                                       m_queueName.c_str(),
                                       MQ_SIZE,
                                       MQ_RESPONSE_BUFFER_SIZE);
    }
    catch (const interprocess_exception& e)
    {
        SWSS_LOG_THROW("failed to open or create main message queue %s: %s",
                m_queueName.c_str(),
                e.what());
    }
}

void ShmProducerStateTable::set(
                    const string &key,
                    const vector<FieldValueTuple> &values,
                    const string &op /*= SET_COMMAND*/,
                    const string &prefix)
{
    auto* operation = new ShmOperationSet(key, values);
    std::lock_guard<std::mutex> lock(m_operationQueueMutex);
    m_operationQueue.emplace(operation);
    
    sendMsg(key, values, op);
}

void ShmProducerStateTable::del(
                    const string &key,
                    const string &op /*= DEL_COMMAND*/,
                    const string &prefix)
{
    auto* operation = new ShmOperationDel(key);
    std::lock_guard<std::mutex> lock(m_operationQueueMutex);
    m_operationQueue.emplace(operation);
    
    sendMsg(key, vector<FieldValueTuple>(), op);
}

void ShmProducerStateTable::set(const std::vector<KeyOpFieldsValuesTuple>& values)
{
    auto* operation = new ShmOperationBatchSet(values);
    std::lock_guard<std::mutex> lock(m_operationQueueMutex);
    m_operationQueue.emplace(operation);

    for (const auto &value : values)
    {
        sendMsg(kfvKey(value), kfvFieldsValues(value), SET_COMMAND);
    }
}

void ShmProducerStateTable::del(const std::vector<std::string>& keys)
{
    auto* operation = new ShmOperationBatchDel(keys);
    std::lock_guard<std::mutex> lock(m_operationQueueMutex);
    m_operationQueue.emplace(operation);

    for (const auto &key : keys)
    {
        sendMsg(key, vector<FieldValueTuple>(), DEL_COMMAND);
    }
}

void ShmProducerStateTable::updateTableThreadFunction()
{
    DBConnector db(m_pipe->getDbName(), 0, true);
    ProducerStateTable table(&db, getTableName());

    while (m_runUpdateTableThread)
    {
        while (!m_operationQueue.empty())
        {
            const std::shared_ptr<ShmOperationItem>& operation = m_operationQueue.front();
            
            switch (operation->m_type)
            {
                case ShmOperationType::SET:
                {
                    auto* set = (const ShmOperationSet*)operation.get();
                    auto& key = kfvKey(set->m_values);
                    auto& values = kfvFieldsValues(set->m_values);
                    table.set(key, values);
                    break;
                }
                
                case ShmOperationType::DEL:
                {
                    auto* del = (const ShmOperationDel*)operation.get();
                    table.del(del->m_key);
                    break;
                }
                
                case ShmOperationType::BATCH_SET:
                {
                    auto* batch = (const ShmOperationBatchSet*)operation.get();
                    table.set(batch->m_values);
                    break;
                }
                
                case ShmOperationType::BATCH_DEL:
                {
                    auto* batch = (const ShmOperationBatchDel*)operation.get();
                    table.del(batch->m_keys);
                    break;
                }
                
                default:
                {
                    SWSS_LOG_THROW("Unknown operation %d in m_operationQueue for table %s.", operation->m_type, getTableName().c_str());
                }
            }
            
            
            // release operation after process finished
            {
                std::lock_guard<std::mutex> lock(m_operationQueueMutex);
                m_operationQueue.pop();
            }
        }
        
        table.flush();
        
        sleep(1);
    }
}

void ShmProducerStateTable::sendMsg(
        const std::string& key,
        const std::vector<swss::FieldValueTuple>& values,
        const std::string& command)
{
    std::vector<swss::FieldValueTuple> copy = values;
    swss::FieldValueTuple opdata(key, command);
    copy.insert(copy.begin(), opdata);
    std::string msg = JSon::buildJson(copy);

    SWSS_LOG_DEBUG("sending: %s", msg.c_str());
    for (int i = 0; i <=  MQ_MAX_RETRY; ++i)
    {
        try
        {
            // retry when send failed because timeout
            if (((message_queue*)m_queue)->timed_send(msg.c_str(),
                            msg.length(),
                            0,
                            boost::posix_time::ptime(microsec_clock::universal_time()) + boost::posix_time::milliseconds(MQ_POLL_TIMEOUT)))
            {
                return;
            }
        }
        catch (const interprocess_exception& e)
        {
            if (i >= MQ_MAX_RETRY)
            {
                SWSS_LOG_ERROR("message queue %s send failed: %s",
                        m_queueName.c_str(),
                        e.what());
            }
        }
    }

    // message send failed because timeout
    SWSS_LOG_ERROR("message queue %s send failed because timeout.", m_queueName.c_str());
}

}
