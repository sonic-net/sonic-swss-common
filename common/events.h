#ifndef _EVENTS_H
#define _EVENTS_H

#include <string>
#include <vector>
#include <map>

/*
 * Events library 
 *
 *  APIs are for publishing & receiving events with source, tag and params along with timestamp.
 *  Used by event publishers and those interested in receiving published events.
 *  Publishers are multiple run from different contexts, as processes running in hosts & containers.
 *  Receiver are often few. Telmetry container runs a receiver.
 *
 */


/* Handle for a publisher / subscriber instance */
typedef void* event_handle_t;

/*
 * Initialize an event publisher instance for an event source.
 *
 *  A single publisher instance is maintained for a source.
 *  Any duplicate init call for a source will return the same instance.
 *
 * NOTE:
 *      The initialization occurs asynchronously.
 *      Any event published before init is complete, is blocked until the init.
 *      is complete. Hence recommend, do the init as soon as the process starts.
 *
 * Input:
 *  event_source
 *      The YANG module name for the event source. All events published with the handle
 *      returned by this call is tagged with this source, transparently. The receiver
 *      could subscribe with this source as filter.
 *
 * Return 
 *  Non NULL handle
 *  NULL on failure
 */

event_handle_t events_init_publisher(const std::string event_source);

/*
 * De-init/free the publisher
 *
 * Input: 
 *  Handle returned from events_init_publisher
 *
 * Output: 
 *  Handle is nullified.
 */
void events_deinit_publisher(event_handle_t handle);


/*
 * List of event params
 */
typedef std::map<std::string, std::string> event_params_t;

/*
 * timestamp param name
 */
#define EVENT_TS_PARAM "timestamp"

/*
 * Publish an event
 *
 *  Internally a globally unique sequence number is embedded in every published event,
 *  The sequence numbers from same publishing instances can be compared
 *  to see if there any missing events between.
 *
 *  The sequence has two components as run-time-id that distinguishes
 *  the running instance of a publisher and other a running sequence
 *  starting from 0, which is local to this runtime-id.
 *
 *  The receiver API keep last received number for each runtime id
 *  and use this info to compute missed event count upon next event.
 *
 * input:
 *  handle - As obtained from events_init_publisher for a event-source.
 *
 *  event_tag -
 *      Name of the YANG container that defines this event in the
 *      event-source module associated with this handle.
 *
 *      YANG path formatted as "< event_source >:< event_tag >"
 *      e.g. {"sonic-events-bgp:bgp-state": { "ip": "10.10.10.10", ...}}
 *
 *  params -
 *      Params associated with event; This may or may not contain
 *      timestamp. In the absence, the timestamp is added, transparently.
 *      The timestamp should be per rfc3339
 *      e.g. "2022-08-17T02:39:21.286611Z"
 *
 * return:
 *  0   - On success
 *  > 0 - On failure, returns zmq_errno, if failure is zmq socket related.
 *  < 0 - For all other failures
 */

/*
 * A sanity check on final JSON string size of event
 * An error log will be written for too big events for alert.
 */
#define EVENT_MAXSZ 1024

int event_publish(event_handle_t handle, const std::string event_tag,
        const event_params_t *params=NULL);



/*
 * Initialize subscriber.
 *  Init subscriber, optionally to filter by event-source.
 *
 * Input:
 *  use_cache
 *      When set to true, it will make use of the cache service transparently.
 *      The cache service caches events during session down time. The deinit
 *      start the caching and init call stops the caching.
 *      default: false
 *
 * recv_timeout
 *      Read blocks by default until an event is available for read.
 *      0  - Returns immediately, with or w/o event
 *      -1 - Default; Blocks until an event is available for read
 *      N  - Count ofd milliseconds to wait for an event.
 *
 *
 *  lst_subscribe_sources_t
 *      List of subscription sources of interest.
 *      The source value is the corresponding YANG module name.
 *      e.g. "sonic-events-bgp " is the source modulr name for bgp.
 *      default: All sources, if none provided.
 *
 * Return:
 *  Non NULL handle on success
 *  NULL on failure
 */
typedef std::vector<std::string> event_subscribe_sources_t;

event_handle_t events_init_subscriber(bool use_cache=false,
        int recv_timeout = -1,
        const event_subscribe_sources_t *sources=NULL);

/*
 * De-init/free the subscriber
 *
 * Input: 
 *  Handle returned from events_init_subscriber
 *
 * Output: 
 *  Handle is nullified.
 */
void events_deinit_subscriber(event_handle_t handle);


typedef struct {
    std::string key;        /* key */
    event_params_t params;  /* Params received */
    uint32_t missed_cnt;        /* missed count */
    int64_t publish_epoch_ms;  /* Epoch time in milliseconds */
} event_receive_op_t;

/*
 * Receive an event.
 * A blocking call unless the subscriber is created with a timeout.
 *
 *  This API maintains an expected sequence number and use the received
 *  sequence in event to compute missed events count. The missed count
 *  provides the count of events missed from this sender.
 *
 * input:
 *  handle - As obtained from events_init_subscriber
 *
 * output:
 *  evt : 
 *      The entire received event.
 *
 * return:
 *  0   - On success
 *  > 0 - Implies failure due to timeout.
 *  < 0 - For all other failures
 *
 */
int event_receive(event_handle_t handle, event_receive_op_t &evt);

/* To receive as JSON */
int event_receive_json(event_handle_t handle, std::string &evt,
        uint32_t &missed_cnt, int64_t &publish_epoch_ms);


#endif /* !_EVENTS_H */ 
