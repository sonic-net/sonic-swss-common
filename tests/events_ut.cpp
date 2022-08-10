#include <iostream>
#include <memory>
#include <thread>
#include <algorithm>
#include <deque>
#include <regex>
#include <chrono>
#include "common/json.hpp"
#include "gtest/gtest.h"
#include "common/events_common.h"
#include "common/events.h"
#include "common/events_wrap.h"
#include "common/events_pi.h"

using namespace std;

static void *zmq_ctx = NULL;

int last_svc_code = -1;

void events_validate_ts(const string s);

event_serialized_lst_t lst_cache;

#define ARRAY_SIZE(d) (sizeof(d) / sizeof((d)[0]))

static bool terminate_svc = false;


/* Mock eventd service for cache & echo commands */
void pub_serve_commands()
{
    event_service service_svr;

    EXPECT_TRUE(NULL != zmq_ctx);
    EXPECT_EQ(0, service_svr.init_server(zmq_ctx, 1000));
    while(!terminate_svc) {
        int code, resp;
        event_serialized_lst_t lst;

        if (0 != service_svr.channel_read(code, lst)) {
            /* check client service status, before blocking on read */
            continue;
        }
        // printf("Read code=%d lst=%d\n", code, (int)lst.size());
        last_svc_code = code;
        switch(code) {
            case EVENT_CACHE_INIT:
                resp = 0;
                lst.clear();
                break;
            case EVENT_CACHE_START:
                resp = 0;
                lst_cache.swap(lst);
                lst.clear();
                break;
            case EVENT_CACHE_STOP:
                resp = 0;
                lst.clear();
                break;
            case EVENT_CACHE_READ:
                resp = 0;
                lst.swap(lst_cache);
                break;
            case EVENT_ECHO:
                resp = 0;
                break;
            default:
                EXPECT_TRUE(false);
                resp = -1;
                break;
        }
        EXPECT_EQ(0, service_svr.channel_write(resp, lst));
    }
    service_svr.close_service();
    EXPECT_FALSE(service_svr.is_active());
    terminate_svc = false;
    last_svc_code = -1;
    event_serialized_lst_t().swap(lst_cache);
}


string read_source;
internal_event_t read_evt;

static bool terminate_sub = false;

/* Mock a subscriber for testing publisher APIs */
void run_sub()
{
    void *mock_sub = zmq_socket (zmq_ctx, ZMQ_SUB);
    string source;
    internal_event_t ev_int;
    int block_ms = 200;

    EXPECT_TRUE(NULL != mock_sub);
    EXPECT_EQ(0, zmq_bind(mock_sub, get_config(XSUB_END_KEY).c_str()));
    EXPECT_EQ(0, zmq_setsockopt(mock_sub, ZMQ_SUBSCRIBE, "", 0));
    EXPECT_EQ(0, zmq_setsockopt(mock_sub, ZMQ_RCVTIMEO, &block_ms, sizeof (block_ms)));

    while(!terminate_sub) {
        if (0 == zmq_message_read(mock_sub, 0, source, ev_int)) {
            read_evt.swap(ev_int);
            read_source.swap(source);
        }
    }
    terminate_sub = false;
    zmq_close(mock_sub);
}

string
parse_read_evt(string &source, internal_event_t &evt,
        string &rid, sequence_t &seq, string &key, event_params_t &params)
{
    int i;
    string ret;

    rid.clear();
    seq = 0;
    key.clear();
    event_params_t().swap(params);

    /* Wait for run_sub to reads the published message with timeout. */
    for(i=0; source.empty() && (i < 20); ++i) {
        this_thread::sleep_for(chrono::milliseconds(10));
    }

    EXPECT_FALSE(source.empty());
    EXPECT_EQ(4, evt.size());

    for (const auto e: evt) {
        if (e.first == EVENT_STR_DATA) {
            EXPECT_EQ(0, convert_from_json(e.second, key, params));
            // cout << "EVENT_STR_DATA: " << e.second << "\n";
        }
        else if (e.first == EVENT_RUNTIME_ID) {
            rid = e.second;
            // cout << "EVENT_RUNTIME_ID: " << e.second << "\n";
        }
        else if (e.first == EVENT_SEQUENCE) {
            stringstream ss(e.second);
            ss >> seq;
            // cout << "EVENT_SEQUENCE: " << seq << "\n";
        }
        else if (e.first == EVENT_EPOCH) {
            istringstream iss(e.second);
            uint64_t val;

            iss >> val;
            // cout << "EVENT_EPOCH: " << seq << "\n";
            EXPECT_NE(val, 0);
        }
        else {
            EXPECT_FALSE(true);
        }
    }

    EXPECT_FALSE(rid.empty());
    EXPECT_FALSE(seq == 0);
    EXPECT_FALSE(key.empty());
    EXPECT_FALSE(params.empty());

    /* clear it before call to next read */
    ret.swap(source);
    internal_event_t().swap(evt);

    return ret;
}

