#include "events_service.h"


int
event_service:init_client(int block_ms)
{
    int rc = -1;

    void *sock = zmq_socket (m_zmq_ctx, ZMQ_REQ);
    RET_ON_ERR(sock != NULL, "Failed to get ZMQ_REQ socket");

    rc = zmq_connect (sock, get_config(REQ_REP_END_KEY));
    RET_ON_ERR(rc == 0, "Failed to connect to %s", get_config(REQ_REP_END_KEY));
    
    // Set read timeout.
    //
    rc = zmq_setsockopt (sock, ZMQ_RCVTIMEO, &block_ms, sizeof (block_ms));
    RET_ON_ERR(rc == 0, "Failed to connect to %s", get_config(REQ_REP_END_KEY));

    m_socket = sock;
out:
    return ret;
}

int
event_service:init_server()
{
    int rc = -1;

    void *sock = zmq_socket (m_zmq_ctx, ZMQ_REP);
    RET_ON_ERR(sock != NULL, "Failed to get ZMQ_REP socket");

    rc = zmq_bind (sock, get_config(REQ_REP_END_KEY));
    RET_ON_ERR(rc == 0, "Failed to bind to %s", get_config(REQ_REP_END_KEY));
    
    m_socket = sock;
out:
    return ret;
}


int
event_service:echo_send(const string s)
{
    events_data_lst_t l = { s };

    return channel_write(EVENT_ECHO, l);

}


int
event_service::echo_receive(string &outs)
{
    events_data_lst_t l;
    int code;

    int rc = channel_read(code, l);
    RET_ON_ERR(rc == 0, "failing to read echo");

    RET_ON_ERR (code == 0, "echo receive resp %d not 0", code);
    RET_ON_ERR (l.size() == 1, "echo received resp size %d is not 1", l.size());

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
event_service::cache_start(events_data_lst_t &lst)
{
    RET_ON_ERR((rc = send_recv(EVENT_CACHE_START, &lst) == 0,
                "Failed to send cache start");
out:
    return rc;
}


int
event_service::cache_stop()
{
    RET_ON_ERR((rc = send_recv(EVENT_CACHE_STOP) == 0,
                "Failed to send cache stop");
out:
    return rc;
}


int
event_service::cache_read(events_data_lst_t &lst)
{
    RET_ON_ERR((rc = send_recv(EVENT_CACHE_READ, &lst) == 0,
                "Failed to send cache read");
out:
    return rc;
}


int
event_service::channel_read(int &code, events_data_lst_t &data)
{
    return zmq_message_read(m_socket, code, data);
}


int
event_service::channel_write(int code, const events_data_lst_t &data)
{
    return zmq_message_send(m_socket, 0, code, data);
}


int
event_service::send_recv(int code, events_data_lst_t *p = NULL)
{
    events_data_lst_t l;
    int resp;

    if(p == NULL) {
        p = &l;
    }

    int rc = channel_write(code, *p);
    RET_ON_ERR(rc == 0, "failing  to send code=%d", code);

    rc = channel_read(resp, *p);
    RET_ON_ERR(rc == 0, "failing to read resp for code=%d", code);

    rc = resp;
    RET_ON_ERR (rc == 0, "echo receive resp %d not 0 for code=%d", resp, code);

out:
    return rc;
}


int
event_service::close_service()
{
    if (m_socket = NULL) { zmq_close(m_socket); m_socket = NULL; }
    return 0;
}

