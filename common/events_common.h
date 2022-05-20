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
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include "logger.h"
#include "events.h"

using namespace std;
using namespace chrono;

extern int errno;
extern int zerrno;

/*
 * Max count of possible concurrent event publishers
 * A rough estimate only, more as a guideline than strict.
 * So this does not limit any usage
 */
#define MAX_PUBLISHERS_COUNT  1000

#define RET_ON_ERR(res, msg, ...)\
    if (!(res)) {\
        int e = errno; \
        zerrno = zmq_errno(); \
        string fmt = string("errno:%d zmq_errno:%d ") + msg; \
        SWSS_LOG_ERROR(fmt.c_str(), e, zerrno, ##__VA_ARGS__); \
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
#define CACHE_MAX_CNT "cache_max_cnt"

/* init config from file */
void read_init_config(const char *fname);

/* Provide a key for configurable entity */
template<T>
T get_config_data(const string key, T def)
{
    string s(get_config(key));

    if (s.empty()) {
        return def;
    }
    else {
        T v;
        stringstream ss(s);

        ss >> v;

        return v;
    }
}


string get_config(const string key);

const string get_timestamp();

/*
 * Way to serialize map or vector
 * boost::archive::text_oarchive could be used to archive any struct/class
 * but that class needs some additional support, that declares
 * boost::serialization::access as private friend and couple more tweaks
 * std::map inherently supports serialization
 */
template <typename Map>
const string
serialize(const Map& data)
{
    std::stringstream ss;
    boost::archive::text_oarchive oarch(ss);
    oarch << data;
    return ss.str();
}

template <typename Map>
void
deserialize(const string& s, Map& data)
{
    std::stringstream ss;
    ss << s;
    boost::archive::text_iarchive iarch(ss);
    iarch >> data;
    return;
}


template <typename Map>
int
map_to_zmsg(const Map& data, zmq_msg_t &msg)
{
    string s = serialize(data);

    int rc = zmq_msg_init_size(&msg, s.size());
    if (rc == 0) {
        strncpy((char *)zmq_msg_data(&msg), s.c_str(), s.size());
    }
    return rc;
}


template <typename Map>
void
zmsg_to_map(zmq_msg_t &msg, Map& data)
{
    string s((const char *)zmq_msg_data(&msg), zmq_msg_size(&msg));
    deserialize(s, data);
}


/*
 * events are published as two part zmq message.
 * First part only has the event source, so receivers could
 * filter by source.
 *
 * Second part contains map as defined in internal_event_t.
 * The map is serialized and sent as string.
 *
 */
/*
 * This is data going over wire and using cache. So be conservative
 * on names
 */
#define EVENT_STR_DATA "d"
#define EVENT_RUNTIME_ID  "r"
#define EVENT_SEQUENCE "s"

typedef map<string, string> internal_event_t;

/* Sequence is converted to string in message */
tyepdef uint32_t sequence_t;
tyepdef string runtime_id_t;

internal_event_t internal_event_ref = {
    { EVENT_STR_DATA, "" },
    { EVENT_RUNTIME_ID, "" },
    { EVENT_SEQUENCE, "" } };

/* ZMQ message part 2 contains serialized version of internal_event_t */
typedef string events_data_type_t
typedef vector<events_data_type_t> events_data_lst_t;


template<typename p>
zmq_read_part(void *sock, int flag, int &more, p data)
{
    zmq_msg_t msg;

    more = 0;
    zmq_msg_init(&msg);
    int rc = zmq_msg_recv(&msg, sock, flag);

    if (rc != -1) {
        size_t more_size = sizeof (more);

        zmsg_to_map(msg, data);

        rc = zmq_getsockopt (m_socket, ZMQ_RCVMORE, &more, &more_size);

    }
    zmq_msg_close(&msg);

    return rc;
}

   
template<typename p>
zmq_send_part(void *sock, int flag, p data)
{
    zmq_msg_t msg;

    int rc = map_to_zmsg(data, msg);
    RET_ON_ERR(rc == 0, "Failed to map to zmsg");

    rc = zmq_msg_send (&msg, sock, flag);
    RET_ON_ERR(rc != -1, "Failed to send part");

    rc = 0;
out:
    zmq_msg_close(&msg);
    return rc;
}

template<typename p1, template p2>
int
zmq_message_send(void *sock, p1 pt1, p2 pt2)
{
    int rc = zmq_send_part(sock, pt2.empty() ? 0 : ZMQ_SNDMORE, pt1);

    if ((rc == 0) && (!pt2.empty())) {
        rc = zmq_send_part(sock, 0, pt2);
    }
    return rc;
}

   
template<typename p1, template p2>
int
zmq_message_read(void *sock, int flag, p1 pt1, p2 pt2)
{
    int more = 0, rc;
    
    rc = zmq_read_part(sock, flag, more, pt1);
    RET_ON_ERR(rc == 0, "Failed to read part1");

    if (more) {
        rc = zmq_read_part(sock, 0, more, pt2);
        RET_ON_ERR(rc == 0, "Failed to read part1");
        RET_ON_ERR(!more, "Don't expect more than 2 parts");
    }

out:
    return rc;
}

