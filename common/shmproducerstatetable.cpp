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

using namespace std;

namespace swss {

ShmProducerStateTable::ShmProducerStateTable(DBConnector *db, const string &tableName)
    : ProducerStateTable(db, tableName)
{
    initializeThread();
}

ShmProducerStateTable::ShmProducerStateTable(RedisPipeline *pipeline, const string &tableName, bool buffered)
    : ProducerStateTable(pipeline, tableName, buffered)
{
    initializeThread();
}

ShmProducerStateTable::~ShmProducerStateTable()
{
    m_runUpdateTableThread = false;
}

void ShmProducerStateTable::initializeThread()
{
    m_runUpdateTableThread = true;
    m_updateTableThread = std::make_shared<std::thread>(&ShmProducerStateTable::updateTableThreadFunction, this);
}

void ShmProducerStateTable::set(const string &key, const vector<FieldValueTuple> &values,
                 const string &op /*= SET_COMMAND*/, const string &prefix)
{
    auto* operation = new ShmOperationSet(key, values);
    std::lock_guard<std::mutex> lock(m_operationQueueMutex);
    m_operationQueue.emplace(operation);
}

void ShmProducerStateTable::del(const string &key, const string &op /*= DEL_COMMAND*/, const string &prefix)
{
    auto* operation = new ShmOperationDel(key);
    std::lock_guard<std::mutex> lock(m_operationQueueMutex);
    m_operationQueue.emplace(operation);
}

void ShmProducerStateTable::set(const std::vector<KeyOpFieldsValuesTuple>& values)
{
    auto* operation = new ShmOperationBatchSet(values);
    std::lock_guard<std::mutex> lock(m_operationQueueMutex);
    m_operationQueue.emplace(operation);
}

void ShmProducerStateTable::del(const std::vector<std::string>& keys)
{
    auto* operation = new ShmOperationBatchDel(keys);
    std::lock_guard<std::mutex> lock(m_operationQueueMutex);
    m_operationQueue.emplace(operation);
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

}
