#include <zmq.h>
#include <unordered_set>
#include "logger.h"
#include "zmqrouteserver.h"

using namespace std;

namespace swss {

void ZmqRouteServer::mqPollThread()
{
    SWSS_LOG_ENTER();
    SWSS_LOG_NOTICE("mqPollThread begin");

    // zmq_poll will use less CPU
    zmq_pollitem_t poll_item;
    poll_item.fd = 0;
    poll_item.socket = m_socket;
    poll_item.events = ZMQ_POLLIN;
    poll_item.revents = 0;

    // Handlers touched in the current (in-progress) burst. We hold notification
    // until the burst quiesces — no new data for BURST_QUIESCE_MS — so the orch
    // main loop wakes once per "really finished" burst rather than after every
    // EAGAIN. Note: the ingress callback may also fire notifyPending() mid-burst
    // when m_toSync crosses gMaxBulkSize; eventfd writes coalesce, so the
    // redundancy is harmless.
    std::unordered_set<ZmqMessageHandler*> dirtyHandlers;

    // Idle long-poll vs post-burst short-poll timeouts.
    constexpr long IDLE_POLL_MS = 1000;
    constexpr long BURST_QUIESCE_MS = 5;

    SWSS_LOG_NOTICE("bind to zmq endpoint: %s", m_endpoint.c_str());
    while (m_runThread)
    {
        // Use the short timeout while a burst is in progress (dirty handlers
        // pending); otherwise use the long idle timeout.
        const long timeout_ms = dirtyHandlers.empty() ? IDLE_POLL_MS : BURST_QUIESCE_MS;
        auto rc = zmq_poll(&poll_item, 1, timeout_ms);

        if (rc == 0)
        {
            // Poll timed out. If a burst was pending, BURST_QUIESCE_MS has
            // passed without new data — flush it now.
            if (!dirtyHandlers.empty())
            {
                for (auto* handler : dirtyHandlers)
                {
                    handler->notifyPending();
                }
                dirtyHandlers.clear();
            }
            continue;
        }
        if (!(poll_item.revents & ZMQ_POLLIN))
        {
            SWSS_LOG_DEBUG("zmq_poll invalid event rc: %d, revents: %d", rc, poll_item.revents);
            continue;
        }

        // Drain everything available on the socket in this burst (ZMQ_DONTWAIT
        // loop until EAGAIN). ZmqRouteServer is async-only; the oneToOneSync
        // (request/response) mode supported by ZmqServer is intentionally not
        // available here — burst coalescing assumes streaming ingress.
        while (m_runThread)
        {
            rc = zmq_recv(m_socket, m_buffer.data(), MQ_RESPONSE_MAX_COUNT, ZMQ_DONTWAIT);

            if (rc < 0)
            {
                int zmq_err = zmq_errno();
                SWSS_LOG_DEBUG("zmq_recv failed, endpoint: %s,zmqerrno: %d", m_endpoint.c_str(), zmq_err);
                if (zmq_err == EINTR || zmq_err == EAGAIN)
                {
                    // Socket drained (or interrupted). Don't notify yet —
                    // re-enter zmq_poll with the short BURST_QUIESCE_MS
                    // timeout to see if more data follows.
                    break;
                }
                else
                {
                    SWSS_LOG_THROW("zmq_recv failed, endpoint: %s,zmqerrno: %d", m_endpoint.c_str(), zmq_err);
                }
            }

            if (rc >= MQ_RESPONSE_MAX_COUNT)
            {
                SWSS_LOG_THROW("zmq_recv message was truncated (over %d bytes, received %d), increase buffer size, message DROPPED",
                        MQ_RESPONSE_MAX_COUNT,
                        rc);
            }

            m_buffer.at(rc) = 0;
            SWSS_LOG_DEBUG("zmq received %d bytes", rc);

            if (auto* handler = handleReceivedData(m_buffer.data(), rc))
            {
                dirtyHandlers.insert(handler);
            }
        }
        // Leave dirtyHandlers populated; the next zmq_poll will use
        // BURST_QUIESCE_MS, and we'll flush on the rc==0 path above when the
        // burst goes quiet.
    }
    SWSS_LOG_NOTICE("mqPollThread end");
}

}
