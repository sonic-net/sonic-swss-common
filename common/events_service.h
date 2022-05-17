#include "string.h"
#include "events_common.h"


/*
 * All the services uses REQ/REP pattern between caller
 * centralized event service. The return code is provided
 * by service in its reply.
 *
 * event_request goes as single part message carrying event_req_t,
 * unless there is data associated.
 *
 * Both req & resp are either single or multi part. In case of no
 * data associated, it is just request code or response return value in one part.
 * In case of data, it comes in seccond part.
 * 
 * Type of data is as decided by request.
 *
 * In case of echo, part2 is the string as provided in the request.
 *
 * The read returns as serialized vector of ZMQ messages, in part2.
 */

typedef enum {
    EVENT_CACHE_START,
    EVENT_CACHE_STOP,
    EVENT_CACHE_READ,
    EVENT_ECHO
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

        ~event_service() { close(); }

        init_client(void *zmq_ctx, int block_ms = -1);

        init_server(void *zmq_ctx);

        /*
         * Event cache service is singleton service
         *
         * Any duplicate start has no impact.
         * The cached events can be read only upon stop. A read before
         * stop returns failure. A read w/o a start succeeds with no message.
         *
         */


        /*
         *  Called to start caching events
         *
         *  This is transparently called by events_deinit_subscriber, if cache service
         *  was enabled.
         *
         *  return:
         *      0   - On success. 
         *      1   - Already running
         *      -1  - On failure.
         */
        int cache_start();

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
         *  is enabled. Returns the set of zmq messages as received.
         *  NOTE: Every event is received as 2 parts. So 2 ZMQ messages makes
         *  one event.
         *
         *  An empty o/p implies no more.
         *
         *  Internal details:
         *
         *  Cache service caches all events until buffer overflow
         *  During receive using sequence+runtime Id, it keeps the count
         *  of missed to receive.
         *
         *  Upon overflow, it creates a separate cache, where it keeps only
         *  the last event received per runtime ID. 
         *  This is required for receiver to be able to get the missed count.
         *
         *  The receiver API will compute the missed in the same way for
         *  events read from subscription channel & as well from cache.
         *
         *  output: 
         *      lst_msgs
         *
         *  return:
         *  0   - On success. Either stopped or none to stop.
         *  -1  - On failure.
         */
        int cache_read(vector<string> &lst);

        /*
         *  Echo send service.
         *
         *  A service to just test out connectivity. The publisher uses
         *  it to shadow its connection to zmq proxy's XSUB end. This is
         *  called transparently by events_init_publisher.
         *
         *  The service is closed upon failure to send as echo service is one shot only.
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
         * The underlying read for req/resp from client/server
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
        int channel_read(int &code, vector<string> &data);

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
        int channel_write(int code, vector<string> &data);


        /*
         * de-init/close service
         */
        void close_service();

        /*
         * Check if service is active
         */
        bool is_active() { return m_socket != NULL };

private:
        void *m_socket;

};


