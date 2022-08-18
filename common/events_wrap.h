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
 *  event_source -
 *      Refer core API events_init_publisher for details.
 *
 * Return:
 *  > 0 -- Handle to use for subsequent calls.
 *  < 0 -- Implies failure. Absoulte value is the error code
 */
void * events_init_publisher_wrap(const char *event_source);


/*
 * Deinit publisher
 *
 * input:
 *  handle -- as provided be events_init_publisher 
 *
 */
void events_deinit_publisher_wrap(void *handle);

typedef struct param_C {
    const char *name;
    const char *val;
} param_C_t;

/*
 * Publish an event.
 *
 * input:
 *  handle: Handle from init_publisher
 *
 *  data:
 *      Data to be published.
 *      Refer core API event_publish for more details.
 *
 * Return:
 *  == 0 -- Published successfully
 *  < 0 -- Implies failure. Absoulte value is the error code
 */
int event_publish_wrap(void *handle, const char *tag,
        const param_C_t *params, uint32_t params_cnt);


/*
 * Init subscriber
 *
 * input:
 *  
 *  init_data:
 *      Refer core API events_init_subscriber for details.
 *
 * Return:
 *  > 0 -- Handle to use for subsequent calls.
 *  < 0 -- Implies failure. Absoulte value is the error code
 */

void *events_init_subscriber_wrap(bool use_cache, int recv_timeout);


/*
 * Deinit subscribe* input:
 *  handle -- as provided be events_init_subscriber 
 *
 */
void events_deinit_subscriber_wrap(void *handle);

/*
 * Receive an event.
 *
 * input:
 *  handle - Handle obtained from init subscriber
 *  evt:
 *      Received event. Refer struct for details.
 *
 * Return:
 *  0   - On success
 *  > 0 - Implies failure due to timeout.
 *  < 0 - For all other failures
 */

typedef struct event_receive_op_C {
    /* Event as JSON string; c-string  to help with C-binding for Go.*/
    char *event_str;
    uint32_t event_sz;      /* Sizeof event string */

    uint32_t missed_cnt;    /* missed count */

    int64_t publish_epoch_ms;   /* Epoch timepoint of publish */

} event_receive_op_C_t;


int event_receive_wrap(void *handle, event_receive_op_C_t *evt);

/*
 * Set SWSS log priority
 */
void swssSetLogPriority(int pri);


/*
 *  Global configurable options can be set via this API.
 *
 *  The options are provided as JSON object with key/values as JSON string.
 *  key = <option name case insensitive>
 *  value = <option value>
 *
 *  Supported options
 *     
 *  Option name: HEARTBEAT_INTERVAL_SECS
 *  Option Value: interval in seconds as int
 *      A value of -1 implies no heartbeat
 *      A value of 0 implies the lowest possible interval as possible/supported. 
 *          This depends on implementation.
 *      Any non zero value implies count of seconds.
 *          NOTE: System will round it to the multiple of lowest interval supported.
 *      Any negative value other than -1 is treated as invalid.
 *
 *
 *  Option name: OFFLINE_CACHE_SIZE     TODO/Not Yet Implemented
 *  Option Value: Size in count MBs as int
 *      A value of 0 implies the system default
 *          This depends on implementation.
 *      Any non zero value is accepted.
 *          This is constraint on available memory. Hence this is only a
 *          guideline to take if possible.
 *      Any negative value other than -1 is treated as invalid.
 *
 *  Input:
 *      options - A c string holding JSON string of the options.
 *
 *  Return:
 *  0   -   Implies success. It also implies that the provided options and
 *          their values were valid.
 *  < 0 -   Implies failure. Either internal failure or invalid options or
 *          invalid values. Look at syslog for details.
 */

#define GLOBAL_OPTION_HEARTBEAT "HEARTBEAT_INTERVAL"

int event_set_global_options(const char *options);



/*
 *  A way to read the current values for global options.
 *  Refer above for details.
 *
 *  Input:
 *      None
 *
 *  Output:
 *      options - A buffer for c string holding JSON string of the options.
 *
 *      options_size -  Size of options buffer. The size must include space
 *                      for terminating NULL character. If string to be copied
 *                      is same or greater size, (options_size - 1) bytes
 *                      are copied with a terminating NULL.
 *
 *
 * Return:
 *  > 0 -   Count of characters of the final JSON string to return. If given
 *          size is less/equal, then it implies the buffer carries truncated string.
 *          NOTE: The final copied string is always null terminated.
 *
 *  < 0 -   Implies failure to reach eventd service.
 *
 */
int event_get_global_options(char *options, int options_size);

#ifdef __cplusplus
}
#endif
#endif // _EVENTS_WRAP_H

