#ifndef _EVENTS_SERVICE_H
#define _EVENTS_SERVICE_H

#include "string.h"
#include "events_common.h"


/*
 * The eventd runs an event service that supports caching & miscellaneous
 * as required by events API.
 *
 * This header lists the services provided,
 * 
 * These services are only used by events API internally.
 *
 * All the services uses REQ/REP pattern between caller
 * centralized event service. The return code is provided
 * by service in its reply.
 *
 * event_request goes as single part message carrying event_req_t,
 * unless there is data associated.
 *
 * Both req & resp are either single or two part. In case of no
 * data associated, it is just request code or response return value in one part.
 * In case of data, it comes in seccond part.
 * 
 * Type of data is one or multiple strings, which is sent as serialized vector
 * of strings. event_serialized_lst_t
 *
 * In case of echo, part2 is the vector of single string as provided in the request.
 *
 * The cache-read returns cached events as vector of serialized strings.
 * NOTE: Cache only saves part 2 of the published event.
 */

/* List of request codes for supported commands */
typedef enum {
    EVENT_CACHE_INIT,   /* Init the cache before start */
    EVENT_CACHE_START,  /* Start caching all published events */
    EVENT_CACHE_STOP,   /* Stop the cache */
    EVENT_CACHE_READ,   /* Read cached events */
    EVENT_ECHO,         /* Echoes the received data in request via response */
    EVENT_OPTIONS,      /* global options Set/Get */
    EVENT_EXIT          /* Exit the eventd service -- Useful for unit test.*/
} event_req_type_t;


/*
 * internal service init & APIs for read & write
 */

/*
 *  An internal service is provided for cache handling & miscellaneous.
 *
 *  Clients initialize for init_client and uses the provided services
 *  server code init for server and use read & write APIs
 */
class event_service {
    public:
        event_service(): m_socket(NULL) {}

        ~event_service() { close_service(); }

        /*
         * Init the service for client or server.
         * The client uses REQ socket & connects.
         * The server uses REP socket & bind.
         *  
         * Block helps setting timeout for any read.
         * Publishing clients try ECHO service send/recv to help shadow
         * its async connection to XPUB end point, but the eventd service
         * could be down. So having a timeout, helps which will timeout
         * recv.
         * Publish clients that choose to block may specify the duration
         *
         * Input:
         *  zmq_ctx - Context to use
         *  block_ms
         *       0 -    Return immediately
         *      <N>-    Count of millisecs to wait for a message.
         *      -1 -    Block until a message is available or any fatal failure.
         *
         *  return:
         *      0  -    On success
         *     -1  -    On failure. zerrno is set to EAGAIN, if timed out.
         *              Else appropriate error code is set as per zmq_msg_recv.
         */
        int init_client(void *zmq_ctx, int block_ms = -1);

        int init_server(void *zmq_ctx, int block_ms = -1);

        /*
         * Event cache service is singleton service
         *
         * Usage:
         *  Init - Initiates the connection
         *  start - Start reading & caching
         *  stop - Stop reading & disconnect.
         *  read - Read the cached events.
         *
         * Any duplicate start has no impact.
         * The cached events can be read only upon stop. A read before
         * stop returns failure. A read w/o a start succeeds with no message.
         *
         */


        /*
         *  Called to init caching events
         *
         *  This is transparently called by events_deinit_subscriber, if cache service
         *  was enabled. This simply triggers a connect request and does not start
         *  reading yet. 
         *  NOTE: ZMQ connects asynchronously.
         *
         *  return:
         *      0   - On success. 
         *      1   - Already running
         *      -1  - On failure.
         */
        int cache_init();

        /*
         *  Called to start caching events
         *
         *  This is transparently called by events_deinit_subscriber after init.
         *  The deinit call may provide events in its local cache.
         *  The caching service uses this as initial/startup stock.
         *
         *  input:
         *      lst - i/p events from caller's cache.
         *
         *  return:
         *      0   - On success. 
         *      1   - Already running
         *      -1  - On failure.
         */
        int cache_start(const event_serialized_lst_t &lst);

