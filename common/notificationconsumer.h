#ifndef __NOTIFICATIONCONSUMER__
#define __NOTIFICATIONCONSUMER__

#include <string>
#include <vector>
#include <queue>
#include <memory>
#include <unordered_map>
#include <list>
#include <chrono>
#include <cinttypes>

#include <hiredis/hiredis.h>

#include "dbconnector.h"
#include "json.h"
#include "logger.h"
#include "redisreply.h"
#include "selectable.h"
#include "table.h"

namespace swss {

static constexpr size_t DEFAULT_NC_POP_BATCH_SIZE = 2048;

class NotificationConsumer : public Selectable
{
public:
    class Queue
    {
    public:
        virtual ~Queue() = default;
        virtual void push(const std::string& value) = 0;
        virtual void pop() = 0;
        virtual const std::string& front() const = 0;
        virtual size_t size() const noexcept = 0;
        virtual bool empty() const noexcept = 0;
    };

    class StandardQueue : public Queue
    {
        std::queue<std::string> m_queue;
    public:
        void push(const std::string& value) override { m_queue.push(value); }
        void pop() override { m_queue.pop(); }
        const std::string& front() const override
        {
            assert(!m_queue.empty());
            return m_queue.front();
        }
        size_t size() const noexcept override { return m_queue.size(); }
        bool empty() const noexcept override { return m_queue.empty(); }
    };

    class DedupQueue : public Queue
    {
        std::list<std::string> m_list;
        std::unordered_map<std::string, std::list<std::string>::iterator> m_map;
        uint64_t m_dupCount = 0;
        uint64_t m_totalPush = 0;
        std::chrono::steady_clock::time_point m_lastLog = std::chrono::steady_clock::now();

    public:
        void push(const std::string& value) override
        {
            logStats();
            m_totalPush++;
            auto it = m_map.find(value);
            if (it != m_map.end())
            {
                m_dupCount++;
                // Move the existing entry to the back of the queue. Note that
                // this reorders events that were already queued: the duplicate
                // is delivered in the position of the most recent occurrence
                // rather than the original one.
                m_list.splice(m_list.end(), m_list, it->second);
                return;
            }
            m_list.push_back(value);
            m_map[value] = std::prev(m_list.end());
        }
        void pop() override
        {
            assert(!m_list.empty());
            m_map.erase(m_list.front());
            m_list.pop_front();
        }
        const std::string& front() const override
        {
            assert(!m_list.empty());
            return m_list.front();
        }
        size_t size() const noexcept override { return m_list.size(); }
        bool empty() const noexcept override { return m_list.empty(); }
    private:
        void logStats()
        {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastLog).count();
            if (elapsed >= 60)
            {
                SWSS_LOG_DEBUG("DedupQueue stats: queue_size=%zu, total_push=%" PRIu64 ", duplicates_skipped=%" PRIu64,
                        m_list.size(), m_totalPush, m_dupCount);
                m_lastLog = now;
                m_totalPush = 0;
                m_dupCount = 0;
            }
        }
    };

    NotificationConsumer(swss::DBConnector *db, const std::string &channel, int pri = 100,
                         size_t popBatchSize = DEFAULT_NC_POP_BATCH_SIZE,
                         std::shared_ptr<Queue> queue = std::make_shared<StandardQueue>());

    // Pop one or multiple data from the internal queue which fed from redis socket
    // Note:
    //    Ensure data ready before popping, either by select or peek
    void pop(std::string &op, std::string &data, std::vector<FieldValueTuple> &values);
    void pops(std::deque<KeyOpFieldsValuesTuple> &vkco);

    // Check the internal queue which fed from redis socket for data ready
    // Returns:
    //     1 - data immediately available inside internal queue, may be just fed from redis socket
    //     0 - no data both in internal queue or redis socket
    //    -1 - error during peeking redis socket
    int peek();

    ~NotificationConsumer() override;

    int getFd() override;
    uint64_t readData() override;
    bool hasData() override;
    bool hasCachedData() override;
    const size_t POP_BATCH_SIZE;

private:

    NotificationConsumer(const NotificationConsumer &other);
    NotificationConsumer& operator = (const NotificationConsumer &other);

    void processReply(redisReply *reply);
    void subscribe();

    swss::DBConnector *m_db;
    swss::DBConnector *m_subscribe;
    std::string m_channel;
    std::shared_ptr<Queue> m_queue;
};

}

#endif // __NOTIFICATIONCONSUMER__

