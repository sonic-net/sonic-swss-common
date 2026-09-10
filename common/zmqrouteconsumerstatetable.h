#pragma once

#include <functional>
#include <memory>
#include <vector>
#include "zmqrouteserver.h"
#include "zmqconsumerstatetable.h"

namespace swss {

class ZmqRouteConsumerStateTable : public ZmqConsumerStateTable
{
  public:
    using IngressCallback =
      std::function<void(
          const std::vector<std::shared_ptr<swss::KeyOpFieldsValuesTuple>>&)>;

    ZmqRouteConsumerStateTable(
        DBConnector *db,
        const std::string &tableName,
        ZmqRouteServer &zmqServer,
        int pri,
        bool dbPersistence)
    : ZmqConsumerStateTable(
              db, tableName, zmqServer,
              ZmqConsumerStateTable::DEFAULT_POP_BATCH_SIZE,
              pri, dbPersistence) {}

    // Detach from the registry here, not just in the base destructor. Member
    // destruction runs most-derived first, so by the time ~ZmqConsumerStateTable()
    // detaches, m_ingressCallback is already gone — leaving a window in which
    // ZmqRouteServer::mqPollThread could dispatch into handleReceivedData() and
    // read a destroyed std::function. Detaching in this most-derived destructor
    // closes that window: removeHandler() blocks until any in-flight dispatch
    // returns and prevents new ones, while m_ingressCallback is still alive. The
    // base destructor's later detach is then a harmless idempotent no-op.
    ~ZmqRouteConsumerStateTable() override
    {
        detachFromRegistry();
    }

    void setIngressCallback(IngressCallback cb)
    {
      m_ingressCallback = std::move(cb);
    }

    bool hasData() override
    {
        // No internal queue; the ingress callback hands tuples directly to the
        // consumer's m_toSync. The Selectable is "ready" only when the
        // SelectableEvent has been notified (via notifyPending).
        return true;
    }

    bool hasCachedData() override
    {
        return false;
    }

    bool initializedWithData() override
    {
        return false;
    }

    /* Wakes the orch main loop. Called by ZmqServer::mqPollThread after a
     * burst (EAGAIN), and may be called by the ingress callback mid-burst when
     * m_toSync exceeds gMaxBulkSize. */
    void notifyPending() override
    {
      m_selectableEvent.notify();
    }

  private:
    void handleReceivedData(const std::vector<std::shared_ptr<swss::KeyOpFieldsValuesTuple>> &kcos) override;

    IngressCallback m_ingressCallback;
};

}
