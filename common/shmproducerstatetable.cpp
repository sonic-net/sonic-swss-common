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
#include "json.h"

using namespace std;

using namespace boost::interprocess;

#define MQ_RESPONSE_BUFFER_SIZE (4*1024*1024)
#define MQ_SIZE 100
#define MQ_MAX_RETRY 10
#define MQ_POLL_TIMEOUT (1000)

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
    
    try
    {
        message_queue::remove(m_queueName.c_str());
    }
    catch (const interprocess_exception& e)
    {
        // message queue still use by some other process
        SWSS_LOG_NOTICE("Failed to close a using message queue '%s': %s", m_queueName.c_str(), e.what());
    }
}

void ShmProducerStateTable::initialize()
{
    m_runUpdateTableThread = true;
    m_updateTableThread = std::make_shared<std::thread>(&ShmProducerStateTable::updateTableThreadFunction, this);
    
    // every table can only have 1 queue.
    m_queueName =  m_pipe->getDbName() + "_" + getTableName();
    
    try
    {
        m_queue = std::make_shared<message_queue>(open_or_create,
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
    
    //sendMsg(key, values, op);
}

void ShmProducerStateTable::del(
                    const string &key,
                    const string &op /*= DEL_COMMAND*/,
                    const string &prefix)
{
    auto* operation = new ShmOperationDel(key);
    std::lock_guard<std::mutex> lock(m_operationQueueMutex);
    m_operationQueue.emplace(operation);
    
    //sendMsg(key, vector<FieldValueTuple>(), op);
}

void ShmProducerStateTable::set(const std::vector<KeyOpFieldsValuesTuple>& values)
{
    auto* operation = new ShmOperationBatchSet(values);
    std::lock_guard<std::mutex> lock(m_operationQueueMutex);
    m_operationQueue.emplace(operation);

    /*
    for (const auto &value : values)
    {
        sendMsg(kfvKey(value), kfvFieldsValues(value), SET_COMMAND);
    }
    */
}

void ShmProducerStateTable::del(const std::vector<std::string>& keys)
{
    auto* operation = new ShmOperationBatchDel(keys);
    std::lock_guard<std::mutex> lock(m_operationQueueMutex);
    m_operationQueue.emplace(operation);

    /*
    for (const auto &key : keys)
    {
        sendMsg(key, vector<FieldValueTuple>(), DEL_COMMAND);
    }
    */
}

void ShmProducerStateTable::updateTableThreadFunction()
{
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
                    ProducerStateTable::set(key, values);
                    break;
                }
                
                case ShmOperationType::DEL:
                {
                    auto* del = (const ShmOperationDel*)operation.get();
                    ProducerStateTable::del(del->m_key);
                    break;
                }
                
                case ShmOperationType::BATCH_SET:
                {
                    auto* batch = (const ShmOperationBatchSet*)operation.get();
                    ProducerStateTable::set(batch->m_values);
                    break;
                }
                
                case ShmOperationType::BATCH_DEL:
                {
                    auto* batch = (const ShmOperationBatchDel*)operation.get();
                    ProducerStateTable::del(batch->m_keys);
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
        
        ProducerStateTable::flush();
        
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

    for (int i = 0; true ; ++i)
    {
        if (m_queue->get_num_msg() >= m_queue->get_max_msg())
        {
            SWSS_LOG_DEBUG("message queue %s is full, retry later.", m_queueName.c_str());
            sleep(MQ_POLL_TIMEOUT);
        }

        try
        {
            m_queue->send(msg.c_str(), msg.length(), 0);
            return;
        }
        catch (const interprocess_exception& e)
        {
            if (i >= MQ_MAX_RETRY)
            {
                SWSS_LOG_THROW("message queue %s send failed: %s",
                        m_queueName.c_str(),
                        e.what());
            }
        }
    }

    SWSS_LOG_THROW("message queue %s send failed after retry.",
            m_queueName.c_str());
}

}
