#include "events_pi.h"

/*
 *  Track created publishers to avoid duplicates
 *  As receivers track missed event count by publishing instances, avoiding
 *  duplicates helps reduce load.
 */

typedef map <string, EventPublisher *> lst_publishers_t;
lst_publishers_t s_publishers;

EventPublisher::EventPublisher() :
    m_zmq_ctx(NULL), m_socket(NULL), m_sequence(0)
{
}

int EventPublisher::init(const string event_source)
{
    m_zmq_ctx = zmq_ctx_new();
    void *sock = zmq_socket (m_zmq_ctx, ZMQ_PUB);

    int rc = zmq_connect (sock, get_config(XSUB_END_KEY).c_str());
    RET_ON_ERR(rc == 0, "Publisher fails to connect %s", get_config(XSUB_END_KEY).c_str());

    /*
     * Event service could be down. So have a timeout.
     * 
     */
    rc = m_event_service.init_client(m_zmq_ctx, EVENTS_SERVICE_TIMEOUT_MS_PUB);
    RET_ON_ERR (rc == 0, "Failed to init event service");

    /*
     * REQ socket is connected and a message is sent & received, more to
     * ensure PUB socket had enough time to establish connection.
     * Any message published before connection establishment is dropped.
     * NOTE: We don't wait for response here, but read it upon first publish
     * If the publisher init happened early at the start by caller, by the
     * the first event is published, the echo response will be available locally.
     */
    rc = m_event_service.echo_send("hello");
    RET_ON_ERR (rc == 0, "Failed to echo send in event service");

    m_event_source = event_source;

    {
    uuid_t id;
    char uuid_str[UUID_STR_SIZE];
    uuid_generate(id);
    uuid_unparse(id, uuid_str);
    m_runtime_id = string(uuid_str);
    }

    m_socket = sock;
out:
    if (m_socket == NULL) {
        zmq_close(sock);
    }
    return rc;
}

EventPublisher::~EventPublisher()
{
    m_event_service.close_service();
    if (m_socket != NULL) {
        zmq_close(m_socket);
    }
    zmq_ctx_term(m_zmq_ctx);
}


int
EventPublisher::publish(const string tag, const event_params_t *params)
{
    int rc;
    internal_event_t event_data;

    if (m_event_service.is_active()) {
        string s;

        /* Failure is no-op; The eventd service my be down
         * NOTE: This call atmost blocks for EVENTS_SERVICE_TIMEOUT_MS_PUB
         * as provided in publisher init.
         */
        m_event_service.echo_receive(s);

        /* Close it as we don't need it anymore */
        m_event_service.close_service();
    }

    /* Check for timestamp in params. If not, provide it. */
    string param_str;
    event_params_t evt_params;
    if (params != NULL) {
        if (params->find(event_ts_param) == params->end()) {
            evt_params = *params;
            evt_params[event_ts_param] = get_timestamp();
            params = &evt_params;
        }
    }
    else {
        evt_params[event_ts_param] = get_timestamp();
        params = &evt_params;
    }

    rc = serialize(*params, param_str);
    RET_ON_ERR(rc == 0, "failed to serialize params %s",
            map_to_str(*params).c_str());

    {
    map_str_str_t event_str_map = { { m_event_source + ":" + tag, param_str}};

    rc = serialize(event_str_map, event_data[EVENT_STR_DATA]);
    RET_ON_ERR(rc == 0, "failed to serialize event str %s", 
            map_to_str(event_str_map).c_str());
    }
    event_data[EVENT_RUNTIME_ID] = m_runtime_id;
    ++m_sequence;
    event_data[EVENT_SEQUENCE] = seq_to_str(m_sequence);

    rc = zmq_message_send(m_socket, m_event_source, event_data);
    RET_ON_ERR(rc == 0, "failed to send for tag %s", tag.c_str());
out:
    return rc;
}

