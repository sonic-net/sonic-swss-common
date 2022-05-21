#ifndef _EVENTS_SERVICE_H
#define _EVENTS_SERVICE_H
/*
 * Private header file used by events API implementation.
 * Required to run white box testing via unit tests.
 */
#include <chrono>
#include <ctime>
#include <fstream>
#include <thread>
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

        void publish(event_handle_t &handle, const std::string &event_tag,
                const event_params_t *params);
    private:

        void *m_zmq_ctx;
        void *m_socket;

        event_service m_event_svc;

        string m_event_source;

        runtime_id_t m_runtime_id;
        sequence_t m_sequence;

        bool m_init;
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
                const event_subscribe_sources_t *subs_sources=NULL);

        virtual ~EventSubscriber();

        int event_receive(string &key, event_params_t &params, int &missed_cnt);

    private:
        void *m_zmq_ctx;
        void *m_socket;

        event_service m_event_svc;

        bool m_init;
        bool m_cache_read;

        /*
         *  Sequences tracked by sender for a source, or in other
         *  words by runtime id.
         */
        typedef struct _evt_info {
            _evt_info() : epoch_secs(0), seq(0) {};
            _evt_info(sequence_t s) : epoch_secs(time(nullptr)), seq(s) {};

            time_t      epoch_secs;
            sequence_t  seq;
        } evt_info_t;

        typedef map<runtime_id_t, sequence_t> track_info_t;
        track_info_t m_track;

        events_data_lst_t m_from_cache;

        void prune_track();

};

#endif /* !_EVENTS_SERVICE_H */ 
