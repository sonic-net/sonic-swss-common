#ifndef __SCHEMA__
#define __SCHEMA__

#ifdef __cplusplus
namespace swss {
#endif

/***** DATABASE *****/

#define APPL_DB         0
#define ASIC_DB         1
#define COUNTERS_DB     2
#define LOGLEVEL_DB     3
#define CONFIG_DB       4
#define PFC_WD_DB       5
#define FLEX_COUNTER_DB 5
#define STATE_DB        6
#define SNMP_OVERLAY_DB 7
#define RESTAPI_DB      8
#define GB_ASIC_DB      9
#define GB_COUNTERS_DB  10
#define GB_FLEX_COUNTER_DB  11
#define CHASSIS_APP_DB      12
#define CHASSIS_STATE_DB    13
#define APPL_STATE_DB       14
#define EVENT_DB            19
#define BMP_STATE_DB        20

/***** APPLICATION DATABASE *****/

#define APP_PORT_TABLE_NAME               "PORT_TABLE"
#define APP_SEND_TO_INGRESS_PORT_TABLE_NAME  "SEND_TO_INGRESS_PORT_TABLE"
#define APP_GEARBOX_TABLE_NAME            "GEARBOX_TABLE"
#define APP_FABRIC_PORT_TABLE_NAME        "FABRIC_PORT_TABLE"
#define APP_VLAN_TABLE_NAME               "VLAN_TABLE"
#define APP_VLAN_MEMBER_TABLE_NAME        "VLAN_MEMBER_TABLE"
#define APP_LAG_TABLE_NAME                "LAG_TABLE"
#define APP_LAG_MEMBER_TABLE_NAME         "LAG_MEMBER_TABLE"
#define APP_INTF_TABLE_NAME               "INTF_TABLE"
#define APP_NEIGH_TABLE_NAME              "NEIGH_TABLE"
#define APP_ROUTE_TABLE_NAME              "ROUTE_TABLE"
#define APP_LABEL_ROUTE_TABLE_NAME        "LABEL_ROUTE_TABLE"
#define APP_TUNNEL_DECAP_TABLE_NAME       "TUNNEL_DECAP_TABLE"
#define APP_TUNNEL_DECAP_TERM_TABLE_NAME  "TUNNEL_DECAP_TERM_TABLE"
#define APP_TUNNEL_ROUTE_TABLE_NAME       "TUNNEL_ROUTE_TABLE"
#define APP_FDB_TABLE_NAME                "FDB_TABLE"
#define APP_PFC_WD_TABLE_NAME             "PFC_WD_TABLE"
#define APP_SWITCH_TABLE_NAME             "SWITCH_TABLE"
#define APP_NEXTHOP_GROUP_TABLE_NAME      "NEXTHOP_GROUP_TABLE"
#define APP_CLASS_BASED_NEXT_HOP_GROUP_TABLE_NAME "CLASS_BASED_NEXT_HOP_GROUP_TABLE"

#define APP_P4RT_TABLE_NAME                   "P4RT_TABLE"
#define APP_P4RT_TABLES_DEFINITION_TABLE_NAME "TABLES_DEFINITION_TABLE"
#define APP_P4RT_ROUTER_INTERFACE_TABLE_NAME  "FIXED_ROUTER_INTERFACE_TABLE"
#define APP_P4RT_NEIGHBOR_TABLE_NAME          "FIXED_NEIGHBOR_TABLE"
#define APP_P4RT_NEXTHOP_TABLE_NAME           "FIXED_NEXTHOP_TABLE"
#define APP_P4RT_WCMP_GROUP_TABLE_NAME        "FIXED_WCMP_GROUP_TABLE"
#define APP_P4RT_IPV4_TABLE_NAME              "FIXED_IPV4_TABLE"
#define APP_P4RT_IPV6_TABLE_NAME              "FIXED_IPV6_TABLE"
#define APP_P4RT_IPV4_MULTICAST_TABLE_NAME    "FIXED_IPV4_MULTICAST_TABLE"
#define APP_P4RT_IPV6_MULTICAST_TABLE_NAME    "FIXED_IPV6_MULTICAST_TABLE"
#define APP_P4RT_ACL_TABLE_DEFINITION_NAME    "ACL_TABLE_DEFINITION_TABLE"
#define APP_P4RT_MIRROR_SESSION_TABLE_NAME    "FIXED_MIRROR_SESSION_TABLE"
#define APP_P4RT_L3_ADMIT_TABLE_NAME          "FIXED_L3_ADMIT_TABLE"
#define APP_P4RT_TUNNEL_TABLE_NAME            "FIXED_TUNNEL_TABLE"
#define APP_P4RT_MULTICAST_ROUTER_INTERFACE_TABLE_NAME "FIXED_MULTICAST_ROUTER_INTERFACE_TABLE"
#define APP_P4RT_REPLICATION_IP_MULTICAST_TABLE_NAME "REPLICATION_IP_MULTICAST_TABLE"
#define APP_P4RT_REPLICATION_L2_MULTICAST_TABLE_NAME "REPLICATION_L2_MULTICAST_TABLE"
#define APP_P4RT_IPV6_TUNNEL_TERMINATION_TABLE_NAME "FIXED_IPV6_TUNNEL_TERMINATION_TABLE"
#define APP_P4RT_DISABLE_VLAN_CHECKS_TABLE_NAME "FIXED_DISABLE_VLAN_CHECKS_TABLE"

#define APP_COPP_TABLE_NAME               "COPP_TABLE"
#define APP_VRF_TABLE_NAME                "VRF_TABLE"
#define APP_VNET_TABLE_NAME               "VNET_TABLE"
#define APP_VNET_RT_TABLE_NAME            "VNET_ROUTE_TABLE"
#define APP_VNET_RT_TUNNEL_TABLE_NAME     "VNET_ROUTE_TUNNEL_TABLE"
#define APP_VXLAN_VRF_TABLE_NAME          "VXLAN_VRF_TABLE"
#define APP_VXLAN_TUNNEL_MAP_TABLE_NAME   "VXLAN_TUNNEL_MAP_TABLE"
#define APP_VXLAN_TUNNEL_TABLE_NAME       "VXLAN_TUNNEL_TABLE"
#define APP_VXLAN_FDB_TABLE_NAME          "VXLAN_FDB_TABLE"
#define APP_VXLAN_REMOTE_VNI_TABLE_NAME   "VXLAN_REMOTE_VNI_TABLE"
#define APP_VXLAN_EVPN_NVO_TABLE_NAME     "VXLAN_EVPN_NVO_TABLE"
#define APP_NEIGH_SUPPRESS_VLAN_TABLE_NAME   "SUPPRESS_VLAN_NEIGH_TABLE"
#define APP_VLAN_STACKING_TABLE_NAME      "VLAN_STACKING_TABLE"
#define APP_VLAN_TRANSLATION_TABLE_NAME   "VLAN_TRANSLATION_TABLE"
#define APP_PASS_THROUGH_ROUTE_TABLE_NAME "PASS_THROUGH_ROUTE_TABLE"
#define APP_ACL_TABLE_TABLE_NAME          "ACL_TABLE_TABLE"
#define APP_ACL_TABLE_TYPE_TABLE_NAME     "ACL_TABLE_TYPE_TABLE"
#define APP_ACL_RULE_TABLE_NAME           "ACL_RULE_TABLE"
#define APP_SFLOW_TABLE_NAME              "SFLOW_TABLE"
#define APP_SFLOW_SESSION_TABLE_NAME      "SFLOW_SESSION_TABLE"
#define APP_SFLOW_SAMPLE_RATE_TABLE_NAME  "SFLOW_SAMPLE_RATE_TABLE"

#define APP_NAT_TABLE_NAME              "NAT_TABLE"
#define APP_NAPT_TABLE_NAME             "NAPT_TABLE"
#define APP_NAT_TWICE_TABLE_NAME        "NAT_TWICE_TABLE"
#define APP_NAPT_TWICE_TABLE_NAME       "NAPT_TWICE_TABLE"
#define APP_NAT_GLOBAL_TABLE_NAME       "NAT_GLOBAL_TABLE"
#define APP_NAPT_POOL_IP_TABLE_NAME     "NAPT_POOL_IP_TABLE"
#define APP_NAT_DNAT_POOL_TABLE_NAME    "NAT_DNAT_POOL_TABLE"

#define APP_STP_VLAN_TABLE_NAME             "STP_VLAN_TABLE"
#define APP_STP_VLAN_PORT_TABLE_NAME        "STP_VLAN_PORT_TABLE"
#define APP_STP_VLAN_INSTANCE_TABLE_NAME    "STP_VLAN_INSTANCE_TABLE"
#define APP_STP_PORT_TABLE_NAME             "STP_PORT_TABLE"
#define APP_STP_PORT_STATE_TABLE_NAME       "STP_PORT_STATE_TABLE"
#define APP_STP_FASTAGEING_FLUSH_TABLE_NAME "STP_FASTAGEING_FLUSH_TABLE"
#define APP_STP_BPDU_GUARD_TABLE_NAME       "STP_BPDU_GUARD_TABLE"
#define APP_MCLAG_FDB_TABLE_NAME            "MCLAG_FDB_TABLE"
#define APP_ISOLATION_GROUP_TABLE_NAME      "ISOLATION_GROUP_TABLE"
#define APP_BFD_SESSION_TABLE_NAME          "BFD_SESSION_TABLE"


#define APP_SAG_TABLE_NAME                  "SAG_TABLE"

#define APP_FC_TO_NHG_INDEX_MAP_TABLE_NAME   "FC_TO_NHG_INDEX_MAP_TABLE"

#define APP_BGP_PROFILE_TABLE_NAME           "BGP_PROFILE_TABLE"

#define APP_VNET_MONITOR_TABLE_NAME          "VNET_MONITOR_TABLE"

/***** ASIC DATABASE *****/
#define ASIC_TEMPERATURE_INFO_TABLE_NAME    "ASIC_TEMPERATURE_INFO"

#define APP_MUX_CABLE_TABLE_NAME            "MUX_CABLE_TABLE"
#define APP_HW_MUX_CABLE_TABLE_NAME         "HW_MUX_CABLE_TABLE"
#define APP_MUX_CABLE_COMMAND_TABLE_NAME    "MUX_CABLE_COMMAND_TABLE"
#define APP_MUX_CABLE_RESPONSE_TABLE_NAME   "MUX_CABLE_RESPONSE_TABLE"

#define APP_FORWARDING_STATE_COMMAND_TABLE_NAME  "FORWARDING_STATE_COMMAND"
#define APP_FORWARDING_STATE_RESPONSE_TABLE_NAME "FORWARDING_STATE_RESPONSE"

#define APP_PEER_PORT_TABLE_NAME             "PORT_TABLE_PEER"
#define APP_PEER_HW_FORWARDING_STATE_TABLE_NAME    "HW_FORWARDING_STATE_PEER"

#define APP_SYSTEM_PORT_TABLE_NAME          "SYSTEM_PORT_TABLE"

#define APP_MACSEC_PORT_TABLE_NAME          "MACSEC_PORT_TABLE"
#define APP_MACSEC_EGRESS_SC_TABLE_NAME     "MACSEC_EGRESS_SC_TABLE"
#define APP_MACSEC_INGRESS_SC_TABLE_NAME    "MACSEC_INGRESS_SC_TABLE"
#define APP_MACSEC_EGRESS_SA_TABLE_NAME     "MACSEC_EGRESS_SA_TABLE"
#define APP_MACSEC_INGRESS_SA_TABLE_NAME    "MACSEC_INGRESS_SA_TABLE"

#define APP_BUFFER_POOL_TABLE_NAME      "BUFFER_POOL_TABLE"
#define APP_BUFFER_PROFILE_TABLE_NAME   "BUFFER_PROFILE_TABLE"
#define APP_BUFFER_PG_TABLE_NAME        "BUFFER_PG_TABLE"
#define APP_BUFFER_QUEUE_TABLE_NAME     "BUFFER_QUEUE_TABLE"
#define APP_BUFFER_PORT_INGRESS_PROFILE_LIST_NAME   "BUFFER_PORT_INGRESS_PROFILE_LIST_TABLE"
#define APP_BUFFER_PORT_EGRESS_PROFILE_LIST_NAME    "BUFFER_PORT_EGRESS_PROFILE_LIST_TABLE"

#define APP_NEIGH_RESOLVE_TABLE_NAME        "NEIGH_RESOLVE_TABLE"

#define APP_SRV6_SID_LIST_TABLE_NAME        "SRV6_SID_LIST_TABLE"
#define APP_SRV6_MY_SID_TABLE_NAME          "SRV6_MY_SID_TABLE"

#define APP_DASH_VNET_TABLE_NAME            "DASH_VNET_TABLE"
#define APP_DASH_QOS_TABLE_NAME             "DASH_QOS_TABLE"
#define APP_DASH_ENI_TABLE_NAME             "DASH_ENI_TABLE"
#define APP_DASH_ACL_IN_TABLE_NAME          "DASH_ACL_IN_TABLE"
#define APP_DASH_ACL_OUT_TABLE_NAME         "DASH_ACL_OUT_TABLE"
#define APP_DASH_ACL_GROUP_TABLE_NAME       "DASH_ACL_GROUP_TABLE"
#define APP_DASH_ACL_RULE_TABLE_NAME        "DASH_ACL_RULE_TABLE"
#define APP_DASH_PREFIX_TAG_TABLE_NAME      "DASH_PREFIX_TAG_TABLE"
#define APP_DASH_ROUTING_TYPE_TABLE_NAME    "DASH_ROUTING_TYPE_TABLE"
#define APP_DASH_APPLIANCE_TABLE_NAME       "DASH_APPLIANCE_TABLE"
#define APP_DASH_ROUTE_TABLE_NAME           "DASH_ROUTE_TABLE"
#define APP_DASH_ROUTE_RULE_TABLE_NAME      "DASH_ROUTE_RULE_TABLE"
#define APP_DASH_VNET_MAPPING_TABLE_NAME    "DASH_VNET_MAPPING_TABLE"
#define APP_DASH_ENI_ROUTE_TABLE_NAME       "DASH_ENI_ROUTE_TABLE"
#define APP_DASH_ROUTE_GROUP_TABLE_NAME     "DASH_ROUTE_GROUP_TABLE"
#define APP_DASH_TUNNEL_TABLE_NAME          "DASH_TUNNEL_TABLE"
#define APP_DASH_PA_VALIDATION_TABLE_NAME   "DASH_PA_VALIDATION_TABLE"
#define APP_DASH_ROUTING_APPLIANCE_TABLE_NAME "DASH_ROUTING_APPLIANCE_TABLE"

/***** TO BE REMOVED *****/

#define APP_TC_TO_QUEUE_MAP_TABLE_NAME  "TC_TO_QUEUE_MAP_TABLE"
#define APP_SCHEDULER_TABLE_NAME        "SCHEDULER_TABLE"
#define APP_DSCP_TO_TC_MAP_TABLE_NAME   "DSCP_TO_TC_MAP_TABLE"
#define APP_QUEUE_TABLE_NAME            "QUEUE_TABLE"
#define APP_PORT_QOS_MAP_TABLE_NAME     "PORT_QOS_MAP_TABLE"
#define APP_WRED_PROFILE_TABLE_NAME     "WRED_PROFILE_TABLE"
#define APP_TC_TO_PRIORITY_GROUP_MAP_NAME           "TC_TO_PRIORITY_GROUP_MAP_TABLE"
#define APP_PFC_PRIORITY_TO_PRIORITY_GROUP_MAP_NAME "PFC_PRIORITY_TO_PRIORITY_GROUP_MAP_TABLE"
#define APP_PFC_PRIORITY_TO_QUEUE_MAP_NAME          "MAP_PFC_PRIORITY_TO_QUEUE"

/***** COUNTER DATABASE *****/

#define COUNTERS_PORT_NAME_MAP              "COUNTERS_PORT_NAME_MAP"
#define COUNTERS_SYSTEM_PORT_NAME_MAP       "COUNTERS_SYSTEM_PORT_NAME_MAP"
#define COUNTERS_LAG_NAME_MAP               "COUNTERS_LAG_NAME_MAP"
#define COUNTERS_TABLE                      "COUNTERS"
#define COUNTERS_QUEUE_NAME_MAP             "COUNTERS_QUEUE_NAME_MAP"
#define COUNTERS_VOQ_NAME_MAP               "COUNTERS_VOQ_NAME_MAP"
#define COUNTERS_QUEUE_PORT_MAP             "COUNTERS_QUEUE_PORT_MAP"
#define COUNTERS_QUEUE_INDEX_MAP            "COUNTERS_QUEUE_INDEX_MAP"
#define COUNTERS_QUEUE_TYPE_MAP             "COUNTERS_QUEUE_TYPE_MAP"
#define COUNTERS_PG_NAME_MAP                "COUNTERS_PG_NAME_MAP"
#define COUNTERS_PG_PORT_MAP                "COUNTERS_PG_PORT_MAP"
#define COUNTERS_PG_INDEX_MAP               "COUNTERS_PG_INDEX_MAP"
#define COUNTERS_RIF_TYPE_MAP               "COUNTERS_RIF_TYPE_MAP"
#define COUNTERS_RIF_NAME_MAP               "COUNTERS_RIF_NAME_MAP"
#define COUNTERS_TRAP_NAME_MAP              "COUNTERS_TRAP_NAME_MAP"
#define COUNTERS_CRM_TABLE                  "CRM"
#define COUNTERS_BUFFER_POOL_NAME_MAP       "COUNTERS_BUFFER_POOL_NAME_MAP"
#define COUNTERS_SWITCH_NAME_MAP            "COUNTERS_SWITCH_NAME_MAP"
#define COUNTERS_MACSEC_NAME_MAP            "COUNTERS_MACSEC_NAME_MAP"
#define COUNTERS_MACSEC_FLOW_TX_NAME_MAP    "COUNTERS_MACSEC_FLOW_TX_NAME_MAP"
#define COUNTERS_MACSEC_FLOW_RX_NAME_MAP    "COUNTERS_MACSEC_FLOW_RX_NAME_MAP"
#define COUNTERS_MACSEC_SA_TX_NAME_MAP      "COUNTERS_MACSEC_SA_TX_NAME_MAP"
#define COUNTERS_MACSEC_SA_RX_NAME_MAP      "COUNTERS_MACSEC_SA_RX_NAME_MAP"
#define COUNTERS_DEBUG_NAME_PORT_STAT_MAP   "COUNTERS_DEBUG_NAME_PORT_STAT_MAP"
#define COUNTERS_DEBUG_NAME_SWITCH_STAT_MAP "COUNTERS_DEBUG_NAME_SWITCH_STAT_MAP"
#define COUNTERS_TUNNEL_TYPE_MAP            "COUNTERS_TUNNEL_TYPE_MAP"
#define COUNTERS_TUNNEL_NAME_MAP            "COUNTERS_TUNNEL_NAME_MAP"
#define COUNTERS_ROUTE_NAME_MAP             "COUNTERS_ROUTE_NAME_MAP"
#define COUNTERS_ROUTE_TO_PATTERN_MAP       "COUNTERS_ROUTE_TO_PATTERN_MAP"
#define COUNTERS_FABRIC_QUEUE_NAME_MAP      "COUNTERS_FABRIC_QUEUE_NAME_MAP"
#define COUNTERS_FABRIC_PORT_NAME_MAP       "COUNTERS_FABRIC_PORT_NAME_MAP"
#define COUNTERS_TWAMP_SESSION_NAME_MAP     "COUNTERS_TWAMP_SESSION_NAME_MAP"

#define COUNTERS_NAT_TABLE              "COUNTERS_NAT"
#define COUNTERS_NAPT_TABLE             "COUNTERS_NAPT"
#define COUNTERS_TWICE_NAT_TABLE        "COUNTERS_TWICE_NAT"
#define COUNTERS_TWICE_NAPT_TABLE       "COUNTERS_TWICE_NAPT"
#define COUNTERS_GLOBAL_NAT_TABLE       "COUNTERS_GLOBAL_NAT"

#define COUNTERS_EVENTS_TABLE           "COUNTERS_EVENTS"

#define PERIODIC_WATERMARKS_TABLE      "PERIODIC_WATERMARKS"
#define PERSISTENT_WATERMARKS_TABLE    "PERSISTENT_WATERMARKS"
#define USER_WATERMARKS_TABLE          "USER_WATERMARKS"

#define RATES_TABLE                         "RATES"

/***** EVENTS COUNTER KEYS *****/
#define COUNTERS_EVENTS_PUBLISHED           "published"
#define COUNTERS_EVENTS_MISSED_SLOW_RCVR    "missed_by_slow_receiver"
#define COUNTERS_EVENTS_MISSED_INTERNAL     "missed_internal"
#define COUNTERS_EVENTS_MISSED_CACHE        "missed_to_cache"
#define COUNTERS_EVENTS_LATENCY             "latency_in_ms"


/***** FLEX COUNTER DATABASE *****/

#define PFC_WD_POLL_MSECS               100
#define FLEX_COUNTER_TABLE              "FLEX_COUNTER_TABLE"
#define PORT_COUNTER_ID_LIST            "PORT_COUNTER_ID_LIST"
#define PORT_DEBUG_COUNTER_ID_LIST      "PORT_DEBUG_COUNTER_ID_LIST"
#define QUEUE_COUNTER_ID_LIST           "QUEUE_COUNTER_ID_LIST"
#define QUEUE_ATTR_ID_LIST              "QUEUE_ATTR_ID_LIST"
#define BUFFER_POOL_COUNTER_ID_LIST     "BUFFER_POOL_COUNTER_ID_LIST"
#define PFC_WD_STATE_TABLE              "PFC_WD_STATE_TABLE"
#define PFC_WD_PORT_COUNTER_ID_LIST     "PORT_COUNTER_ID_LIST"
#define PFC_WD_QUEUE_COUNTER_ID_LIST    "QUEUE_COUNTER_ID_LIST"
#define PFC_WD_QUEUE_ATTR_ID_LIST       "QUEUE_ATTR_ID_LIST"
#define PG_COUNTER_ID_LIST              "PG_COUNTER_ID_LIST"
#define PG_ATTR_ID_LIST                 "PG_ATTR_ID_LIST"
#define RIF_COUNTER_ID_LIST             "RIF_COUNTER_ID_LIST"
#define TUNNEL_COUNTER_ID_LIST          "TUNNEL_COUNTER_ID_LIST"
#define SWITCH_DEBUG_COUNTER_ID_LIST    "SWITCH_DEBUG_COUNTER_ID_LIST"
#define MACSEC_FLOW_COUNTER_ID_LIST     "MACSEC_FLOW_COUNTER_ID_LIST"
#define MACSEC_SA_COUNTER_ID_LIST       "MACSEC_SA_COUNTER_ID_LIST"
#define MACSEC_SA_ATTR_ID_LIST          "MACSEC_SA_ATTR_ID_LIST"
#define TUNNEL_ATTR_ID_LIST             "TUNNEL_ATTR_ID_LIST"
#define ACL_COUNTER_ATTR_ID_LIST        "ACL_COUNTER_ATTR_ID_LIST"
#define FLOW_COUNTER_ID_LIST            "FLOW_COUNTER_ID_LIST"
#define PLUGIN_TABLE                    "PLUGIN_TABLE"
#define LUA_PLUGIN_TYPE                 "LUA_PLUGIN_TYPE"
#define SAI_OBJECT_TYPE                 "SAI_OBJECT_TYPE"

#define POLL_INTERVAL_FIELD                 "POLL_INTERVAL"
#define STATS_MODE_FIELD                    "STATS_MODE"
#define STATS_MODE_READ                     "STATS_MODE_READ"
#define STATS_MODE_READ_AND_CLEAR           "STATS_MODE_READ_AND_CLEAR"
#define QUEUE_PLUGIN_FIELD                  "QUEUE_PLUGIN_LIST"
#define PORT_PLUGIN_FIELD                   "PORT_PLUGIN_LIST"
#define WRED_QUEUE_PLUGIN_FIELD             "WRED_QUEUE_PLUGIN_LIST"
#define WRED_PORT_PLUGIN_FIELD              "WRED_PORT_PLUGIN_LIST"
#define MACSEC_SA_PLUGIN_FIELD              "MACSEC_SA_PLUGIN_LIST"
#define RIF_PLUGIN_FIELD                    "RIF_PLUGIN_LIST"
#define PG_PLUGIN_FIELD                     "PG_PLUGIN_LIST"
#define TUNNEL_PLUGIN_FIELD                 "TUNNEL_PLUGIN_LIST"
#define BUFFER_POOL_PLUGIN_FIELD            "BUFFER_POOL_PLUGIN_LIST"
#define FLOW_COUNTER_PLUGIN_FIELD           "FLOW_COUNTER_PLUGIN_FIELD"
#define FLEX_COUNTER_STATUS_FIELD           "FLEX_COUNTER_STATUS"
#define FLEX_COUNTER_GROUP_TABLE            "FLEX_COUNTER_GROUP_TABLE"
#define FLEX_COUNTER_DELAY_STATUS_FIELD     "FLEX_COUNTER_DELAY_STATUS"

/***** CONFIGURATION DATABASE *****/

#define CFG_PORT_TABLE_NAME           "PORT"
#define CFG_PORT_CABLE_LEN_TABLE_NAME "CABLE_LENGTH"

#define CFG_SEND_TO_INGRESS_PORT_TABLE_NAME  "SEND_TO_INGRESS_PORT"

#define CFG_GEARBOX_TABLE_NAME           "GEARBOX"

#define CFG_INTF_TABLE_NAME               "INTERFACE"
#define CFG_LOOPBACK_INTERFACE_TABLE_NAME "LOOPBACK_INTERFACE"
#define CFG_MGMT_INTERFACE_TABLE_NAME     "MGMT_INTERFACE"
#define CFG_LAG_INTF_TABLE_NAME           "PORTCHANNEL_INTERFACE"
#define CFG_VLAN_INTF_TABLE_NAME          "VLAN_INTERFACE"
#define CFG_VLAN_SUB_INTF_TABLE_NAME      "VLAN_SUB_INTERFACE"

#define CFG_ASIC_SENSORS_TABLE_NAME       "ASIC_SENSORS"

#define CFG_LAG_TABLE_NAME         "PORTCHANNEL"
#define CFG_LAG_MEMBER_TABLE_NAME  "PORTCHANNEL_MEMBER"
#define CFG_VLAN_TABLE_NAME        "VLAN"
#define CFG_VLAN_MEMBER_TABLE_NAME "VLAN_MEMBER"
#define CFG_VLAN_STACKING_TABLE_NAME  "VLAN_STACKING"
#define CFG_VLAN_TRANSLATION_TABLE_NAME   "VLAN_TRANSLATION"
#define CFG_FDB_TABLE_NAME         "FDB"
#define CFG_SWITCH_TABLE_NAME      "SWITCH"
#define CFG_VRF_TABLE_NAME         "VRF"
#define CFG_CRM_TABLE_NAME         "CRM"
#define CFG_MGMT_VRF_CONFIG_TABLE_NAME    "MGMT_VRF_CONFIG"

#define CFG_DHCP_SERVER_TABLE_NAME   "DHCP_SERVER"
#define CFG_NTP_GLOBAL_TABLE_NAME    "NTP"
#define CFG_NTP_SERVER_TABLE_NAME    "NTP_SERVER"
#define CFG_NTP_KEY_TABLE_NAME       "NTP_KEY"
#define CFG_SYSLOG_SERVER_TABLE_NAME "SYSLOG_SERVER"
#define CFG_SYSLOG_CONFIG_TABLE_NAME "SYSLOG_CONFIG"

#define CFG_BGP_NEIGHBOR_TABLE_NAME "BGP_NEIGHBOR"
#define CFG_BGP_INTERNAL_NEIGHBOR_TABLE_NAME "BGP_INTERNAL_NEIGHBOR"
#define CFG_BGP_MONITORS_TABLE_NAME "BGP_MONITORS"
#define CFG_BGP_PEER_RANGE_TABLE_NAME "BGP_PEER_RANGE"
#define CFG_BGP_DEVICE_GLOBAL_TABLE_NAME "BGP_DEVICE_GLOBAL"
#define CFG_BMP_TABLE_NAME "BMP"

#define CFG_SWITCH_HASH_TABLE_NAME     "SWITCH_HASH"
#define CFG_DEVICE_METADATA_TABLE_NAME "DEVICE_METADATA"

#define CFG_DEVICE_NEIGHBOR_TABLE_NAME          "DEVICE_NEIGHBOR"
#define CFG_DEVICE_NEIGHBOR_METADATA_TABLE_NAME "DEVICE_NEIGHBOR_METADATA"

#define CFG_MIRROR_SESSION_TABLE_NAME "MIRROR_SESSION"
#define CFG_ACL_TABLE_TABLE_NAME      "ACL_TABLE"
#define CFG_ACL_TABLE_TYPE_TABLE_NAME "ACL_TABLE_TYPE"
#define CFG_ACL_RULE_TABLE_NAME       "ACL_RULE"
#define CFG_PFC_WD_TABLE_NAME         "PFC_WD"
#define CFG_FLEX_COUNTER_TABLE_NAME "FLEX_COUNTER_TABLE"
#define CFG_WATERMARK_TABLE_NAME "WATERMARK_TABLE"

#define CFG_PBH_TABLE_TABLE_NAME      "PBH_TABLE"
#define CFG_PBH_RULE_TABLE_NAME       "PBH_RULE"
#define CFG_PBH_HASH_TABLE_NAME       "PBH_HASH"
#define CFG_PBH_HASH_FIELD_TABLE_NAME "PBH_HASH_FIELD"

#define CFG_PFC_PRIORITY_TO_PRIORITY_GROUP_MAP_TABLE_NAME "PFC_PRIORITY_TO_PRIORITY_GROUP_MAP"
#define CFG_TC_TO_PRIORITY_GROUP_MAP_TABLE_NAME     "TC_TO_PRIORITY_GROUP_MAP"
#define CFG_PFC_PRIORITY_TO_QUEUE_MAP_TABLE_NAME    "MAP_PFC_PRIORITY_TO_QUEUE"
#define CFG_TC_TO_QUEUE_MAP_TABLE_NAME              "TC_TO_QUEUE_MAP"
#define CFG_DSCP_TO_TC_MAP_TABLE_NAME               "DSCP_TO_TC_MAP"
#define CFG_MPLS_TC_TO_TC_MAP_TABLE_NAME            "MPLS_TC_TO_TC_MAP"
#define CFG_SCHEDULER_TABLE_NAME                    "SCHEDULER"
#define CFG_PORT_QOS_MAP_TABLE_NAME                 "PORT_QOS_MAP"
#define CFG_WRED_PROFILE_TABLE_NAME                 "WRED_PROFILE"
#define CFG_QUEUE_TABLE_NAME                        "QUEUE"
#define CFG_DOT1P_TO_TC_MAP_TABLE_NAME              "DOT1P_TO_TC_MAP"
#define CFG_DSCP_TO_FC_MAP_TABLE_NAME               "DSCP_TO_FC_MAP"
#define CFG_EXP_TO_FC_MAP_TABLE_NAME                "EXP_TO_FC_MAP"
#define CFG_TC_TO_DSCP_MAP_TABLE_NAME               "TC_TO_DSCP_MAP"
#define CFG_TC_TO_DOT1P_MAP_TABLE_NAME              "TC_TO_DOT1P_MAP"

#define CFG_BUFFER_POOL_TABLE_NAME                  "BUFFER_POOL"
#define CFG_BUFFER_PROFILE_TABLE_NAME               "BUFFER_PROFILE"
#define CFG_BUFFER_QUEUE_TABLE_NAME                 "BUFFER_QUEUE"
#define CFG_BUFFER_PG_TABLE_NAME                    "BUFFER_PG"
#define CFG_BUFFER_PORT_INGRESS_PROFILE_LIST_NAME   "BUFFER_PORT_INGRESS_PROFILE_LIST"
#define CFG_BUFFER_PORT_EGRESS_PROFILE_LIST_NAME    "BUFFER_PORT_EGRESS_PROFILE_LIST"

#define CFG_DEFAULT_LOSSLESS_BUFFER_PARAMETER       "DEFAULT_LOSSLESS_BUFFER_PARAMETER"

#define CFG_POLICER_TABLE_NAME                      "POLICER"

#define CFG_WARM_RESTART_TABLE_NAME                 "WARM_RESTART"

#define CFG_VXLAN_TUNNEL_TABLE_NAME                 "VXLAN_TUNNEL"
#define CFG_VXLAN_TUNNEL_MAP_TABLE_NAME             "VXLAN_TUNNEL_MAP"
#define CFG_VXLAN_EVPN_NVO_TABLE_NAME               "VXLAN_EVPN_NVO"
#define CFG_VNET_TABLE_NAME                         "VNET"
#define CFG_NEIGH_TABLE_NAME                        "NEIGH"
#define CFG_NEIGH_SUPPRESS_VLAN_TABLE_NAME          "SUPPRESS_VLAN_NEIGH"

#define CFG_VNET_RT_TABLE_NAME                      "VNET_ROUTE"
#define CFG_VNET_RT_TUNNEL_TABLE_NAME               "VNET_ROUTE_TUNNEL"

#define CFG_NVGRE_TUNNEL_TABLE_NAME                 "NVGRE_TUNNEL"
#define CFG_NVGRE_TUNNEL_MAP_TABLE_NAME             "NVGRE_TUNNEL_MAP"

#define CFG_PASS_THROUGH_ROUTE_TABLE_NAME           "PASS_THROUGH_ROUTE_TABLE"
#define CFG_SFLOW_TABLE_NAME                        "SFLOW"
#define CFG_SFLOW_SESSION_TABLE_NAME                "SFLOW_SESSION"

#define CFG_DEBUG_COUNTER_TABLE_NAME                "DEBUG_COUNTER"
#define CFG_DEBUG_COUNTER_DROP_REASON_TABLE_NAME    "DEBUG_COUNTER_DROP_REASON"

#define CFG_STATIC_NAT_TABLE_NAME                   "STATIC_NAT"
#define CFG_STATIC_NAPT_TABLE_NAME                  "STATIC_NAPT"
#define CFG_NAT_POOL_TABLE_NAME                     "NAT_POOL"
#define CFG_NAT_BINDINGS_TABLE_NAME                 "NAT_BINDINGS"
#define CFG_NAT_GLOBAL_TABLE_NAME                   "NAT_GLOBAL"

#define CFG_STP_GLOBAL_TABLE_NAME                   "STP"
#define CFG_STP_VLAN_TABLE_NAME                     "STP_VLAN"
#define CFG_STP_VLAN_PORT_TABLE_NAME                "STP_VLAN_PORT"
#define CFG_STP_PORT_TABLE_NAME                     "STP_PORT"

#define CFG_MCLAG_TABLE_NAME                        "MCLAG_DOMAIN"
#define CFG_MCLAG_INTF_TABLE_NAME                   "MCLAG_INTERFACE"
#define CFG_MCLAG_UNIQUE_IP_TABLE_NAME              "MCLAG_UNIQUE_IP"

#define CFG_PORT_STORM_CONTROL_TABLE_NAME           "PORT_STORM_CONTROL"

#define CFG_RATES_TABLE_NAME                        "RATES"

#define CFG_FEATURE_TABLE_NAME                      "FEATURE"

#define CFG_COPP_TRAP_TABLE_NAME                    "COPP_TRAP"
#define CFG_COPP_GROUP_TABLE_NAME                   "COPP_GROUP"

#define CFG_FG_NHG                                  "FG_NHG"
#define CFG_FG_NHG_PREFIX                           "FG_NHG_PREFIX"
#define CFG_FG_NHG_MEMBER                           "FG_NHG_MEMBER"

#define CFG_MUX_CABLE_TABLE_NAME                    "MUX_CABLE"
#define CFG_MUX_LINKMGR_TABLE_NAME                  "MUX_LINKMGR"

#define CFG_PEER_SWITCH_TABLE_NAME                  "PEER_SWITCH"

#define CFG_TUNNEL_TABLE_NAME                       "TUNNEL"
#define CFG_SUBNET_DECAP_TABLE_NAME                 "SUBNET_DECAP"

#define CFG_SYSTEM_PORT_TABLE_NAME                  "SYSTEM_PORT"
#define CFG_VOQ_INBAND_INTERFACE_TABLE_NAME         "VOQ_INBAND_INTERFACE"

#define CFG_MACSEC_PROFILE_TABLE_NAME               "MACSEC_PROFILE"

#define CHASSIS_APP_SYSTEM_INTERFACE_TABLE_NAME     "SYSTEM_INTERFACE"
#define CHASSIS_APP_SYSTEM_NEIGH_TABLE_NAME         "SYSTEM_NEIGH"
#define CHASSIS_APP_LAG_TABLE_NAME                  "SYSTEM_LAG_TABLE"
#define CHASSIS_APP_LAG_MEMBER_TABLE_NAME           "SYSTEM_LAG_MEMBER_TABLE"

#define CFG_CHASSIS_MODULE_TABLE                    "CHASSIS_MODULE"

#define CFG_TWAMP_SESSION_TABLE_NAME                "TWAMP_SESSION"
#define CFG_BANNER_MESSAGE_TABLE_NAME               "BANNER_MESSAGE"

#define CFG_LOCAL_USERS_PASSWORDS_RESET             "LOCAL_USERS_PASSWORDS_RESET"

#define CFG_DHCP_TABLE                              "DHCP_RELAY"

#define CFG_FLOW_COUNTER_ROUTE_PATTERN_TABLE_NAME   "FLOW_COUNTER_ROUTE_PATTERN"
#define CFG_LOGGER_TABLE_NAME                       "LOGGER"

#define CFG_SAG_TABLE_NAME                          "SAG"

#define CFG_SUPPRESS_ASIC_SDK_HEALTH_EVENT_NAME     "SUPPRESS_ASIC_SDK_HEALTH_EVENT"

/***** STATE DATABASE *****/

#define STATE_SWITCH_CAPABILITY_TABLE_NAME          "SWITCH_CAPABILITY"
#define STATE_ACL_STAGE_CAPABILITY_TABLE_NAME       "ACL_STAGE_CAPABILITY_TABLE"
#define STATE_PBH_CAPABILITIES_TABLE_NAME           "PBH_CAPABILITIES"
#define STATE_PORT_TABLE_NAME                       "PORT_TABLE"
#define STATE_LAG_TABLE_NAME                        "LAG_TABLE"
#define STATE_VLAN_TABLE_NAME                       "VLAN_TABLE"
#define STATE_VLAN_MEMBER_TABLE_NAME                "VLAN_MEMBER_TABLE"
#define STATE_INTERFACE_TABLE_NAME                  "INTERFACE_TABLE"
#define STATE_FDB_TABLE_NAME                        "FDB_TABLE"
#define STATE_WARM_RESTART_TABLE_NAME               "WARM_RESTART_TABLE"
#define STATE_WARM_RESTART_ENABLE_TABLE_NAME        "WARM_RESTART_ENABLE_TABLE"
#define STATE_VRF_TABLE_NAME                        "VRF_TABLE"
#define STATE_VRF_OBJECT_TABLE_NAME                 "VRF_OBJECT_TABLE"
#define STATE_MGMT_PORT_TABLE_NAME                  "MGMT_PORT_TABLE"
#define STATE_NEIGH_RESTORE_TABLE_NAME              "NEIGH_RESTORE_TABLE"
#define STATE_MIRROR_SESSION_TABLE_NAME             "MIRROR_SESSION_TABLE"
#define STATE_VXLAN_TABLE_NAME                      "VXLAN_TABLE"
#define STATE_VXLAN_TUNNEL_TABLE_NAME               "VXLAN_TUNNEL_TABLE"
#define STATE_NEIGH_SUPPRESS_VLAN_TABLE_NAME        "SUPPRESS_VLAN_NEIGH_TABLE"
#define STATE_BGP_TABLE_NAME                        "BGP_STATE_TABLE"
#define STATE_DEBUG_COUNTER_CAPABILITIES_NAME       "DEBUG_COUNTER_CAPABILITIES"
#define STATE_NAT_RESTORE_TABLE_NAME                "NAT_RESTORE_TABLE"
#define STATE_MCLAG_TABLE_NAME                      "MCLAG_TABLE"
#define STATE_MCLAG_LOCAL_INTF_TABLE_NAME           "MCLAG_LOCAL_INTF_TABLE"
#define STATE_MCLAG_REMOTE_INTF_TABLE_NAME          "MCLAG_REMOTE_INTF_TABLE"
#define STATE_MCLAG_REMOTE_FDB_TABLE_NAME           "MCLAG_REMOTE_FDB_TABLE"

#define STATE_STP_TABLE_NAME                        "STP_TABLE"
#define STATE_BUM_STORM_CAPABILITY_TABLE_NAME       "BUM_STORM_CAPABILITY_TABLE"

#define STATE_COPP_GROUP_TABLE_NAME                 "COPP_GROUP_TABLE"
#define STATE_COPP_TRAP_TABLE_NAME                  "COPP_TRAP_TABLE"
#define STATE_FG_ROUTE_TABLE_NAME                   "FG_ROUTE_TABLE"

#define STATE_MUX_CABLE_TABLE_NAME                  "MUX_CABLE_TABLE"
#define STATE_HW_MUX_CABLE_TABLE_NAME               "HW_MUX_CABLE_TABLE"
#define STATE_MUX_LINKMGR_TABLE_NAME                "MUX_LINKMGR_TABLE"
#define STATE_MUX_METRICS_TABLE_NAME                "MUX_METRICS_TABLE"
#define STATE_MUX_CABLE_INFO_TABLE_NAME             "MUX_CABLE_INFO"
#define STATE_LINK_PROBE_STATS_TABLE_NAME           "LINK_PROBE_STATS"

#define STATE_PEER_MUX_METRICS_TABLE_NAME           "MUX_METRICS_TABLE_PEER"
#define STATE_PEER_HW_FORWARDING_STATE_TABLE_NAME   "HW_MUX_CABLE_TABLE_PEER"

#define STATE_SYSTEM_NEIGH_TABLE_NAME               "SYSTEM_NEIGH_TABLE"

#define STATE_TWAMP_SESSION_TABLE_NAME              "TWAMP_SESSION_TABLE"

#define STATE_MACSEC_PORT_TABLE_NAME        "MACSEC_PORT_TABLE"
#define STATE_MACSEC_INGRESS_SC_TABLE_NAME  "MACSEC_INGRESS_SC_TABLE"
#define STATE_MACSEC_INGRESS_SA_TABLE_NAME  "MACSEC_INGRESS_SA_TABLE"
#define STATE_MACSEC_EGRESS_SC_TABLE_NAME   "MACSEC_EGRESS_SC_TABLE"
#define STATE_MACSEC_EGRESS_SA_TABLE_NAME   "MACSEC_EGRESS_SA_TABLE"

#define STATE_ASIC_TABLE                            "ASIC_TABLE"
#define STATE_BUFFER_MAXIMUM_VALUE_TABLE            "BUFFER_MAX_PARAM_TABLE"
#define STATE_PERIPHERAL_TABLE                      "PERIPHERAL_TABLE"
#define STATE_PORT_PERIPHERAL_TABLE                 "PORT_PERIPHERAL_TABLE"
#define STATE_BUFFER_POOL_TABLE_NAME                "BUFFER_POOL_TABLE"
#define STATE_BUFFER_PROFILE_TABLE_NAME             "BUFFER_PROFILE_TABLE"
#define STATE_DHCPv6_COUNTER_TABLE_NAME             "DHCPv6_COUNTER_TABLE"

#define STATE_TUNNEL_DECAP_TABLE_NAME               "TUNNEL_DECAP_TABLE"
#define STATE_TUNNEL_DECAP_TERM_TABLE_NAME          "TUNNEL_DECAP_TERM_TABLE"

#define STATE_BFD_SESSION_TABLE_NAME                "BFD_SESSION_TABLE"
#define STATE_ROUTE_TABLE_NAME                      "ROUTE_TABLE"
#define STATE_VNET_RT_TUNNEL_TABLE_NAME             "VNET_ROUTE_TUNNEL_TABLE"
#define STATE_ADVERTISE_NETWORK_TABLE_NAME          "ADVERTISE_NETWORK_TABLE"

#define STATE_FLOW_COUNTER_CAPABILITY_TABLE_NAME    "FLOW_COUNTER_CAPABILITY_TABLE"

#define STATE_VNET_MONITOR_TABLE_NAME          "VNET_MONITOR_TABLE"

#define STATE_TRANSCEIVER_INFO_TABLE_NAME           "TRANSCEIVER_INFO"

#define STATE_ASIC_SDK_HEALTH_EVENT_TABLE_NAME      "ASIC_SDK_HEALTH_EVENT_TABLE"

// ACL table and ACL rule table
#define STATE_ACL_TABLE_TABLE_NAME                  "ACL_TABLE_TABLE"
#define STATE_ACL_RULE_TABLE_NAME                   "ACL_RULE_TABLE"

/***** Counter capability tables for Queue and Port ****/
#define STATE_QUEUE_COUNTER_CAPABILITIES_NAME   "QUEUE_COUNTER_CAPABILITIES"
#define STATE_PORT_COUNTER_CAPABILITIES_NAME    "PORT_COUNTER_CAPABILITIES"

/***** PROFILE DATABASE *****/

#define PROFILE_DELETE_TABLE                  "PROFILE_DELETE"

/***** MISC *****/

#define IPV4_NAME "IPv4"
#define IPV6_NAME "IPv6"

#define FRONT_PANEL_PORT_PREFIX "Ethernet"
#define PORTCHANNEL_PREFIX      "PortChannel"
#define VLAN_PREFIX             "Vlan"

#define SET_COMMAND "SET"
#define DEL_COMMAND "DEL"
#define EMPTY_PREFIX ""

#define CFG_DTEL_TABLE_NAME					"DTEL"
#define CFG_DTEL_REPORT_SESSION_TABLE_NAME	"DTEL_REPORT_SESSION"
#define CFG_DTEL_INT_SESSION_TABLE_NAME		"DTEL_INT_SESSION"
#define CFG_DTEL_QUEUE_REPORT_TABLE_NAME	"DTEL_QUEUE_REPORT"
#define CFG_DTEL_EVENT_TABLE_NAME			"DTEL_EVENT"

#define CFG_FABRIC_MONITOR_DATA_TABLE_NAME	"FABRIC_MONITOR"
#define CFG_FABRIC_MONITOR_PORT_TABLE_NAME	"FABRIC_PORT"
#define APP_FABRIC_MONITOR_DATA_TABLE_NAME	"FABRIC_MONITOR_TABLE"
#define APP_FABRIC_MONITOR_PORT_TABLE_NAME	"FABRIC_PORT_TABLE"

#define EVENT_HISTORY_TABLE_NAME          "EVENT"
#define EVENT_CURRENT_ALARM_TABLE_NAME    "ALARM"
#define EVENT_STATS_TABLE_NAME            "EVENT_STATS"
#define EVENT_ALARM_STATS_TABLE_NAME      "ALARM_STATS"

/***** BMP STATE DATABASE *****/
#define BMP_STATE_BGP_NEIGHBOR_TABLE             "BGP_NEIGHBOR_TABLE"
#define BMP_STATE_BGP_RIB_IN_TABLE               "BGP_RIB_IN_TABLE"
#define BMP_STATE_BGP_RIB_OUT_TABLE              "BGP_RIB_OUT_TABLE"

#ifdef __cplusplus
}
#endif

#endif