event_handle_t
events_init_publisher(const string event_source)
{
    event_handle_t ret = NULL;
    lst_publishers_t::iterator it = s_publishers.find(event_source);
    if (it != s_publishers.end()) {
        // Pre-exists
        ret = it->second;
    }
    else {
        EventPublisher *p =  new EventPublisher();

        int rc = p->init(event_source);

        if (rc != 0) {
            delete p;
        }
        else {
            ret = p;
            s_publishers[event_source] = p;
        }
    }
    return ret;
}

void
events_deinit_publisher(event_handle_t &handle)
{
    lst_publishers_t::iterator it;

    for(it=s_publishers.begin(); it != s_publishers.end(); ++it) {
        if (it->second == handle) {
            delete it->second;
            s_publishers.erase(it);
            break;
        }
    }
    handle = NULL;

}

int
event_publish(event_handle_t handle, const string tag, const event_params_t *params)
{
    lst_publishers_t::const_iterator itc;
    for(itc=s_publishers.begin(); itc != s_publishers.end(); ++itc) {
        if (itc->second == handle) {
            return itc->second->publish(tag, params);
        }
    }
    return -1;
}


EventSubscriber::EventSubscriber() : m_zmq_ctx(NULL), m_socket(NULL)
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

    if (m_event_service.is_active()) {
        events_data_lst_t events;

        rc = m_event_service.cache_init();
        RET_ON_ERR(rc == 0, "Failed to init the cache");

        /* Shadow the cache init request, as it is async */
        m_event_service.send_recv(EVENT_ECHO);

        /* read for 2 seconds in non-block mode, to drain any local cache */
        chrono::steady_clock::time_point start = chrono::steady_clock::now();
        while(true) {
            string source, evt_str;
            internal_event_t evt_data;

            rc = zmq_message_read(m_socket, ZMQ_DONTWAIT, source, evt_data);
            if (rc == -1) {
                if (zerrno == EAGAIN) {
                    /* Try again after a small pause */
                    this_thread::sleep_for(chrono::milliseconds(10));
                }
                else {
                    break;
                }
            }
            serialize(evt_data, evt_str);
            events.push_back(evt_str);
            chrono::steady_clock::time_point now = chrono::steady_clock::now();
            if (chrono::duration_cast<std::chrono::milliseconds>(now - start).count() >
                    CACHE_DRAIN_IN_MILLISECS)
                break;
        }

        /* Start cache service with locally read events as initial stock */
        RET_ON_ERR(m_event_service.cache_start(events) == 0,
                "Failed to send cache start");
        m_event_service.close_service();
    }
out:
    if (m_socket != NULL) {
        zmq_close(m_socket);
    }
    zmq_ctx_term(m_zmq_ctx);
}


int
EventSubscriber::init(bool use_cache, int recv_timeout,
        const event_subscribe_sources_t *subs_sources)
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

    void *sock = zmq_socket (m_zmq_ctx, ZMQ_SUB);

    int rc = zmq_connect (sock, get_config(XPUB_END_KEY).c_str());
    RET_ON_ERR(rc == 0, "Subscriber fails to connect %s", get_config(XPUB_END_KEY).c_str());

    if ((subs_sources == NULL) || (subs_sources->empty())) {
        rc = zmq_setsockopt(sock, ZMQ_SUBSCRIBE, "", 0);
        RET_ON_ERR(rc == 0, "Fails to set option");
    }
    else {
        for (const auto e: *subs_sources) {
            rc = zmq_setsockopt(sock, ZMQ_SUBSCRIBE, e.c_str(), e.size());
            RET_ON_ERR(rc == 0, "Fails to set option");
        }
    }

    if (recv_timeout != -1) {
        rc = zmq_setsockopt (sock, ZMQ_RCVTIMEO, &recv_timeout, sizeof (recv_timeout));
        RET_ON_ERR(rc == 0, "Failed to ZMQ_RCVTIMEO to %d", recv_timeout);
    }

    if (use_cache) {
        rc = m_event_service.init_client(m_zmq_ctx, EVENTS_SERVICE_TIMEOUT_MS_SUB);
        RET_ON_ERR(rc == 0, "Fails to init the service");

        if (m_event_service.cache_stop() == 0) {
            // Stopped an active cache 
            m_cache_read = true;
        }
    }
    m_socket = sock;
