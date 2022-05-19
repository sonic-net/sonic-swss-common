#include "events_pi.h"

/*
 *  Track created publishers to avoid duplicates
 *  As we track missed event count by publishing instances, avoiding
 *  duplicates helps reduce load.
 */
typedef map <string, EventPublisher> lst_publishers_t;
lst_publishers_t s_publishers;

EventPublisher::EventPublisher(const string source) :
    m_zmq_ctx(NULL), m_socket(NULL), m_sequence(0), m_init(false)
{
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
    zmq_close(m_socket);
    zmq_ctx_term(m_zmq_ctx);
}


void
EventPublisher::event_publish(const string tag, const event_params_t *params)
{
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
    event_data[EVENT_RUNTIME_ID] = m_runtime_id;
    event_data[EVENT_SEQUENCE] = seq_to_str(m_sequence);

    return zmq_message_send(m_event_source, event_data);
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
            s_publishers[key] = p;
        }
    }
    return ret;
}

void
events_deinit_publisher(event_handle_t &handle)
{
    lst_publishers_t::iterator it;
    EventPublisher *pub = NULL;

    for(it=s_publishers.begin(); it != s_publishers.end(); ++it) {
        if (it->second == handle) {
            delete it->second;
            s_publishers.erase(it);
            break;
        }
    }
    handle = NULL;

}

void
event_publish(event_handle_t handle, const string tag, const event_params_t *params)
{
    for(it=s_publishers.begin(); it != s_publishers.end(); ++it) {
        if (it->second == handle) {
            it->second->event_publish(tag, params);
            break;
        }
    }
}


EventSubscriber::EventSubscriber() : m_zmq_ctx(NULL), m_socket(NULL), m_init(false),
    m_cache_read(false)
{};


EventSubscriber::~EventSubscriber()
{
    /*
     *  Call init cache 
     *  Send & recv echo to ensure cache service made the connect, which is async.
     * 
     *  Read events for 2 seconds in non-block mode to drain the local zmq cache
     *  maintained by zmq lib.
     *
     *  Disconnect the SUBS socket
     *  Send the read events to cache service star, as initial stock.
     */
    int rc = 0;

    if (m_use_cache) {
        events_data_lst_t events;

        rc = m_event_svc.cache_init();
        RET_ON_ERR(rc == 0, "Failed to init the cache");

        /* Shadow the cache init request, as it is async */
        m_event_svc.send_recv(EVENT_ECHO);

        /* read for 2 seconds in non-block mode, to drain any local cache */
        chrono::steady_clock::timepoint start = chrono::steady_clock::now();
        while(true) {
            string source, evt_str;
            rc = zmq_message_read(m_socket, ZMQ_DONTWAIT, source, evt_str);
            if (rc == -1) {
                if (zerrno == EAGAIN) {
                    rc = 0;
                }
                break;
            }
            events.push_back(evt_str);
            if(chrono::steady_clock::now() - start > chrono::seconds(2)) 
                break;
        }

        /* Start cache service with locally read events as initial stock */
        RET_ON_ERR(m_event_svc.cache_start(events) == 0,
                "Failed to send cache start");
    }
out:
    zmq_close(m_socket);
    zmq_ctx_term(m_zmq_ctx);
}


