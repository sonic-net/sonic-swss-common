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
    vector<string> l = { s };

    return channel_write(EVENT_ECHO, l);

}


int
event_service::echo_receive(string &outs)
{
    vector<string> l;
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
event_service::cache_start()
{
    int rc = send_recv(EVENT_CACHE_START);
    if (rc == 0) {
        /* To shadow subscribe connect required for cache start */
        send_recv(EVENT_ECHO);
    }
    return rc;
}


int
event_service::cache_stop()
{
    return send_recv(EVENT_CACHE_STOP);
}


int
event_service::cache_read(vector<string> &lst)
{
     return send_recv(EVENT_CACHE_READ, lst);
}


int
event_service::channel_read(int &code, vector<string> &data)
{
    int more = 0, rc;
    size_t more_size = sizeof (more);

    {
        zmq_msg_t rcv_code;

        rc = zmq_msg_recv(&rcv_code, m_req_socket, 0);
        RET_ON_ERR(rc != -1, "Failed to receive code");

        zmsg_to_map(rcv_code, code);
        zmq_msg_close(&rcv_code);
    }

    rc =  zmq_getsockopt (m_socket, ZMQ_RCVMORE, &more, &more_size);
    RET_ON_ERR(rc == 0, "Failed to get sockopt for  read channel");


    if (more) {
        zmq_msg_t rcv_data;

        rc = zmq_msg_recv(&rcv_data, m_req_socket, 0); 
        RET_ON_ERR(rc != -1, "Failed to receive data");

        rc =  zmq_getsockopt (m_socket, ZMQ_RCVMORE, &more, &more_size);
        RET_ON_ERR(rc == 0, "Failed to get sockopt for  read channel");
        RET_ON_ERR(!more, "Expecting more than 2 parts");

        zmsg_to_map(rcv_data, data);
        zmq_msg_close(&rcv_data);
    }
out:
    reurn rc;
}


int
event_service::channel_write(int code, const vector<string> &data)
{
    zmq_msg_t msg_req, msg_data;
    int flag = 0;

    int rc = map_to_zmsg(code,msg_req);
    RET_ON_ERR(rc == 0, "Failed int (%d) to msg", code);

    if (!data.empty()) {
        rc = map_to_zmsg(code, msg_data);
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
event_service::send_recv(int code, vector<string> *p = NULL)
{
    vector<string> l;
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

