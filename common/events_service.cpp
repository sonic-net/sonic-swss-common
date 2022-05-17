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
event_service::channel_read(int &code, vector<string> &data)
{
    int more = 0, rc;
    size_t more_size = sizeof (more);

    {
        zmq_msg_t rcv_code;

        zmq_msg_init(&rcv_code);
        rc = zmq_msg_recv(&rcv_code, m_req_socket, 0);
       
        RET_ON_ERR(rc != -1, "Failed to receive code");

        msg_to_int(rcv_code, code);
        zmq_msg_close(&rcv_code);
    }

    rc =  zmq_getsockopt (m_socket, ZMQ_RCVMORE, &more, &more_size);
    RET_ON_ERR(rc == 0, "Failed to get sockopt for  read channel");


    if (more) {
        zmq_msg_t rcv_data;

        zmq_msg_init(&rcv_data);
        rc = zmq_msg_recv(&rcv_data, m_req_socket, 0); 
        RET_ON_ERR(rc != -1, "Failed to receive data");

        rc =  zmq_getsockopt (m_socket, ZMQ_RCVMORE, &more, &more_size);
        RET_ON_ERR(rc == 0, "Failed to get sockopt for  read channel");
        RET_ON_ERR(!more, "Expecting more than 2 parts");

        msg_to_vec(rcv_data, data);
        zmq_msg_close(&rcv_data);
    }
out:
    reurn rc;
}


int
event_service::channel_write(int code, vector<string> &data)
{
    zmq_msg_t msg_req, msg_data;
    int flag = 0;

    int rc = int_to_msg(code,msg_req);
    RET_ON_ERR(rc == 0, "Failed int (%d) to msg", code);

    if (!data.empty()) {
        rc = vec_to_msg(code, msg_data);
        RET_ON_ERR(rc == 0, "Failed vec (%d) to msg", data.size());
        flag = ZMQ_SNDMORE;
    }

    rc = zmq_msg_send (&msg_req, m_socket, flag);
    RET_ON_ERR(rc == 0, "Failed to send code");

    if (flag != 0) {
        rc = zmq_msg_send (&msg_data, m_socket, 0);
        RET_ON_ERR(rc == 0, "Failed to send data");
    }

out:
    zmq_msg_close(&msg_req);
    zmq_msg_close(&msg_data);
    return rc;
}

int
event_service::close_service()
{
    if (m_socket = NULL) { zmq_close(m_socket); m_socket = NULL; }
    return 0;
}


