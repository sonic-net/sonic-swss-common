#include <thread>
#include "eventd.h"

/*
 * There are 3 threads, including the main
 *
 * main thread -- Runs eventd service that accepts commands event_req_type_t
 *  This can be used to control caching events and a no-op echo service.
 *
 * capture/cache service 
 *  Saves all the events between cache start & stop
 *
 * Main proxy service that runs XSUB/XPUB ends
 */

#define READ_SET_SIZE 100


eventd_server::eventd_server() : m_capture(NULL)
{
    m_ctx = zmq_ctx_new();
    RET_ON_ERR(m_ctx != NULL, "Failed to get zmq ctx");
out:
    return;
}


eventd_server::~eventd_server()
{
    close();
}


int
eventd_server::zproxy_service()
{
    int ret = -1;
    SWSS_LOG_INFO("Start xpub/xsub proxy");

    void *frontend = zmq_socket(m_ctx, ZMQ_XSUB);
    RET_ON_ERR(frontend != NULL, "failing to get ZMQ_XSUB socket");

    int rc = zmq_bind(frontend, get_config(XSUB_END_KEY));
    RET_ON_ERR(rc == 0, "Failing to bind XSUB to %s", get_config(XSUB_END_KEY));

    void *backend = zmq_socket(m_ctx, ZMQ_XPUB);
    RET_ON_ERR(backend != NULL, "failing to get ZMQ_XPUB socket");

    rc = zmq_bind(backend, get_config(XPUB_END_KEY));
    RET_ON_ERR(rc == 0, "Failing to bind XPUB to %s", get_config(XPUB_END_KEY));

    void *capture = zmq_socket(m_ctx, ZMQ_PUB);
    RET_ON_ERR(capture != NULL, "failing to get ZMQ_XSUB socket");

    rc = zmq_bind(capture, get_config(CAPTURE_END_KEY));
    RET_ON_ERR(rc == 0, "Failing to bind PAIR to %s", get_config(PAIR_END_KEY));

    m_thread_proxy = thread(&eventd_server::zproxy_service_run, this, frontend, 
            backend, capture);
    ret = 0;
out:
    return ret;
}


void
eventd_server::zproxy_service_run(void *frontend, void *backend, void *capture)
{
    SWSS_LOG_INFO("Running xpub/xsub proxy");

    /* runs forever until zmq context is terminated */
    zmq_proxy(frontend, backend, capture);

    zmq_close(frontend);
    zmq_close(backend);
    zmq_close(capture);

    SWSS_LOG_ERR("Terminating xpub/xsub proxy");

    return 0;
}


int
eventd_server::capture_events()
{
    /* clean any pre-existing cache */
    int ret = -1;

    vector<strings>().swap(m_events);
    map<runtime_id_t, string>.swap(m_last_events);

    RET_ON_ERR(m_capture != NULL, "capture sock is not initialized yet");

    while(true) {
        zmq_msg_t msg;
        internal_event_t event;
        int more = 0;
        size_t more_size = sizeof (more);

        {
            zmq_msg_t pat;
            zmq_msg_init(&pat);
            RET_ON_ERR(zmq_msg_recv(&pat, m_capture, 0) != -1,
                    "Failed to capture pattern");
            zmq_msg_close(&pat);
        }

        RET_ON_ERR(zmq_getsockopt (m_capture, ZMQ_RCVMORE, &more, &more_size) == 0,
                        "Failed to get sockopt for capture sock");
        RET_ON_ERR(more, "Event data expected, but more is false");

        zmq_msg_init(&msg);
        RET_ON_ERR(zmq_msg_recv(&msg, m_capture, 0) != -1,
                "Failed to read event data");

        string s((const char *)zmq_msg_data(&msg), zmq_msg_size(&msg));
        zmq_msg_close(&msg);

        deserialize(s, event);

        m_last_events[event[EVENT_RUNTIME_ID]] = s;

        try
        {
            m_events.push_back(s);
        }
        catch (exception& e)
        {
            stringstream ss;
            ss << e.what();
            SWSS_LOG_ERROR("Cache save event failed with %s events:size=%d",
                    ss.str().c_str(), m_events.size());
            goto out;
        }
    }
out:
    /* Destroy the service and exit the thread */
    close();
    return 0;
}


int
eventd_server::eventd_service()
{
    event_service service;

    RET_ON_ERR(zproxy_service() == 0, "Failed to start zproxy_service");

    RET_ON_ERR(service.init_server(m_ctx) == 0, "Failed to init service");

    while(true) {
        int code, resp = -1; 
        vector<events_cache_type_t> req_data, resp_data;

        RET_ON_ERR(channel_read(code, data) == 0,
                "Failed to read request");

        switch(code) {
            case EVENT_CACHE_START:
                if (m_capture != NULL) {
                    resp_code = 1;
                    break;
                }
                m_capture = zmq_socket(m_ctx, ZMQ_SUB);
                RET_ON_ERR(capture != NULL, "failing to get ZMQ_XSUB socket");

                rc = zmq_connect(capture, get_config(CAPTURE_END_KEY));
                RET_ON_ERR(rc == 0, "Failing to bind PAIR to %s", get_config(PAIR_END_KEY));

                rc = zmq_setsockopt(sub_read, ZMQ_SUBSCRIBE, "", 0);
                RET_ON_ERR(rc == 0, "Failing to ZMQ_SUBSCRIBE");

                /* Kick off the service */
                m_thread_capture = thread(&eventd_server::capture_events, this);

                resp_code = 0;
                break;

                
            case EVENT_CACHE_STOP:
                resp_code = 0;
                if (m_capture != NULL) {
                    close(m_capture);
                    m_capture = NULL;

                    /* Wait for thread to end */
                    m_thread_capture.join();
                }
                break;


            case EVENT_CACHE_READ:
                resp_code = 0;

                if (m_events.empty()) {
                    for (last_events_t::iterator it = m_last_events.begin();
                            it != m_last_events.end(); ++it) {
                        m_events.push_back(it->second);
                    }
                    last_events_t().swap(m_last_events);
                }

                int sz = m_events.size() < READ_SET_SIZE ? m_events.size() : READ_SET_SIZE;

                auto it = std::next(m_events.begin(), sz);
                move(m_events.begin(), m_events.end(), back_inserter(resp_data));

                if (sz == m_events.size()) {
                    events_data_lst_t().swap(m_events);
                } else {
                    m_events.erase(m_events.begin(), it);
                }
                break;


            case EVENT_ECHO:
                resp_code = 0;
                resp_data.swap(req_data);

            default:
                SWSS_LOG_ERROR("Unexpected request: %d", code);
                assert(false);
                break;
        }
        RET_ON_ERR(channel_write(resp_code, resp_data) == 0,
                "Failed to write response back");
    }
out:
    /* Breaks here on fatal failure */
    if (m_capture != NULL) {
        close(m_capture);
        m_capture = NULL;
    }
    close();
    m_thread_proxy.join();
    m_thread_capture.join();
    return 0;
}



void eventd_server::close()
{
    zmq_ctx_term(m_ctx); m_ctx = NULL;

}