void do_test_publish(bool wrap)
{
    {
        /* Direct log messages to stdout */
        string dummy, op("STDOUT");
        swss::Logger::swssOutputNotify(dummy, op);
        swss::Logger::setMinPrio(swss::Logger::SWSS_DEBUG);
    }

    string evt_source0("sonic-events-bgp");
    string evt_source1("sonic-events-xyz");
    string evt_tag0("bgp-state");
    string evt_tag1("xyz-state");
    event_params_t params0({{"ip", "10.10.10.10"}, {"state", "up"}});
    event_params_t params1;
    event_params_t::iterator it_param;
    event_handle_t h;

    string rid0, rid1;
    sequence_t seq0, seq1 = 0;
    string rd_key0, rd_key1;
    event_params_t rd_params0, rd_params1;

    zmq_ctx = zmq_ctx_new();
    EXPECT_TRUE(NULL != zmq_ctx);

    thread thr(&pub_serve_commands);
    thread thr_sub(&run_sub);

    if (wrap) {
        h = events_init_publisher_wrap(evt_source0.c_str());
    }
    else {
        h = events_init_publisher(evt_source0);
    }
    EXPECT_TRUE(NULL != h);

    /* Take a pause to allow publish to connect async */
    this_thread::sleep_for(chrono::milliseconds(300));

    if (wrap) {
        param_C_t params_list[10];
        param_C_t *params_ptr = params_list;
        int i = 0;

        /* fill up list  of params with bare pointers */
        for (it_param = params0.begin(); it_param != params0.end(); ++it_param) {
            params_ptr->name = it_param->first.c_str();
            params_ptr->val = it_param->second.c_str();
            ++params_ptr;
            if (++i >= (int)ARRAY_SIZE(params_list)) {
                EXPECT_TRUE(false);
                break;
            }
        }
        EXPECT_EQ(0, event_publish_wrap(h, evt_tag0.c_str(),
                    params_list, (uint32_t)params0.size()));
    } 
    else {
        EXPECT_EQ(0, event_publish(h, evt_tag0, &params0));
    }

    parse_read_evt(read_source, read_evt, rid0, seq0, rd_key0, rd_params0);

    EXPECT_EQ(seq0, 1);
    EXPECT_EQ(rd_key0, evt_source0 + ":" + evt_tag0);

    it_param = rd_params0.find(EVENT_TS_PARAM);
    EXPECT_TRUE(it_param != rd_params0.end());
    if (it_param != rd_params0.end()) {
        events_validate_ts(it_param->second);
        rd_params0.erase(it_param);
    }

    EXPECT_EQ(rd_params0, params0);

    // Publish second message
    //
    EXPECT_EQ(0, event_publish(h, evt_tag1, &params1));

    parse_read_evt(read_source, read_evt, rid1, seq1, rd_key1, rd_params1);

    EXPECT_EQ(rid0, rid1);
    EXPECT_EQ(seq1, 2);
    EXPECT_EQ(rd_key1, evt_source0 + ":" + evt_tag1);

    it_param = rd_params1.find(EVENT_TS_PARAM);
    EXPECT_TRUE(it_param != rd_params1.end());
    if (it_param != rd_params1.end()) {
        events_validate_ts(it_param->second);
        rd_params1.erase(it_param);
    }

    EXPECT_EQ(rd_params1, params1);


    // Publish using a new source
    //
    event_handle_t h1 = events_init_publisher(evt_source1);
    EXPECT_TRUE(NULL != h1);

    /* Take a pause to allow publish to connect async */
    this_thread::sleep_for(chrono::milliseconds(300));

    EXPECT_EQ(0, event_publish(h1, evt_tag0, &params0));

    parse_read_evt(read_source, read_evt, rid0, seq0, rd_key0, rd_params0);

    EXPECT_NE(rid0, rid1);
    EXPECT_EQ(seq0, 1);
    EXPECT_EQ(rd_key0, evt_source1 + ":" + evt_tag0);

    it_param = rd_params0.find(EVENT_TS_PARAM);
    EXPECT_TRUE(it_param != rd_params0.end());
    if (it_param != rd_params0.end()) {
        events_validate_ts(it_param->second);
        rd_params0.erase(it_param);
    }

    EXPECT_EQ(rd_params0, params0);


    /* Don't need event's service anymore */
    terminate_svc = true;
    terminate_sub = true;

    // EXPECT_EQ(evt_source, source_read);

    thr.join();
    thr_sub.join();

    events_deinit_publisher_wrap(h);
    events_deinit_publisher(h1);

    zmq_ctx_term(zmq_ctx);
    zmq_ctx = NULL;
    printf("************ PUBLISH wrap=%d DONE ***************\n", wrap);
}