out:
    if (m_socket == NULL) {
        zmq_close(sock);
    }
    return rc;
}


void
EventSubscriber::prune_track()
{
    map<time_t, vector<runtime_id_t> > lst;

    /* Sort entries by last touched time */
    for(const auto e: m_track) {
        lst[e.second.epoch_secs].push_back(e.first);
    }

    /* By default it walks from lowest value / earliest timestamp */
    map<time_t, vector<runtime_id_t> >::const_iterator itc = lst.begin();
    for(; (itc != lst.end()) && (m_track.size() > MAX_PUBLISHERS_COUNT); ++itc) {
        for (const auto r: itc->second) {
            m_track.erase(r);
        }
    }
        
}


int
EventSubscriber::event_receive(string &key, event_params_t &params, int &missed_cnt)
{
    int rc = 0; 
    key.clear();
    event_params_t().swap(params);

    while (key.empty()) {
        internal_event_t event_data;

        if (m_cache_read && m_from_cache.empty()) {
            m_event_service.cache_read(m_from_cache);
            m_cache_read = !m_from_cache.empty();
        }

        if (!m_from_cache.empty()) {

            events_data_lst_t::iterator it = m_from_cache.begin();
            rc = deserialize(*it, event_data);
            m_from_cache.erase(it);
            RET_ON_ERR(rc == 0, "Failed to deserialize message from cache");

        }
        else {
            /* Read from SUBS channel */
            string evt_source;
            rc = zmq_message_read(m_socket, 0, evt_source, event_data);
            RET_ON_ERR(rc == 0, "Failed to read message from sock");
        }

        /* Find any missed events for this runtime ID */
        missed_cnt = 0;
        sequence_t seq = events_base::str_to_seq(event_data[EVENT_SEQUENCE]);
        track_info_t::iterator it = m_track.find(event_data[EVENT_RUNTIME_ID]);
        if (it != m_track.end()) {
            /* current seq - last read - 1 == 0 if none missed */
            missed_cnt = seq - it->second.seq - 1;
            it->second = evt_info_t(seq);
        }
        else {
            if (m_track.size() > (MAX_PUBLISHERS_COUNT + 10)) {
                prune_track();
            }
            m_track[event_data[EVENT_RUNTIME_ID]] = evt_info_t(seq);
        }
        if (missed_cnt >= 0) {
            map_str_str_t ev;

            rc = deserialize(event_data[EVENT_STR_DATA], ev);
            RET_ON_ERR(rc == 0, "Failed to deserialize %s",
                    event_data[EVENT_STR_DATA].substr(0, 32).c_str());

            if (ev.size() != 1) rc = -1;
            RET_ON_ERR(rc == 0, "Expect string (%s) deserialize to one key",
                    event_data[EVENT_STR_DATA].substr(0, 32).c_str());
            
            key = ev.begin()->first;

            rc = deserialize(ev.begin()->second, params);
            RET_ON_ERR(rc == 0, "failed to deserialize params %s",
                    ev.begin()->second.substr(0, 32).c_str());

        }
        else {
            /* negative value implies duplicate; Possibly seen from cache */
        }
    }
out:
    return rc;

}


/* Expect only one subscriber per process */
static EventSubscriber *s_subscriber = NULL;

event_handle_t
events_init_subscriber(bool use_cache, int recv_timeout,
        const event_subscribe_sources_t *sources)
{
    if (s_subscriber == NULL) {
        EventSubscriber *p = new EventSubscriber();

        RET_ON_ERR(p->init(use_cache, recv_timeout, sources) == 0,
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
event_receive(event_handle_t handle, string &key,
        event_params_t &params, int &missed_cnt)
{
    if ((handle == s_subscriber) && (s_subscriber != NULL)) {
        return s_subscriber->event_receive(key, params, missed_cnt);
    }
    return -1;
}

int event_last_error()
{
    return recv_last_err;
}