        /*
         *  Called to stop caching events
         *
         *  This is transparently called by events_init_subscriber, if cache service
         *  is enabled.
         *
         *  return:
         *      0   - On success. 
         *      1   - Not running
         *      -1  - On failure.
         */
        int cache_stop();

        /*
         *  cache read
         *
         *  This is transparently called by event_receive, if cache service
         *  is enabled.
         *  Each event is received as 2 parts. First part is more a filter for
         *  hence dropped. The second part is returned as string events_data_type_t.
         *  The string is the serialized form of internal_event_t.
         *
         *  An empty o/p implies no more.
         *
         *  Internal details:
         *
         *  Cache service caches all events until buffer overflow or max-ceil.
         *
         *  Upon overflow, it creates a separate cache, where it keeps only
         *  the last event received per runtime ID. 
         *  This is required for receiver to be able to get the missed count.
         *
         *  The receiver API will compute the missed in the same way for
         *  events read from subscription channel & as well from cache.
         *
         *  output: 
         *      lst - A set of events, with a max cap.
         *            Hence multiple reads may be required to read all.
         *            An empty list implies no more event in cache.
         *
         *  return:
         *  0   - On success.
         *  -1  - On failure.
         */
        int cache_read(event_serialized_lst_t &lst);

        /*
         *  Echo send service.
         *
         *  A service to just test out connectivity. The publisher uses
         *  it to shadow its connection to zmq proxy's XSUB end. This is
         *  called transparently by events_init_publisher.
         *
         * Input:
         *  s - string to echo.
         *
         *  return:
         *      0   - On success
         *      -1  - On failure.
         */
        int echo_send(const string s);

        /*
         *  Echo receive service.
         *
         *  Receives the las sent echo.
         *  This is transparently called by event_publish on first
         *  publish call. This helps ensure connect XSUB end is *most* likely
         *  (99.9%) established, as write & read back takes one full loop.
         *
         *  The service is closed upon read as echo service is one shot only.
         *
         *  output:
         *      s - echoed string
         *
         *  return:
         *  0   - On success
         *  -1  - On failure
         */
        int echo_receive(string &s);


        /*
         * Global options request set
         *
         * Input:
         *  val -- Put the interval for set
         *
         * Return:
         *  0   - On Success
         *  -1  - On Failure
         */
        int global_options_set(const char *val);


        /*
         * Global options request get.
         *
         * Input:
         *  val_sz -- Size of val buffer
         *
         * Output:
         *  val -- Get the current val
         *
         * Return:
         *  > 0 - Count of bytes to copied/to-be-copied. 
         *        Result is truncated if given size <= this value.
         *        But copied string is *always* null termninated.
         *
         *  -1  - On Failure
         */
        int global_options_get(char *val, int val_sz);

        /*
         * The read for req/resp from client/server. The APIs above use this
         * to read response and the server use this to read request.
         *
         * Input: None
         *
         * Output:
         *  code -  It is event_req_type_t for request or return code
         *          for response
         *  data - Data read, if any
         *
         * return:
         *  0   - On success
         *  -1  - On failure
         */
        int channel_read(int &code, event_serialized_lst_t &data);

        /*
         * The under lying write for req/resp from client/server
         *
         * Input:
         *  code -  It is event_req_type_t for request or return code
         *          for response
         *  data - If any data to be sent along.
         *
         * Output: None
         *
         * return:
         *  0   - On success
         *  -1  - On failure
         */
        int channel_write(int code, const event_serialized_lst_t &data);

        /*
         * send and receive helper.
         * Writes given code & data and reads back data into
         * provided event_serialized_lst_t arg and response read is
         * returned.
         *
         * input:
         *  code -  Request code
         *  lst  -  Data to send
         *
         * output:
         *  lst -   Data read, if any
         *
         * return:
         *  Any failure or response code from server.
         */
        int send_recv(int code, const event_serialized_lst_t *lst_in = NULL,
                event_serialized_lst_t *lst_out = NULL);

        /*
         * de-init/close service
         */
        void close_service();

        /*
         * Check if service is active
         */
        bool is_active() { return m_socket != NULL; };

private:
        void *m_socket;

};

#endif // _EVENTS_SERVICE_H