TEST(events, publish)
{
    do_test_publish(false);
}

TEST(events, publish_wrap)
{
    do_test_publish(true);
}

typedef struct {
    int id;
    string source;
    string tag;
    string rid;
    string seq;
    event_params_t params;
    uint32_t missed_cnt;
} test_data_t;

internal_event_t create_ev(const test_data_t &data)
{
    internal_event_t event_data;

    event_data[EVENT_STR_DATA] =
        convert_to_json(data.source + ":" + data.tag, data.params);

    event_data[EVENT_RUNTIME_ID] = data.rid;
    event_data[EVENT_SEQUENCE] = data.seq;

    return event_data;
}

/* Mock test data with event parameters and expected missed count */
static const test_data_t ldata[] = {
    {
        0,
        "source0",
        "tag0",
        "guid-0",
        "1",
        {{"ip", "10.10.10.10"}, {"state", "up"}},
        0
    },
    {
        1,
        "source0",
        "tag1",
        "guid-1",
        "100",
        {{"ip", "10.10.27.10"}, {"state", "down"}},
        0
    },
    {
        2,
        "source1",
        "tag2",
        "guid-2",
        "101",
        {{"ip", "10.10.24.10"}, {"state", "down"}},
        0
    },
    {
        3,
        "source0",
        "tag3",
        "guid-1",
        "105",
        {{"ip", "10.10.10.10"}, {"state", "up"}},
        4
    },
    {
        4,
        "source0",
        "tag4",
        "guid-0",
        "2",
        {{"ip", "10.10.20.10"}, {"state", "down"}},
        0
    },
    {
        5,
        "source1",
        "tag5",
        "guid-2",
        "110",
        {{"ip", "10.10.24.10"}, {"state", "down"}},
        8
    },
    {
        6,
        "source0",
        "tag0",
        "guid-0",
        "5",
        {{"ip", "10.10.10.10"}, {"state", "up"}},
        2
    },
    {
        7,
        "source0",
        "tag1",
        "guid-1",
        "106",
        {{"ip", "10.10.27.10"}, {"state", "down"}},
        0
    },
    {
        8,
        "source1",
        "tag2",
        "guid-2",
        "111",
        {{"ip", "10.10.24.10"}, {"state", "down"}},
        0
    },
    {
        9,
        "source0",
        "tag3",
        "guid-1",
        "109",
        {{"ip", "10.10.10.10"}, {"state", "up"}},
        2
    },
    {
        10,
        "source0",
        "tag4",
        "guid-0",
        "6",
        {{"ip", "10.10.20.10"}, {"state", "down"}},
        0
    },
    {
        11,
        "source1",
        "tag5",
        "guid-2",
        "119",
        {{"ip", "10.10.24.10"}, {"state", "down"}},
        7
    },
};

int pub_send_index = 0;
int pub_send_cnt = 0;

/* Mock publisher to test subscriber. Runs in dedicated thread */
void run_pub()
{
    /*
     * Two ends of zmq had to be on different threads.
     * so run it independently
     */
    void *mock_pub;
    mock_pub = zmq_socket (zmq_ctx, ZMQ_PUB);
    EXPECT_TRUE(NULL != mock_pub);
    EXPECT_EQ(0, zmq_bind(mock_pub, get_config(XPUB_END_KEY).c_str()));

    /* Sends pub_send_cnt events from pub_send_index. */
    while (pub_send_cnt >= 0) {
        if (pub_send_cnt == 0) {
            this_thread::sleep_for(chrono::milliseconds(2));
            continue;
        }

        int i;
        for(i=0; i<pub_send_cnt; ++i) {
            string src("some_src");
            internal_event_t evt(create_ev(ldata[pub_send_index+i]));

            EXPECT_EQ(0, zmq_message_send(mock_pub, src, evt));
        }
        pub_send_cnt = 0;
    }
    zmq_close(mock_pub);
    pub_send_cnt = 0;
}


/* Helper API to publish via run_pub running in different thread. */
void pub_events(int index, int cnt)
{
    /* Take a pause to allow publish to connect async */
    this_thread::sleep_for(chrono::milliseconds(200));

    pub_send_index = index;
    pub_send_cnt = cnt;

    /* Take a pause to ensure, subscriber would have got it */
    this_thread::sleep_for(chrono::milliseconds(200));

    /* Verify all messages are sent/published */
    EXPECT_EQ(0, pub_send_cnt);
}


