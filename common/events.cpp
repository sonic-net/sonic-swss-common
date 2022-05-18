#include "events_pi.h"

lst_publishers_t EventPublisher::s_publishers = lst_publishers_t();

EventPublisher::EventPublisher(const string source) :
    m_zmq_ctx(NULL), m_socket(NULL), m_sequence(0), m_init(false)
{
    uuuid_clear(m_runtime_id);

}

int EventPublisher::init(const string event_source, int block_ms)
{
    m_zmq_ctx = zmq_ctx_new();
    m_socket = zmq_socket (m_zmq_ctx, ZMQ_PUB);

    int rc = zmq_connect (m_socket, get_config(XSUB_END_KEY));
    RET_ON_ERR(rc == 0, "Publisher fails to connect %s", get_config(XSUB_END_KEY));

    // REQ socket is connected and a message is sent & received, more to
    // ensure PUB socket had enough time to establish connection.
    // Any message published before connection establishment is dropped.
    //
    event_service m_event_svc;
    rc = m_event_svc.init_client(block_ms);
    RET_ON_ERR (rc == 0, "Failed to init event service");

    rc = m_event_svc.echo_send("hello");
    RET_ON_ERR (rc == 0, "Failed to echo send in event service");

    zmq_msg_init_size(&m_zmsg_source, event_source.size());
    memcpy((char *)zmq_msg_data(&m_zmsg_source), event_source, event_source.size());

    m_event_source(event_source);

    uuid_t id;
    uuid_generate(id);
    uuid_unparse(id, m_runtime_id);

    m_init = true;
out:
    return m_init ? 0 : -1;
}

EventPublisher::~EventPublisher()
{
    zmq_msg_close(&m_zmsg_source);
    zmq_close(m_socket);
    zmq_close(m_echo_socket);
    zmq_ctx_term(m_zmq_ctx);
}


void
EventPublisher::event_publish(const string tag, const event_params_t *params)
{
    zmq_msg_t msg;

    if (m_event_service.is_active()) {
        string s;

        /* Failure is no-op; The eventd service my be down
         * NOTE: This call atmost blocks for block_ms milliseconds
         * as provided in publisher init.
         */
        m_event_service.echo_receive(s);
    }

    string param_str;
    if ((params != NULL) && (param->find(event_ts) != params->end())) {

        param_str = serialize(*params);
    }
    else {
        event_params_t evt_params = *params;
        evt_params[event_ts] = get_timestamp();
        param_str = serialize(evt_params);
    }

    map_str_str_t event_str = { { m_event_source + ":" + tag, param_str}};

    ++m_sequence;
    internal_event_t event_data;
    event_data[EVENT_STR_DATA] = serialize(event_str);
    {
        stringstream ss;

        ss << m_sequence;

        event_data[EVENT_SEQUENCE] = ss.str();
    }
    event_data[EVENT_RUNTIME_ID] = m_runtime_id;

    RET_ON_ERR(map_to_zmsg(event_data, msg) == 0,
            "Failed to buildmsg data size=%d", event_data.size());

    // Send the message
    // First part -- The event-source/pattern
    // Second part -- Metadata
    // Third part -- Params, if given
    //
    RET_ON_ERR(zmq_msg_send (&m_zmsg_source, m_socket, ZMQ_SNDMORE) != -1,
            "Failed to publish part 1 to %s", XSUB_PATH);
    RET_ON_ERR(zmq_msg_send (&msg, m_socket, 0) != -1,
            "Failed to publish part 2 to %s", XSUB_PATH);
out:
    zmq_msg_close(&msg_metadata);

}

event_handle_t
events_init_publisher(const string event_source, int block_millisecs)
{
    event_handle_t ret = NULL;
    lst_publishers_t::iterator it = s_publishers.find(event_source);
    if (it != s_publishers.end()) {
        // Pre-exists
        ret = it->second;
    }
    else {
        EventPublisher *p =  new EventPublisher();

        int rc = p->init(event_source, block_millisecs);

        if (rc != 0) {
            delete p;
        }
        else {
            ret = p;
            s_publishers[key] = ret;
        }
    }
    return ret;
}

void
events_deinit_publisher(event_handle_t &handle)
{
    lst_publishers_t::iterator it;
    EventPublisher *pub = dynamic_cast<EventPublisher *>(&handle);

    for(it=s_publishers.begin(); it != s_publishers.end(); ++it) {
        if (it->second == handle) {
            break;
        }
    }

    if (it != s_publishers.end() {
        s_publishers.erase(it);

        delete pub;
    }
    handle = NULL;

}

void
event_publish(event_handle_t handle, const string tag, const event_params_t *params)
{
    EventPublisher *pub = dynamic_cast<EventPublisher *>(handle);

    for(it=s_publishers.begin(); it != s_publishers.end(); ++it) {
        if (it->second == handle) {
            pub->event_publish(tag, params);
            break;
        }
    }
}






