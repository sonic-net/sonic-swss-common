#ifndef _EVENTS_WRAP_H
#define _EVENTS_WRAP_H

#ifdef __cplusplus
extern "C" {
#endif

#define ARGS_SOURCE "source"

/*
 * golang has very limited binding to C
 * restricting to the i/f to int & pointers
 *
 * Pass any data as JSON string in & out
 */

/*
 * Init publisher
 *
 * input:
 *  args:   as JSON string as below
 *      ' { "source" : "<source to use for publishing" }'
 *      e.g: '{ "source": "bgp" }
 *
 * Return:
 *  > 0 -- Handle to use for subsequent calls.
 *  < 0 -- Implies failure. Absoulte value is the error code
 */
void * events_init_publisher_wrap(const char *args);


/*
 * Deinit publisher
 *
 * input:
 *  handle -- as provided be events_init_publisher 
 *
 */
void events_deinit_publisher_wrap(void *handle);

/*
 * Publish an event.
 *
 * input:
 *  handle: Handle from init_publisher
 *
 *  args:
 *      '{ "tag" : "<event tag to use for publishing>",
 *         "params": {
 *             <map of string:string params>
 *          }
 *       }'
 *      e.g: '{
 *          "tag": "bgp-state":
 *          "params": {
 *               "timestamp": "2022-08-17T02:39:21.286611Z",
 *               "ip": "100.126.188.90",
 *               "status": "down"
 *           }
 *       }'
 *
 *
 * Return:
 *  == 0 -- Published successfully
 *  < 0 -- Implies failure. Absoulte value is the error code
 */
#define ARGS_TAG "tag"
#define ARGS_PARAMS "params"

int event_publish_wrap(void *handle, const char *args);


/*
 * Init subscriber
 *
 * input:
 *  args:   as JSON string as below
 *      '{
 *          "use_cache" : <true/false as bool. default: false>,
 *          "recv_timeout": <timeout value as int, default: -1>,
 *       }'
 *      A missing key will be implied for default.
 *      e.g: '{ "use_cache": false }
 *
 * Return:
 *  > 0 -- Handle to use for subsequent calls.
 *  < 0 -- Implies failure. Absoulte value is the error code
 */
#define ARGS_USE_CACHE "use_cache"
#define ARGS_RECV_TIMEOUT "recv_timeout"

void *events_init_subscriber_wrap(const char *args);


/*
 * Deinit subscribe* input:
 *  handle -- as provided be events_init_subscriber 
 *
 */
#define ARGS_KEY "key"
#define ARGS_MISSED_CNT "missed_cnt"

void events_deinit_subscriber_wrap(void *handle);

/*
 * Receive an event.
 *
 * input:
 *  args:
 *      Buffer for receiving event formatted as below.
 *      '{ "key" : "<event's key as string>",
 *         "params": {
 *             <map of string:string params>
 *          },
 *          "missed_cnt": <missed count as int>
 *       }'
 *      e.g: '{
 *          "key": "sonic-events-bgp:bgp-state":
 *          "params": {
 *               "timestamp": "2022-08-17T02:39:21.286611Z",
 *               "ip": "100.126.188.90",
 *               "status": "down"
 *           },
 *          "missed_cnt": 0
 *       }'
 *  sz:
 *      Size of the buffer for receiving event.
 *
 *
 * Return:
 *  > 0 -- Implies received an event
 *  = 0 -- implies no event received due to timeout
 *  < 0 -- Implies failure. Absoulte value is the error code
 */
int event_receive_wrap(void *handle, char *event_str,
        int event_str_sz, char *missed_cnt, int missed_cnt_sz);

int event_receive_wrap_1(void *handle);

int event_receive_wrap_2(char *event_str, int sz);
int event_receive_wrap_3(char *event_str,
        int event_str_sz, char *missed_cnt, int missed_cnt_sz);

int event_receive_wrap_4(void *handle, char *event_str);

int event_receive_wrap_5(const char *event_str);

#ifdef __cplusplus
}
#endif
#endif // _EVENTS_WRAP_H

