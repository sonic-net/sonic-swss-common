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

using namespace std;


/* Base class for publisher & subscriber */
class events_base
{
    public:
        virtual ~events_base() = default;

        static sequence_t str_to_seq(const string s)
        {
            stringstream ss(s);
            sequence_t seq;

            ss >> seq;

            return seq;
        }

        static string seq_to_str(sequence_t seq)
        {
            stringstream ss;
            ss << seq;
            return ss.str();
        }
        
};

/*
 * A single instance per sender & source.
 * A sender is very likely to use single source only.
 * There can be multiple sender processes/threads for a single source.
 *
 */

class EventPublisher : public events_base
{
    public:
        EventPublisher();

        virtual ~EventPublisher();

        int init(const string event_source);

        int publish(const std::string event_tag,
                const event_params_t *params);
    private:

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

class EventSubscriber : public events_base
{
    public:
        EventSubscriber();
        
        int init(bool use_cache=false,
                int recv_timeout= -1,
                const event_subscribe_sources_t *subs_sources=NULL);

        virtual ~EventSubscriber();

        int event_receive(string &key, event_params_t &params, int &missed_cnt);

    private:
        void *m_zmq_ctx;
        void *m_socket;

        /* Service to use for cache management & echo */
        event_service m_event_service;

        /*
         * Set to true, upon cache read returning non zero count of events
         * implying more to read.
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
        events_data_lst_t m_from_cache;

};

/*
 *  Cache drain timeout.
 *
 *  When de-init is called, it calls stop cache service.
 *  But before this point, there could be events received in zmq's
 *  local cache pending read and those that arrived since last read.
 *  These events will not be seen by cache service.
 *  So read those off and give it to cache service as starting stock.
 *  As we don't have a clue on count in zmq's cache, read in non-block
 *  mode for a period.
 */
#define CACHE_DRAIN_IN_MILLISECS 1000

#endif /* !_EVENTS_PI_H */ 
