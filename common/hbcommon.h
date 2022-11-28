#ifndef __HBCOMMON__
#define __HBCOMMON__

#define HB_VERSION_1_0           1

/* Default values */
#define HB_DEFAULT_ECHO_TIMER    5 /* 5 seconds, periodic interval at which the heartbeats are sent */
#define HB_DEFAULT_DEAD_TIMER    ((3 * HB_DEFAULT_ECHO_TIMER) + 1)
#define HB_DEFAULT_ACTION        (HB_ACTION_LOG | HB_ACTION_PRINT_BACKTRACE)
#define HB_DEFAULT_SIGNAL_NUM    SIGTERM

typedef enum
{
    HB_ACTION_LOG             = 0x2,
    HB_ACTION_SIGNAL          = 0x4,
    HB_ACTION_PRINT_BACKTRACE = 0x8
} hb_miss_action_e;

typedef enum
{
    HB_ECHO                = 0x2,
    HB_ECHO_ACK	           = 0x4,
    HB_CLIENT_REGISTER     = 0x8,
    HB_CLIENT_DEREGISTER   = 0x10
} hb_msg_type_e;

typedef struct
{
    /* Client SLA or preferences */

    /* Client passes the SLA value as 0, if the default value is to 
     * be picked up by the HB Server */  

    uint32_t         hb_poll_time;
    uint32_t         hb_dead_time;
    hb_miss_action_e action_on_dead_time_expiry;
    uint32_t         signal_num_for_action;
} hb_client_sla_t;

/* Message sent by HB Client to the Server */
typedef struct
{
    uint32_t         version;
    char             process_name[64];
    uint32_t         process_pid;

    hb_msg_type_e    msg_type;
    hb_client_sla_t  sla;
} hb_client_message_t;

/* Message sent by HB Monitor to the Clients */
typedef struct
{
    uint32_t         version;
    hb_msg_type_e    msg_type;
} hb_server_message_t;

#endif /* __HBCOMMON__ */
