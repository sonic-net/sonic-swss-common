#include <iostream>
#include <memory>
#include <thread>
#include <algorithm>
#include <deque>
#include <regex>
#include <chrono>
#include "gtest/gtest.h"
#include "common/events_common.h"
#include "common/events.h"
#include "common/events_pi.h"

using namespace std;

static bool terminate_svc = false;
static bool terminate_sub = false;
static void *zmq_ctx = NULL;

void events_validate_ts(const string s);

void pub_serve_commands()
{
    event_service service_svr;

    EXPECT_EQ(0, service_svr.init_server(zmq_ctx, 1000));

    while(!terminate_svc) {
        int code, resp;
        events_data_lst_t lst;

        if (0 != service_svr.channel_read(code, lst)) {
            /* check client service status, before blocking on read */
            continue;
        }
        switch(code) {
            case EVENT_CACHE_INIT:
                resp = 0;
                lst.clear();
                break;
            case EVENT_CACHE_START:
                resp = 0;
                lst.clear();
                break;
            case EVENT_CACHE_STOP:
                resp = 0;
                lst.clear();
                break;
            case EVENT_CACHE_READ:
                resp = 0;
                lst = { "rerer", "rrrr", "cccc" };
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
}


string read_source;
internal_event_t read_evt;

void run_sub()
{
    void *mock_sub = zmq_socket (zmq_ctx, ZMQ_SUB);
    string source;
    internal_event_t ev_int;
    int block_ms = 200;

    EXPECT_TRUE(NULL != mock_sub);
    EXPECT_EQ(0, zmq_bind(mock_sub, get_config(XSUB_END_KEY).c_str()));
    EXPECT_EQ(0, zmq_setsockopt(mock_sub, ZMQ_SUBSCRIBE, "", 0));
    EXPECT_EQ(0, zmq_setsockopt(mock_sub, ZMQ_RCVTIMEO, &block_ms, sizeof (block_ms, 0)));

    while(!terminate_sub) {
        if (0 == zmq_message_read(mock_sub, 0, source, ev_int)) {
            read_evt.swap(ev_int);
            read_source.swap(source);
        }
    }

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

    /* Pause with timeout for reading published message */
    for(i=0; source.empty() && (i < 20); ++i) {
        this_thread::sleep_for(chrono::milliseconds(10));
    }

    EXPECT_FALSE(source.empty());
    EXPECT_EQ(3, evt.size());

    for (const auto e: evt) {
        if (e.first == EVENT_STR_DATA) {
            map_str_str_t m;
            EXPECT_EQ(0, deserialize(e.second, m));
            EXPECT_EQ(1, m.size());
            key = m.begin()->first;
            EXPECT_EQ(0, deserialize(m.begin()->second, params));
            cout << "EVENT_STR_DATA: " << e.second << "\n";
        }
        else if (e.first == EVENT_RUNTIME_ID) {
            rid = e.second;
            cout << "EVENT_RUNTIME_ID: " << e.second << "\n";
        }
        else if (e.first == EVENT_SEQUENCE) {
            stringstream ss(e.second);
            ss >> seq;
            cout << "EVENT_SEQUENCE: " << seq << "\n";
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



TEST(events, publish)
{
    running_ut = 1;

    string evt_source0("sonic-events-bgp");
    string evt_source1("sonic-events-xyz");
    string evt_tag0("bgp-state");
    string evt_tag1("xyz-state");
    event_params_t params0({{"ip", "10.10.10.10"}, {"state", "up"}});
    event_params_t params1;
    event_params_t::iterator it_param;

    string rid0, rid1;
    sequence_t seq0, seq1 = 0;
    string rd_key0, rd_key1;
    event_params_t rd_params0, rd_params1;

    zmq_ctx = zmq_ctx_new();
    EXPECT_TRUE(NULL != zmq_ctx);

    thread thr(&pub_serve_commands);
    thread thr_sub(&run_sub);

    event_handle_t h = events_init_publisher(evt_source0);
    EXPECT_TRUE(NULL != h);

    /* Take a pause to allow publish to connect async */
    this_thread::sleep_for(chrono::milliseconds(300));

    EXPECT_EQ(0, event_publish(h, evt_tag0, &params0));

    parse_read_evt(read_source, read_evt, rid0, seq0, rd_key0, rd_params0);

    EXPECT_EQ(seq0, 1);
    EXPECT_EQ(rd_key0, evt_source0 + ":" + evt_tag0);

    it_param = rd_params0.find(event_ts_param);
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

    it_param = rd_params1.find(event_ts_param);
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

    it_param = rd_params0.find(event_ts_param);
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

    events_deinit_publisher(h);
    events_deinit_publisher(h1);

    zmq_ctx_term(zmq_ctx);

}

