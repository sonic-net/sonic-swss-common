/*
 * common APIs used by events code.
 */
#include <stdio.h>
#include <chrono>
#include <fstream>
#include <thread>
#include <errno.h>
#include "string.h"
#include "json.hpp"
#include "zmq.h"
#include <unordered_map>

#include "logger.h"
#include "events.h"

using namespace std;
using namespace chrono;

extern int errno;

#define RET_ON_ERR(res, msg, ...)\
    if (!(res)) {\
        int e = errno; \
        int z = zmq_errno(); \
        string fmt = "errno:%d zmq_errno:%d " + msg; \
        SWSS_LOG_ERROR(fmt.c_str(), e, z, ##__VA_ARGS__); \
        goto out; }

#define ERR_CHECK(res, ...) {\
    if (!(res)) \
        SWSS_LOG_ERROR(__VA_ARGS__); }

// Some simple definitions
//
typedef map<string, string> map_str_str_t;

/*
 * Config that can be read from init_cfg
 */
#define INIT_CFG_PATH "/etc/sonic/init_cfg.json"
#define CFG_EVENTS_KEY "events"

/* configurable entities' keys */
#define XSUB_END_KEY "xsub_path"
#define XPUB_END_KEY "xpub_path"
#define REQ_REP_END_KEY "req_rep_path"
#define CAPTURE_END_KEY "capture_path"
#define STATS_UPD_SECS "stats_upd_secs"

/* init config from file */
void read_init_config(const char *fname);

/* Provide a key for configurable entity */
string get_config(const string key);

const string get_timestamp();

const string serialize(const map_str_str_t & data);

void deserialize(const string& s, map_str_str_t & data);

int map_to_zmsg(const map_str_str_t & data, zmq_msg_t &msg);

void zmsg_to_map(zmq_msg_t &msg, map_str_str_t & data);

#define u32_to_s(u32) std::to_string(u32)

#define s_to_u32(s, u32) { stringstream _ss(s); s >> u32;}

