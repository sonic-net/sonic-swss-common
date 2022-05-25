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

static bool terminate_thr = false;
static void *zmq_ctx = NULL;

void pub_serve_commands()
{
    event_service service_svr;

    EXPECT_EQ(0, service_svr.init_server(zmq_ctx, 1000));

    while(!terminate_thr) {
        int code, resp;
        events_data_lst_t lst;

        printf("reading ...\n");
        if (0 != service_svr.channel_read(code, lst)) {
            /* check client service status, before blocking on read */
            continue;
        }
        printf("pub serve_commands code=%d lst=%d\n", code, (int)lst.size());
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
    printf("exiting thread\n");
}


void run_sub()
{
    void *mock_sub = zmq_socket (zmq_ctx, ZMQ_SUB);
    string source_read;
    internal_event_t ev_int;

    EXPECT_TRUE(NULL != mock_sub);
    EXPECT_EQ(0, zmq_bind(mock_sub, get_config(XSUB_END_KEY).c_str()));
    EXPECT_EQ(0, zmq_setsockopt(mock_sub, ZMQ_SUBSCRIBE, "", 0));

    printf("waiting to read\n");
    EXPECT_EQ(0, zmq_message_read(mock_sub, 0, source_read, ev_int));
    printf("done reading\n");
    zmq_close(mock_sub);
}

TEST(events, publish)
{
    string evt_source("sonic-events-bgp");
    string evt_tag("bgp-state");
    event_params_t params1({{"ip", "10.10.10.10"}, {"state", "up"}});
    event_params_t params_read;
    string evt_key = evt_source + ":" + evt_tag;
    map_str_str_t read_evt;

    running_ut = 1;

    zmq_ctx = zmq_ctx_new();
    EXPECT_TRUE(NULL != zmq_ctx);

    thread thr(&pub_serve_commands);
    thread thr_sub(&run_sub);

    printf("Calling events_init_publisher path=%s\n", get_config(XSUB_END_KEY).c_str());
    event_handle_t h = events_init_publisher(evt_source);
    EXPECT_TRUE(NULL != h);
    printf("DONE events_init_publisher\n");

    /* Take a pause to allow publish to connect async */
    this_thread::sleep_for(chrono::milliseconds(1000));

    printf("called publish\n");
    EXPECT_EQ(0, event_publish(h, evt_tag, &params1));
    printf("done publish\n");

    /* Don't need event's service anymore */
    terminate_thr = true;

    // EXPECT_EQ(evt_source, source_read);

    thr.join();
    thr_sub.join();
    printf("Joined thread\n");

    events_deinit_publisher(h);

    printf("deinit done; terminating ctx\n");
    zmq_ctx_term(zmq_ctx);
    printf("ctx terminated\n");

}

