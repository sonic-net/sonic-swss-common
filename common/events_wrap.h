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

#ifdef __cplusplus
}
#endif
#endif // _EVENTS_WRAP_H

