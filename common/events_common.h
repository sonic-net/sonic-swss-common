
/* The internal code that caches runtime-IDs could retire upon de-init */
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

using namespace std;
using namespace chrono;

#define ERR_MESSAGE_INVALID -2
#define ERR_OTHER -1

/*
 * Max count of possible concurrent event publishers
 * We maintain a cache of last seen sequence number per publisher.
 * This provides a MAX ceiling for cache.
 * An exiting publisher retires its runtime-ID explicitly.
 * A crashed publisher or event lost for any reason will leave
 * behind the runtime ID. Overtime, these leaked IDs could fill the cache.
 * Hence whenever the cache hits this max, old instances are removed.
 * old instances are identified using time of last publish.
 *
 * In the scenario of too many publisher crashed and an instance that
 * that does not publish for a very long time, could get purged.
 * But crashing publishers is a bigger issue and we need not be
 * perfect in that scenario.
 */
#define MAX_PUBLISHERS_COUNT  1000

#define RET_ON_ERR(res, msg, ...)\
    if (!(res)) {\
        int _e = errno; \
        SWSS_LOG_ERROR(msg, ##__VA_ARGS__); \
        SWSS_LOG_ERROR("last:errno=%d", _e); \
        goto out; }


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
template <typename T> string type_name();

template <class T>
string
type_name()
{
    typedef typename remove_reference<T>::type TR;
    unique_ptr<char, void(*)(void*)> own
        (
            abi::__cxa_demangle(typeid(TR).name(), nullptr,
                nullptr, nullptr),
            free
        );
    string r = own != nullptr ? own.get() : typeid(TR).name();
    if (is_const<TR>::value)
        r += " const";
    if (is_volatile<TR>::value)
        r += " volatile";
    if (is_lvalue_reference<T>::value)
        r += "&";
    else if (is_rvalue_reference<T>::value)
        r += "&&";
    return r;
}


template <class T>
string
get_typename(T &val)
{
    return type_name<decltype(val)>();
}


/* map to human readable str; Useful for error reporting. */
template <typename Map>
string
map_to_str(const Map &m)
{
    stringstream _ss;
    string sep;

    _ss << "{";
    for (const auto elem: m) {
        _ss << sep << "{" << elem.first << "," << elem.second.substr(0,10) << "}";
        if (sep.empty()) {
            sep = ", ";
        }
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
 * events are published as two part zmq message.
 * First part only has the event source, so receivers could
 * filter by source.
 *
 * Second part contains serialized form of map as defined in internal_event_t.
 */
/*
 * This is data going over wire and using cache. So be conservative
 * on names
 */
#define EVENT_STR_DATA "d"
#define EVENT_RUNTIME_ID  "r"
#define EVENT_SEQUENCE "s"
#define EVENT_EPOCH "t"

typedef map<string, string> internal_event_t;

/* Sequence is converted to string in message */
typedef uint32_t sequence_t;
#define SEQUENCE_MAX UINT32_MAX

typedef string runtime_id_t;

/*
 * internal_event_t internal_event_ref = {
 *    { EVENT_STR_DATA, "<json string of event>" },
 *    { EVENT_RUNTIME_ID, "<assigned runtime id of publisher>" },
 *    { EVENT_SEQUENCE, "<sequence number of event>" },
 *    { EVENT_EPOCH, "<epoch time at the point of publish>" } };
 */

/*
 * Control messages could be sent as events with specific
 * prefix "CONTROL_"
 * e.g. CONTROL_DEINIT
 */
#define EVENT_STR_CTRL_PREFIX "CONTROL_"
#define EVENT_STR_CTRL_PREFIX_SZ ((int)sizeof(EVENT_STR_CTRL_PREFIX) - 1)

/* The internal code that caches runtime-IDs could retire upon de-init */
#define EVENT_STR_CTRL_DEINIT "CONTROL_DEINIT"

typedef vector<internal_event_t> internal_events_lst_t;

/* Cache maintains the part 2 of an event as serialized string. */
typedef string event_serialized_t;  // events_data_type_t;
typedef vector<event_serialized_t> event_serialized_lst_t; // events_data_lst_t;


sequence_t str_to_seq(const string s);
string seq_to_str(sequence_t seq);

struct serialization
{
    /*
     * Way to serialize map or vector
     * boost::archive::text_oarchive could be used to archive any struct/class
     * but that class needs some additional support, that declares
     * boost::serialization::access as private friend and couple more tweaks.
     * The std::map & vector inherently supports serialization.
     */
    template <typename Map>
    int
    serialize(const Map& data, string &s)
    {
        s.clear();
        ostringstream _ser_ss;

        try {
            boost::archive::text_oarchive oarch(_ser_ss);
            oarch << data;
            s = _ser_ss.str();
            return 0;
        }
        catch (exception& e) {
            stringstream _ser_ex_ss;

            _ser_ex_ss << e.what() << "map type:" << get_typename(data);
            SWSS_LOG_ERROR("serialize Failed: %s", _ser_ex_ss.str().c_str());
            return ERR_MESSAGE_INVALID;
        }   
    }

    template <typename Map>
    int
    deserialize(const string& s, Map& data)
    {

        try {
            istringstream ss(s);
            boost::archive::text_iarchive iarch(ss);
            iarch >> data;
            return 0;
        }
        catch (exception& e) {
            stringstream _ss_ex;

            _ss_ex << e.what() << "str[0:64]:(" << s.substr(0, 64) << ") data type: "
                << get_typename(data);
            SWSS_LOG_ERROR("deserialize Failed: %s", _ss_ex.str().c_str());
            return ERR_MESSAGE_INVALID;
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

            zmq_getsockopt (sock, ZMQ_RCVMORE, &more, &more_size);

            rc = zmsg_to_map(msg, data);
            RET_ON_ERR(rc == 0, "Failed to deserialize part rc=%d", rc);
            /* read more flag if message read fails to de-serialize */
        }
        else {
            /* override with zmq err */
            rc = zmq_errno();
            if (rc != 11) {
                SWSS_LOG_ERROR("Failure to read part rc=%d", rc);
            }
        }
    out:
        zmq_msg_close(&msg);

        return rc;
    }

       
    template<typename DT>
    int
    zmq_send_part(void *sock, int flag, const DT &data)
    {
        zmq_msg_t msg;

        int rc = map_to_zmsg(data, msg);
        RET_ON_ERR(rc == 0, "Failed to map to zmsg %d", rc);

        rc = zmq_msg_send (&msg, sock, flag);
        if (rc == -1) {
            /* override with zmq err */
            rc = zmq_errno();
            RET_ON_ERR(false, "Failed to send part %d", rc);
        }
        /* zmq_msg_send returns count of bytes sent */
        rc = 0;
    out:
        zmq_msg_close(&msg);
        return rc;
    }

    template<typename P1, typename P2>
    int
    zmq_message_send(void *sock, const P1 &pt1, const P2 &pt2)
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
        int more = 0, rc, rc2 = 0;

        rc = zmq_read_part(sock, flag, more, pt1);

        if (more) {
            /*
             * read second part if more is set, irrespective
             * of any failure. More is set, only if sock is valid.
             */
            rc2 = zmq_read_part(sock, 0, more, pt2);
        }
        RET_ON_ERR((rc == 0) || (rc == 11), "Failure to read part1 rc=%d", rc);
        if (rc2 != 0) {
            rc = rc2;
            RET_ON_ERR(false, "Failed to read part2 rc=%d", rc);
        }
        if (more) {
            rc = -1;
            RET_ON_ERR(false, "Don't expect more than 2 parts, rc=%d", rc);
        }
    out:
        return rc;
    }

};

template <typename Map>
int
serialize(const Map& data, string &s)
{
    auto render = boost::serialization::singleton<serialization>::get_const_instance();

    return render.serialize(data, s);
}

template <typename Map>
int
deserialize(const string& s, Map& data)
{
    auto render = boost::serialization::singleton<serialization>::get_const_instance();

    return render.deserialize(s, data);
}

template<typename P1, typename P2>
int
zmq_message_send(void *sock, const P1 &pt1, const P2 &pt2)
{
    auto render = boost::serialization::singleton<serialization>::get_const_instance();

    return render.zmq_message_send(sock, pt1, pt2);
}

template<typename P1, typename P2>
int
zmq_message_read(void *sock, int flag, P1 &pt1, P2 &pt2)
{
    auto render = boost::serialization::singleton<serialization>::get_const_instance();

    return render.zmq_message_read(sock, flag, pt1, pt2);
}

/* Convert {<key>: < params >tttt a JSON string */
string convert_to_json(const string key, const map_str_str_t &params);

/* Parse JSON string into {<key>: < params >} */
int convert_from_json(const string json_str, string &key, map_str_str_t &params);

/*
 *  Cache drain timeout.
 *
 *  When subscriber's de-init is called, it calls start cache service.
 *  When subscriber init is called, it calls cache stop service.
 *
 *  In either scenario, an entity stops reading and let other start.
 *  The entity that stops may have to read little longer to drain any
 *  events in local ZMQ cache.
 *
 *  This timeout helps with that.
 *  
 *  In case of subscriber de-init, the events read during this period
 *  is given to cache as start-up or initial stock.
 *  In case of init where cache service reads for this period, gives
 *  those as part of cache read and subscriber service will be diligent
 *  about reading the same event from the channel, hence duplicate
 *  for next one second.
 */
#define CACHE_DRAIN_IN_MILLISECS 1000

#endif /* !_EVENTS_COMMON_H */ 
