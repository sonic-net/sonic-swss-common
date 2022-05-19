/*
 * Events library 
 *
 *  APIs are for publishing & receiving events with source, tag and params along with timestamp.
 *  Used by event publishers and those interested in receiving published events.
 *  Publishers are multiple sources, as processes running in hosts & containers.
 *  Receiver are often few. Telmetry container runs a receiver.
 *
 */


typedef void* event_handle_t;

/*
 * Initialize an event publisher instance for an event source.
 *
 *  A single publisher instance is maintained for a source.
 *  Any duplicate init call for a source will return the same instance.
 *
 *  Choosing cache will help read cached data, during downtime, if any.
 *
 * NOTE:
 *      The initialization occurs asynchronously.
 *      Any event published before init is complete, is blocked until the init
 *      is complete. Hence recommend, do the init as soon as the process starts.
 *
 * Input:
 *  event_source
 *      The YANG module name for the event source. All events published with the handle
 *      returned by this call is tagged with this source, transparently. The receiver
 *      could subscribe with this source as filter.
 *
 *  block_millisecs -
 *      Block either until publisher connects successfully or timeout
 *      whichever earlier.
 *      0   - No blocling.
 *      -1  - Block until connected.
 *      N   - Count in milli seconds to wait.
 *      NOTE: The connection is to eventd service, which could be down.
 * Return 
 *  Non NULL handle
 *  NULL on failure
 */

event_handle_t events_init_publisher(std::string event_source,
        int block_millisecs);

/*
 * De-init/free the publisher
 *
 * Input: 
 *  Handle returned from events_init_publisher
 *
 * Output: 
 *  None
 */
void events_deinit_publisher(event_handle_t &handle);


/*
 * List of event params
 */
typedef std::map<std::string, std::string> event_params_t;

/*
 * timestamp param name
 */
const string event_ts("timestamp");

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
 *  The receiver API keep next last received number for each runtime id
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
 */
void event_publish(event_handle_t handle, const std::string event_tag,
        const event_params_t *params=NULL);



typedef std::vector<std::string> event_subscribe_sources_t;

/*
 * Initialize subscriber.
 *  Init subscriber, optionally to filter by event-source.
 *
 * Input:
 *  use_cache
 *      When set to true, it will make use of the cache service transparently.
 *      The cache service caches events during session down time (last deinit to this
 *      init call).
 *
 *  lst_subscribe_sources_t
 *      List of subscription sources of interest.
 *      The source value is the corresponding YANG module name.
 *      e.g. "sonic-events-bgp " is the source modulr name for bgp.
 *
 * Return:
 *  Non NULL handle on success
 *  NULL on failure
 */
event_handle_t events_init_subscriber(bool use_cache=false,
        const event_subscribe_sources_t *sources=NULL);

/*
 * De-init/free the subscriber
 *
 * Input: 
 *  Handle returned from events_init_subscriber
 *
 * Output: 
 *  None
 */
void events_deinit_subscriber(event_handle_t &handle);

/*
 * Received event as JSON string as 
 *  < YANG path of schema >: {
 *      event_params_t
 *  }
 */
typedef std::string event_str_t;

/*
 * Receive an event.
 * A blocking call.
 *
 *  This API maintains an expected sequence number and use the received
 *  sequence in event to compute missed events count.
 *
 * input:
 *  handle - As obtained from events_init_subscriber
 *
 * output:
 *  event - Received event.
 *
 *  missed_cnt:
 *      Count of missed events from this sender, before this event. Sum of
 *      missed count from all received events will give the total missed.
 *
 * return:
 *  0 - On success
 * -1 - On failure. The handle is not valid.
 *
 */
int event_receive(event_handle_t handle, event_str_t &event, int &missed_cnt);

