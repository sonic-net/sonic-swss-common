/*
 * Private header file
 * Required to run white box testing via unit tests.
 */
#include <chrono>
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

/*
 * events are published as two part zmq message.
 * First part only has the event source, so receivers could
 * filter by source.
 *
 * Second part contains map of 
 *  <efined below.
 *
 */
#define EVENT_STR_DATA "event_str"
#define EVENT_RUNTIME_ID  "runtime_id"
#define EVENT_SEQUENCE "sequence"

typedef map<string, string> internal_event_t;

tyepdef string runtime_id_t;

internal_event_t internal_event_ref = {
    { EVENT_STR_DATA, "" },
    { EVENT_RUNTIME_ID, "" },
    { EVENT_SEQUENCE, "" } };

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

typedef map<<string, event_handle_t> lst_publishers_t;

class EventPublisher : public events_base {
    public:
        // Track publishers by source & sender, as missed messages
        // is tracked per sender by source
        //
        static lst_publishers_t s_publishers;

        void *m_zmq_ctx;
        void *m_socket;

        event_service m_event_svc;

        /* Event source in msg to be sent as part 1 of multi-part */
        zmq_msg_t m_zmsg_source;
        string m_event_source;

        runtime_id_t m_runtime_id;
        uint32_t m_sequence;

        bool m_init;

        EventPublisher();

        int init(const char *event_source, int block_ms=-1);

        virtual ~EventPublisher();

        void publish(event_handle_t &handle, const std::string &event_tag,
                const event_params_t *params);

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

        void *m_zmq_ctx;
        void *m_socket;

        // Indices are tracked per sendefr & source.
        //
        map<string, index_data_t> m_indices;

        // Cumulative missed count across all subscribed sources.
        //
        uint64_t m_missed_cnt;

        EventSubscriber(const event_subscribe_sources_t *subs_sources);

        virtual ~EventSubscriber();

        bool do_receive(map_str_str_t  *data);

        bool validate_meta(const map_str_str_t  &meta);

        index_data_t update_missed(map_str_str_t  &meta);

        void event_receive(event_metadata_t &metadata, event_params_t &params,
                unsigned long *missed_count = NULL);
};

class EventProxy
{
    public:
        void *m_ctx;
        void *m_rep;
        void *m_frontend;
        void *m_backend;
        bool m_init_success;

        std::thread m_req_thread,  m_proxy_thread;

        EventProxy();

        ~EventProxy();

        int run(void);

        void run_cache_service(void);
        void run_proxy(void);
};

extern EventProxy g_event_proxy;



