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
    zmq_msg_t msg_req, msg_data;
    int rc = zmq_msg_init_size(&snd_msg, s.size()+1);
    RET_ON_ERR(rc == 0, "Failed to init zmq msg for %d bytes", s.size()+1);
    memcpy((void *)zmq_msg_data(&snd_msg), s.str(), s.size()+1);
    rc = zmq_msg_send(&snd_msg, m_socket, 0);
    RET_ON_ERR (rc != -1, "Failed to send to %s", get_config(REQ_REP_END_KEY));
out:
    if (rc != 0) {
        close();
    }
    return rc;
}


int
event_service::echo_receive(string &outs, bool block)
{
    zmq_msg_t rcv_msg;
    
    zmq_msg_init(&rcv_msg);

    int rc = zmq_msg_recv(&rcv_msg, m_socket, ZMQ_DONTWAIT);
    int err = zmq_errno();
    RET_ON_ERR (rc != -1,
        "Failed to receive message from echo service err=%d", err);

    outs = string((const char *)zmq_msg_data(&rcv_msg));
out:
    zmq_msg_close(&rcv_msg);
    close();
    return rc;
}


int
event_service::cache_start()
{
    // TODO
    reurn 0;
}


int
event_service::cache_stop()
{
    // TODO
    reurn 0;
}


int
event_service::cache_read(vector<zmq_msg_t> &lst)
{
    // TODO
    reurn 0;
}


int
event_service::server_read(event_req_type_t &req_code, event_service_data_t &data)
{
    // TODO
    reurn 0;
}


int
event_service::server_write(int resp_code, event_service_data_t &data)
{
    // TODO
    reurn 0;
}

int
event_service::close_service()
{
    if (m_socket = NULL) { zmq_close(m_socket); m_socket = NULL; }
    return 0;
}


