#include "events_service.h"

/*
 * Cache management
 *
 *  1)` Caller expected to call init first, which initiates the connection
 *      to the capture end point. Being async, it would take some milliseconds
 *      to connect.
 *
 *  2)  Caller starts the cache, optionally with some local cache it may have.
 *      The cache service keeps it as its startup/initial stock.
 *      This helps the caller saves his local cache with cache service.
 *
 *  3)  Caller call stops, upon it making connect to XPUB end.
 *      As caller's connect is async and also this zmq end may have some cache
 *      of events by ZMQ locally. So read events little longer.
 *
 *  4)  Upon stop, the caller may read cached events.
 *      The events are provided in FIFO order. 
 *      As cached events can be too many, the service returns a few at a time.
 *      The caller is expected to read repeatedly until no event is returned.
 *
 *  Cache overflow:
 *      A ceil is set and may run out of memory, before ceil is reached.
 *      In either case, the caching is *not* completely stopped but cached as
 *      one event per runtime-id/publishing instance. This info is required
 *      to compute missed message count due to overflow and otherwise.
 */

int
event_service::init_client(void *zmq_ctx, int block_ms)
{
    int rc = -1;

    void *sock = zmq_socket (zmq_ctx, ZMQ_REQ);
    RET_ON_ERR(sock != NULL, "Failed to get ZMQ_REQ socket rc=%d", rc);

    rc = zmq_connect (sock, get_config(REQ_REP_END_KEY).c_str());
    RET_ON_ERR(rc == 0, "Failed to connect to %s", get_config(REQ_REP_END_KEY).c_str());
    
    // Set read timeout.
    //
    rc = zmq_setsockopt (sock, ZMQ_RCVTIMEO, &block_ms, sizeof (block_ms));
    RET_ON_ERR(rc == 0, "Failed to ZMQ_RCVTIMEO to %d", block_ms);

    m_socket = sock;
    sock = NULL;
out:
    if (sock != NULL) {
        zmq_close(sock);
    }
    return rc;
}

int
event_service::init_server(void *zmq_ctx, int block_ms)
{
    int rc = -1;

    void *sock = zmq_socket (zmq_ctx, ZMQ_REP);
    RET_ON_ERR(sock != NULL, "Failed to get ZMQ_REP socket rc=%d", rc);

    rc = zmq_bind (sock, get_config(REQ_REP_END_KEY).c_str());
    RET_ON_ERR(rc == 0, "Failed to bind to %s", get_config(REQ_REP_END_KEY).c_str());
    
    // Set read timeout.
    //
    rc = zmq_setsockopt (sock, ZMQ_RCVTIMEO, &block_ms, sizeof (block_ms));
    RET_ON_ERR(rc == 0, "Failed to ZMQ_RCVTIMEO to %d", block_ms);

    m_socket = sock;
    sock = NULL;
out:
    if (sock != NULL) {
        zmq_close(sock);
    }
    return rc;
}


int
event_service::echo_send(const string s)
{
    event_serialized_lst_t l = { s };

    return channel_write(EVENT_ECHO, l);

}


int
event_service::echo_receive(string &outs)
{
    event_serialized_lst_t l;
    int code;

    int rc = channel_read(code, l);
    RET_ON_ERR(rc == 0, "failing to read echo rc=%d", rc);

    RET_ON_ERR (code == 0, "echo receive resp %d not 0", code);
    RET_ON_ERR (l.size() == 1, "echo received resp size %d is not 1",
            (int)l.size());

    outs = l[0];
out:
    return rc;
}


int
event_service::cache_init()
{
    int rc = send_recv(EVENT_CACHE_INIT);
    if (rc == 0) {
        /* To shadow subscribe connect required for cache init */
        send_recv(EVENT_ECHO);
    }
    return rc;
}


int
event_service::cache_start(const event_serialized_lst_t &lst)
{
    int rc;

    RET_ON_ERR((rc = send_recv(EVENT_CACHE_START, &lst)) == 0,
                "Failed to send cache start rc=%d", rc);
out:
    return rc;
}


int
event_service::cache_stop()
{
    int rc;

    RET_ON_ERR((rc = send_recv(EVENT_CACHE_STOP)) == 0,
                "Failed to send cache stop rc=%d", rc);
out:
    return rc;
}


int
event_service::cache_read(event_serialized_lst_t &lst)
{
    int rc;

    RET_ON_ERR((rc = send_recv(EVENT_CACHE_READ, NULL, &lst)) == 0,
                "Failed to send cache read rc=%d", rc);
out:
    return rc;
}


int
event_service::channel_read(int &code, event_serialized_lst_t &data)
{
    event_serialized_lst_t().swap(data);
    return zmq_message_read(m_socket, 0, code, data);
}


int
event_service::channel_write(int code, const event_serialized_lst_t &data)
{
    return zmq_message_send(m_socket, code, data);
}


int
event_service::send_recv(int code, const event_serialized_lst_t *lst_in,
        event_serialized_lst_t *lst_out)
{
    event_serialized_lst_t l;
    int resp;

    if(lst_in == NULL) {
        lst_in = &l;
    }

    int rc = channel_write(code, *lst_in);
    RET_ON_ERR(rc == 0, "failing  to send code=%d", code);

    if (lst_out == NULL) {
        lst_out = &l;
    }
    rc = channel_read(resp, *lst_out);
    RET_ON_ERR(rc == 0, "failing to read resp for code=%d", code);

    rc = resp;
    RET_ON_ERR (rc == 0, "receive resp %d not 0 for code=%d", resp, code);

out:
    return rc;
}


void
event_service::close_service()
{
    if (m_socket != NULL) { zmq_close(m_socket); m_socket = NULL; }
}

