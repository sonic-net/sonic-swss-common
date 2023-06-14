#ifndef _EVENTS_PI_H
#define _EVENTS_PI_H
/*
 * Private header file used by events API implementation.
 * Required to run white box testing via unit tests.
 */
#include <chrono>
#include <ctime>
#include <fstream>
#include <uuid/uuid.h>
#include "string.h"
#include "json.hpp"
#include "zmq.h"
#include <unordered_map>

#include "logger.h"
#include "events.h"
#include "events_common.h"
#include "events_service.h"
#include "events_wrap.h"

using namespace std;


/* Base class for publisher & subscriber */
class events_base
{
    public:
        virtual ~events_base() = default;

};

/*
 * A single instance per sender & source.
 * A sender is very likely to use single source only.
 * There can be multiple sender processes/threads for a single source.
 *
 */
class EventPublisher;
typedef shared_ptr<EventPublisher> EventPublisher_ptr_t;
typedef map <string, EventPublisher_ptr_t> lst_publishers_t;


class EventPublisher : public events_base
{
    static lst_publishers_t s_publishers;

    public:
        virtual ~EventPublisher();

        static event_handle_t get_publisher(const string event_source);
        static void drop_publisher(event_handle_t handle);
        static int do_publish(event_handle_t handle, const string tag,
                const event_params_t *params);

    private:
        EventPublisher();

        int init(const string event_source);

        int publish(const string event_tag,
                const event_params_t *params);
        int send_evt(const string str_data);
        void remove_runtime_id();

        void *m_zmq_ctx;
        void *m_socket;

        /* Event service - Used for echo */
        event_service m_event_service;

        /* Source YANG module for all events published by this instance */
        string m_event_source;

        /* Globally unique instance ID for this publishing instance */
        runtime_id_t m_runtime_id;

        /* A running sequence number for events published by this instance */
        sequence_t m_sequence;
};

/*
 *  Receiver's instance to receive.
 *
 *  An instance could be created as one per consumer, so a slow customer
 *  would have nearly no impact on others.
 *
 */
class EventSubscriber;
typedef shared_ptr<EventSubscriber> EventSubscriber_ptr_t;

class EventSubscriber : public events_base
{
    static EventSubscriber_ptr_t s_subscriber;

    public:
        virtual ~EventSubscriber();

        static event_handle_t get_subscriber(bool use_cache, int recv_timeout,
                const event_subscribe_sources_t *sources);

        static void drop_subscriber(event_handle_t handle);

        static EventSubscriber_ptr_t get_instance(event_handle_t handle);

        int event_receive(event_receive_op_t &);
        int event_receive(event_receive_op_C_t &);
        int event_receive(string &event_str, uint32_t &missed, int64_t &pub_ms);

    private:
        EventSubscriber();

        int init(bool use_cache=false,
                int recv_timeout= -1,
                const event_subscribe_sources_t *subs_sources=NULL);


        void *m_zmq_ctx;
        void *m_socket;

        /* Service to use for cache management & echo */
        event_service m_event_service;

        /*
         * Set to true, if there may be more events to read from cache.
         */
        bool m_cache_read;

        /*
         *  Sequences tracked by sender for a source, or in other
         *  words by runtime id.
         *  Epochtime, helps prune oldest, when cache overflows.
         */
        typedef struct _evt_info {
            _evt_info() : epoch_secs(0), seq(0) {};
            _evt_info(sequence_t s) : epoch_secs(time(nullptr)), seq(s) {};

            time_t      epoch_secs;
            sequence_t  seq;
        } evt_info_t;

        typedef map<runtime_id_t, evt_info_t> track_info_t;
        track_info_t m_track;

        /* Prune the m_track, when goes beyond max - MAX_PUBLISHERS_COUNT */
        void prune_track();


        /*
         * List of cached events.
         * Only part 2 / internal_event_t is cached as serialized string.
         */
        event_serialized_lst_t m_from_cache;

};

/*
 * The uuid_unparse() function converts the supplied UUID uu from
 * the binary representation into a 36-byte string (plus trailing
 * '\0') of the form 1b4e28ba-2fa1-11d2-883f-0016d3cca427 and stores
 * this value in the character string pointed to by out. 
 */
#define UUID_STR_SIZE 40

/*
 * Publisher use echo service and subscriber uses cache service
 * The eventd process runs this service, which could be down
 * All service interactions being async, a timeout is required
 * not to block forever on read.
 *
 * The unit is in milliseconds in sync with ZMQ_RCVTIMEO of
 * zmq_setsockopt.
 *
 * Publisher uses more to shadow async connectivity from PUB to 
 * XSUB end point of eventd's proxy. Hence have a smaller value.
 *
 * Subscriber uses it for cache management and here we need a
 * longer timeout, to handle slow proxy. This timeout value's only
 * impact could be when subscriber process is trying to terminate.
 */

#define EVENTS_SERVICE_TIMEOUT_MS_PUB 200
#define EVENTS_SERVICE_TIMEOUT_MS_SUB 2000

#endif /* !_EVENTS_PI_H */ 