int
EventSubscriber::init(bool use_cache, const event_subscribe_sources_t *subs_sources)
{
    /*
     *  Initiate SUBS connection to XPUB end point.
     *  Send subscribe request
     *
     *  If cache use enabled, buy some time to shadow
     *  the earlier SUBS connect, which is async, before
     *  calling event_service to stop cache
     */
    m_zmq_ctx = zmq_ctx_new();

    m_socket = zmq_socket (m_zmq_ctx, ZMQ_SUB);

    int rc = zmq_connect (m_socket, get_config(XPUB_END_KEY));
    RET_ON_ERR(rc == 0, "Subscriber fails to connect %s", get_config(XPUB_END_KEY));

    if ((subs_sources == NULL) || (subs_sources->empty())) {
        rc = zmq_setsockopt(sub_read, ZMQ_SUBSCRIBE, "", 0);
        RET_ON_ERR(rc == 0, "Fails to set option");
    }
    else {
        for (const auto e: *subs_sources) {
            rc = zmq_setsockopt(sub_read, ZMQ_SUBSCRIBE, e.c_str(), e.size());
            RET_ON_ERR(rc == 0, "Fails to set option");
        }
    }

    if (use_cache) {
        rc = m_event_svc.init_client(m_zmq_ctx);
        RET_ON_ERR(rc == 0, "Fails to init the service");

        /* Shadow the cache SUBS connect request, as it is async */
        m_event_svc.send_recv(EVENT_ECHO);

        if (m_event_svc.cache_stop() == 0) {
            // Stopped an active cache 
            m_cache_read = true;
        }
        m_use_cache = true;
    }
    m_init = true;
out:
    return rc;
}


void
EventSubscriber::prune_track()
{
    map<time_t, vector<runtime_id_t> > lst;

    for(const auto e: m_track) {
        lst[e.second.epoch_secs].push_back(e.first);
    }

    map<time_t, vector<runtime_id_t> >::const_iterator itc = lst.begin();
    for(; (itc != lst.end()) && (m_track.size() > MAX_PUBLISHERS_COUNT); ++itc) {
        for (const auto r: itc->second) {
            m_track.erase(r);
        }
    }
        
}


int
EventSubscriber::event_receive(event_str_t &event, int &missed_cnt)
{
    event.clear();
    while (event.empty()) {
        internal_event_t event_data;
        int rc = 0; 

        if (m_cache_read && m_from_cache.empty()) {
            m_event_svc.cache_read(m_from_cache);
            m_cache_read = !m_from_cache.empty();
        }

        if (!m_from_cache.empty()) {

            events_data_lst_t::iterator it = m_from_cache.begin();
            deserialize(*it, event_data);
            m_from_cache.erase(it);

        }
        else {
            /* Read from SUBS channel */
            string evt_source;
            rc = zmq_message_read(m_socket, 0, evt_source, event_data);
            RET_ON_ERR(rc == 0, "Failed to read message from sock");
        }

        /* Find any missed events for this runtime ID */
        missed_cnt = 0;
        track_info_t::iterator it = m_track.find(event_data[EVENT_RUNTIME_ID]);
        if (it != m_track.end()) {
            sequence_t seq = events_base::str_to_seq(event_data[EVENT_SEQUENCE]);
            /* current seq - last read - 1 == 0 if none missed */
            missed_cnt = seq - it->second.seq - 1;
            it->second = evt_info_t(seq);
        }
        else {
            if (m_track.size() > (MAX_PUBLISHERS_COUNT + 10)) {
                prune_track();
                m_track[event_data[EVENT_RUNTIME_ID] = evt_info_t(seq);
            }
        }
        if (missed_cnt >= 0) {
            event = event_data[EVENT_STR_DATA];
        }
    }
out:
    return rc;

}


/* Expect only one subscriber per process */
EventSubscriber *s_subscriber = NULL;

event_handle_t
events_init_subscriber(bool use_cache=false,
                const event_subscribe_sources_t *sources=NULL)
{
    if (s_subscriber == NULL) {
        EventSubscriber *p = new EventSubscriber();

        RET_ON_ERR(p->init(use_cache, sources) == 0,
                "Failed to init subscriber");

        s_subscriber = p;
    }
out:
    return s_subscriber;
}


void
events_deinit_subscriber(event_handle_t &handle)
{
    if ((handle == s_subscriber) && (s_subscriber != NULL)) {
        delete s_subscriber;
        s_subscriber = NULL;
    }
    handle = NULL;
}



int
event_receive(event_handle_t handle, event_str_t &event, int &missed_cnt)
{
    if ((handle == s_subscriber) && (s_subscriber != NULL)) {
        return s_subscriber->event_receive(event, missed_cnt);
    }
    return -1;
}