void do_test_subscribe(bool wrap)
{
    int i;
#if 0
    {
        /* Direct log messages to stdout */
        string dummy, op("STDOUT");
        swss::Logger::swssOutputNotify(dummy, op);
        swss::Logger::setMinPrio(swss::Logger::SWSS_DEBUG);
    }
#endif

    /*
     * Events published during subs deinit, which will be provided
     * to events cache as start up cache.
     */
    int index_deinit_cache = 0;   /* index of events published during subs deinit*/
    int cnt_deinit_cache = 3;   /* count of events published during subs deinit*/
    
    /*
     * Events published to active cache.
     */
    int index_active_cache = index_deinit_cache + cnt_deinit_cache;
    int cnt_active_cache = 5;

    /*
     * Events published to receiver
     */
    int overlap_subs = 3;
    EXPECT_TRUE(cnt_active_cache >= overlap_subs);
    int index_subs = index_active_cache + cnt_active_cache - overlap_subs;
    int cnt_subs = ((int)ARRAY_SIZE(ldata)) - index_subs;
    EXPECT_TRUE(cnt_subs > overlap_subs);

    event_handle_t hsub;

    zmq_ctx = zmq_ctx_new();
    EXPECT_TRUE(NULL != zmq_ctx);

    thread thr_svc(&pub_serve_commands);
    thread thr_pub(&run_pub);

    if (wrap) {
        hsub = events_init_subscriber_wrap(true, -1);
    }
    else {
        hsub = events_init_subscriber(true);
    }
    EXPECT_TRUE(NULL != hsub);
    EXPECT_EQ(last_svc_code, EVENT_CACHE_STOP);

    /* Publish messages for deinit to capture */
    pub_events(index_deinit_cache, cnt_deinit_cache);

    if (wrap) {
        events_deinit_subscriber_wrap(hsub);
    } else {
        events_deinit_subscriber(hsub);
    }
    EXPECT_EQ(last_svc_code, EVENT_CACHE_START);
    EXPECT_TRUE(cnt_deinit_cache == (int)lst_cache.size());

    /* Publish messages for cache to capture with overlap */
    /* As we mimic cache service, add to the cache directly */
    for (i = 0; i < cnt_active_cache; ++i) {
        string s;
        internal_event_t evt(create_ev(ldata[index_active_cache+i]));
        serialize(evt, s);
        lst_cache.push_back(s);
    }
    EXPECT_EQ((int)lst_cache.size(), index_subs+overlap_subs);

    /* We publish all events ahead of receive, so set a timeout */
    if (wrap) {
        hsub = events_init_subscriber_wrap(true, 100);
    }
    else {
        hsub = events_init_subscriber(true, 100);
    }
    EXPECT_TRUE(NULL != hsub);
    EXPECT_EQ(last_svc_code, EVENT_CACHE_STOP);

    pub_events(index_subs, cnt_subs);

    for(i=0; true; ++i) {
        string exp_key;
        event_receive_op_t evt;
        int rc;

        if (wrap) {
            char buff[1024];
            event_receive_op_C_t evtc;

            evtc.event_str = buff;
            evtc.event_sz = ARRAY_SIZE(buff);

            rc = event_receive_wrap(hsub, &evtc);
            if (rc == 0) {
                evt.missed_cnt = evtc.missed_cnt;
                evt.publish_epoch_ms = evtc.publish_epoch_ms;
                rc = convert_from_json(evtc.event_str, evt.key, evt.params);
            }
        }
        else {
            rc = event_receive(hsub, evt);
        }

        if (rc != 0) {
            EXPECT_EQ(EAGAIN, rc);
            break;
        }

        EXPECT_EQ(ldata[i].params, evt.params);
        
        exp_key = ldata[i].source + ":" + ldata[i].tag;

        EXPECT_EQ(exp_key, evt.key);

        EXPECT_EQ(ldata[i].missed_cnt, evt.missed_cnt);

        EXPECT_NE(0, evt.publish_epoch_ms);
    }

    EXPECT_EQ(i, (int)ARRAY_SIZE(ldata));

    if (wrap) {
        events_deinit_subscriber_wrap(hsub);
    } else {
        events_deinit_subscriber(hsub);
    }

    /* Don't need event's service anymore */
    terminate_svc = true;
    pub_send_cnt = -1;

    thr_svc.join();
    thr_pub.join();
    zmq_ctx_term(zmq_ctx);
    printf("************ SUBSCRIBE wrap=%d DONE ***************\n", wrap);
}

TEST(events, subscribe)
{
    do_test_subscribe(false);
}


TEST(events, subscribe_wrap)
{
    do_test_subscribe(true);
}


