#include <zmq.h>
#include <chrono>
#include "logger.h"
#include "binaryserializer.h"
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
    ZmqHandlerRegistry::DirtyHandlerMap dirtyHandlers;

    // Idle long-poll vs post-burst short-poll timeouts.
    constexpr long IDLE_POLL_MS = 1000;
    constexpr long BURST_QUIESCE_MS = 5;

    // Upper bound on how long a dirty handler may go unnotified while a burst
    // keeps running. The quiesce notify above only fires when the stream
    // pauses for BURST_QUIESCE_MS; a stream that never pauses and never
    // crosses the consumer-side bulk threshold would otherwise defer
    // notification indefinitely. Coalescing keeps the staged state current,
    // so the exposure is delayed convergence, not wrong state -- but a
    // sustained same-key flap is exactly when prompt programming of the
    // latest state matters.
    constexpr long BURST_MAX_HOLDOFF_MS = 50;

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
            //
            // Flush through the registry rather than iterating dirtyHandlers
            // here: these are raw pointers captured up to BURST_QUIESCE_MS
            // ago, and a consumer destroyed in the meantime would have
            // unregistered itself but left its pointer in the set. The
            // registry checks liveness and notifies under the same lock that
            // removeHandler() takes. It also clears the set.
            getHandlerRegistry()->flushDirtyHandlers(
                dirtyHandlers, std::chrono::steady_clock::now());
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
        //
        // One clock read per drain pass rather than per message; emplace keeps
        // the FIRST dirty time on repeat touches, which is what the holdoff
        // below is measured from. Pass-start granularity only makes flushes
        // earlier, never later.
        const auto drainStart = std::chrono::steady_clock::now();
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

            // Deserialize and dispatch here (rather than via the void
            // ZmqServer::handleReceivedData) so we can capture the handler the
            // message was dispatched to and coalesce notifyPending() per burst.
            std::string dbName;
            std::string tableName;
            std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> kcos;
            BinarySerializer::deserializeBuffer(m_buffer.data(), rc, dbName, tableName, kcos);

            if (auto* handler = getHandlerRegistry()->dispatch(dbName, tableName, kcos))
            {
                dirtyHandlers.emplace(handler, drainStart);
            }
        }

        // A continuous stream keeps zmq_poll returning with data, so the
        // rc == 0 quiesce flush above may never run. Flush overdue handlers
        // here, once per drain-to-empty pass: any stream that lets the socket
        // empty momentarily (recv hits EAGAIN) gets notified within about
        // BURST_MAX_HOLDOFF_MS. A producer that keeps the socket continuously
        // non-empty while staying under the gMaxBulkSize threshold can defer
        // this flush for the length of the drain pass; we accept that rather
        // than pay a per-message deadline check on the hot path.
        getHandlerRegistry()->flushDirtyHandlers(
            dirtyHandlers,
            std::chrono::steady_clock::now() - std::chrono::milliseconds(BURST_MAX_HOLDOFF_MS));
        // Leave dirtyHandlers populated; the next zmq_poll will use
        // BURST_QUIESCE_MS, and we'll flush on the rc==0 path above when the
        // burst goes quiet.
    }
    SWSS_LOG_NOTICE("mqPollThread end");
}

}
