/*
 * common APIs used by events code.
 */
#include <chrono>
#include <fstream>
#include <thread>
#include "string.h"
#include "json.hpp"
#include "zmq.h"
#include <unordered_map>

#include "logger.h"
#include "events.h"

using namespace std;
using namespace chrono;

#define RET_ON_ERR(res, ...) {\
    if (!(res)) \
        SWSS_LOG_ERROR(__VA_ARGS__); }
        goto out;

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
#define PAIR_END_KEY "pair_path"
#define STATS_UPD_SECS "stats_upd_secs"

/* init config from file */
void read_init_config(const char *fname);

/* Provide a key for configurable entity */
string get_config(const string key);

const string get_timestamp();

const string serialize(const map_str_str_t & data);

void deserialize(const string& s, map_str_str_t & data);

void map_to_zmsg(const map_str_str_t & data, zmq_msg_t &msg);

void zmsg_to_map(zmq_msg_t &msg, map_str_str_t & data);

