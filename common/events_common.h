#ifndef _EVENTS_COMMON_H
#define _EVENTS_COMMON_H
/*
 * common APIs used by events code.
 */
#include <stdio.h>
#include <chrono>
#include <fstream>
#include <errno.h>
#include <cxxabi.h>
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
         SWSS_LOG_ERROR(fmt.c_str(), e, zerrno __VA_OPT__(,) __VA_ARGS__); \
 * So this does not limit any usage
 */
#define MAX_PUBLISHERS_COUNT  1000

extern int running_ut;


/* TODO: Combine two SWSS_LOG_ERROR into one */
#define RET_ON_ERR(res, msg, ...)\
    if (!(res)) {\
        int _e = errno; \
        zerrno = zmq_errno(); \
        SWSS_LOG_ERROR(msg, ##__VA_ARGS__); \
        SWSS_LOG_ERROR("last:errno=%d zerr=%d", _e, zerrno); \
        if (running_ut) { \
            printf(msg, ##__VA_ARGS__); \
            printf("last:errno=%d zerr=%d\n", _e, zerrno); }\
        goto out; }

#define ERR_CHECK(res, ...) {\
    if (!(res)) \
        SWSS_LOG_ERROR(__VA_ARGS__); }

/* helper API to print variable type */
/*
 *Usage:
 *    const int ci = 0;
 *    std::cout << type_name<decltype(ci)>() << '\n';
 *
 *    map<string, string> l;
 *    std::cout << type_name<decltype(l)>() << '\n';
 *
 *    tt_t t;
 *    std::cout << type_name<decltype(t)>() << '\n';
 *    std::cout << type_name<decltype(tt_t)>() << '\n';
 */

template <typename T> std::string type_name();

template <class T>
std::string
type_name()
{
    typedef typename std::remove_reference<T>::type TR;
    std::unique_ptr<char, void(*)(void*)> own
        (
            abi::__cxa_demangle(typeid(TR).name(), nullptr,
                nullptr, nullptr),
            std::free
        );
    std::string r = own != nullptr ? own.get() : typeid(TR).name();
    if (std::is_const<TR>::value)
        r += " const";
    if (std::is_volatile<TR>::value)
        r += " volatile";
    if (std::is_lvalue_reference<T>::value)
        r += "&";
    else if (std::is_rvalue_reference<T>::value)
        r += "&&";
    return r;
}


template <class T>
std::string
get_typename(T &val)
{
    return type_name<decltype(val)>();
}


template <typename Map>
string
map_to_str(const Map &m)
{
    stringstream _ss;
    _ss << "{";
    for (const auto elem: m) {
        _ss << "{" << elem.first << "," << elem.second << "}";
    }
    _ss << "}";
    return _ss.str();
}

// Some simple definitions
//
typedef map<string, string> map_str_str_t;

/*
 * Config that can be read from init_cfg
 */
#define INIT_CFG_PATH "/etc/sonic/init_cfg.json"
#define CFG_EVENTS_KEY "events"

/* configurable entities' keys */
/* zmq proxy's xsub & xpub end points */
#define XSUB_END_KEY "xsub_path"       
#define XPUB_END_KEY "xpub_path"

/* Eventd service end point; All service req/resp occur via this path */
#define REQ_REP_END_KEY "req_rep_path"

/* Internal proxy to service path for capturing events for caching */
#define CAPTURE_END_KEY "capture_path"

#define STATS_UPD_SECS "stats_upd_secs"
#define CACHE_MAX_CNT "cache_max_cnt"

/* init config from file */
void read_init_config(const char *fname);

/* Read config entry for a key */
string get_config(const string key);

template<typename T>
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


const string get_timestamp();

/*
 * Way to serialize map or vector
 * boost::archive::text_oarchive could be used to archive any struct/class
 * but that class needs some additional support, that declares
 * boost::serialization::access as private friend and couple more tweaks
 * std::map inherently supports serialization
 */
template <typename Map>
int
serialize(const Map& data, string &s)
{
    s.clear();
    std::stringstream _ser_ss;
    boost::archive::text_oarchive oarch(_ser_ss);

    try {
        oarch << data;
        s = _ser_ss.str();
        return 0;
    }
    catch (exception& e) {
        stringstream _ser_ex_ss;

        _ser_ex_ss << e.what() << "map type:" << get_typename(data);
        SWSS_LOG_ERROR("serialize Failed: %s", _ser_ex_ss.str().c_str());
        return -1;
    }   
}

template <typename Map>
int
deserialize(const string& s, Map& data)
{
    std::stringstream ss(s);
    boost::archive::text_iarchive iarch(ss);

    try {
        iarch >> data;
        return 0;
    }
    catch (exception& e) {
        stringstream _ss_ex;

        _ss_ex << e.what() << "str[0:32]:" << s.substr(0, 32) << " data type: "
            << get_typename(data);
        SWSS_LOG_ERROR("deserialize Failed: %s", _ss_ex.str().c_str());
        return -1;
    }   
}


template <typename Map>
int
map_to_zmsg(const Map& data, zmq_msg_t &msg)
{
    string s;
    int rc = serialize(data, s);

    if (rc == 0) {
        rc = zmq_msg_init_size(&msg, s.size());
    }
    if (rc == 0) {
        strncpy((char *)zmq_msg_data(&msg), s.c_str(), s.size());
    }
    return rc;
}


template <typename Map>
int
zmsg_to_map(zmq_msg_t &msg, Map& data)
{
    string s((const char *)zmq_msg_data(&msg), zmq_msg_size(&msg));
    return deserialize(s, data);
}


/*
 * events are published as two part zmq message.
 * First part only has the event source, so receivers could
 * filter by source.
 *
 * Second part contains serialized form of map as defined in internal_event_t.
 * The map is serialized and sent as string events_data_type_t.
 * Caching that handles of set of events, handleas ordered events
 * as declared in events_data_lst_t.
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
typedef uint32_t sequence_t;
typedef string runtime_id_t;

/*
 * internal_event_t internal_event_ref = {
 *    { EVENT_STR_DATA, "" },
 *    { EVENT_RUNTIME_ID, "" },
 *    { EVENT_SEQUENCE, "" } };
 */

/* ZMQ message part 2 contains serialized version of internal_event_t */
typedef string events_data_type_t;
typedef vector<events_data_type_t> events_data_lst_t;


template<typename DT>
int 
zmq_read_part(void *sock, int flag, int &more, DT &data)
{
    zmq_msg_t msg;

    more = 0;
    zmq_msg_init(&msg);
    int rc = zmq_msg_recv(&msg, sock, flag);

    if (rc != -1) {
        size_t more_size = sizeof (more);

        rc = zmsg_to_map(msg, data);

        /* read more flag if message read fails to de-serialize */
        zmq_getsockopt (sock, ZMQ_RCVMORE, &more, &more_size);

    }
    zmq_msg_close(&msg);

    return rc;
}

   
template<typename DT>
int
zmq_send_part(void *sock, int flag, DT &data)
{
    zmq_msg_t msg;

    int rc = map_to_zmsg(data, msg);
    RET_ON_ERR(rc == 0, "Failed to map to zmsg %d", rc);

    rc = zmq_msg_send (&msg, sock, flag);
    RET_ON_ERR(rc != -1, "Failed to send part %d", rc);

    rc = 0;
out:
    zmq_msg_close(&msg);
    return rc;
}

template<typename P1, typename P2>
int
zmq_message_send(void *sock, P1 &pt1, P2 &pt2)
{
    int rc = zmq_send_part(sock, pt2.empty() ? 0 : ZMQ_SNDMORE, pt1);

    /* send second part, only if first is sent successfully */
    if ((rc == 0) && (!pt2.empty())) {
        rc = zmq_send_part(sock, 0, pt2);
    }
    return rc;
}

   
template<typename P1, typename P2>
int
zmq_message_read(void *sock, int flag, P1 &pt1, P2 &pt2)
{
    int more = 0, rc, rc1;
    
    rc = zmq_read_part(sock, flag, more, pt1);

    if (more) {
        /*
         * read second part if more is set, irrespective
         * of any failure. More is set, only if sock is valid.
         */
        rc1 = zmq_read_part(sock, 0, more, pt2);
    }
    RET_ON_ERR(rc == 0, "Failed to read part1");
    RET_ON_ERR(rc1 == 0, "Failed to read part2");
    RET_ON_ERR(!more, "Don't expect more than 2 parts");
out:
    return rc;
}

#endif /* !_EVENTS_COMMON_H */ 
