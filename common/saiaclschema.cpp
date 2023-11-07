#include "saiaclschema.h"

#include <stdexcept>

namespace swss
{
namespace acl
{

// ENUM Stage strings
static constexpr char kStagePreIngressName[] = "PRE_INGRESS";
static constexpr char kStageIngressName[] = "INGRESS";
static constexpr char kStageEgressName[] = "EGRESS";

// ENUM Format strings
static constexpr char kFormatNone[] = "NONE";
static constexpr char kFormatHexString[] = "HEX_STRING";
static constexpr char kFormatMac[] = "MAC";
static constexpr char kFormatIPv4[] = "IPV4";
static constexpr char kFormatIPv6[] = "IPV6";
static constexpr char kFormatString[] = "STRING";

Stage StageFromName(const std::string &name)
{
    if (name == kStagePreIngressName)
        return Stage::kPreIngress;
    if (name == kStageIngressName)
        return Stage::kIngress;
    if (name == kStageEgressName)
        return Stage::kEgress;
    throw std::invalid_argument("Unknown stage enum: " + name);
}

const std::string &StageName(Stage stage)
{
    static const auto *const kPreIngressString = new std::string(kStagePreIngressName);
    static const auto *const kIngressString = new std::string(kStageIngressName);
    static const auto *const kEgressString = new std::string(kStageEgressName);
    switch (stage)
    {
    case Stage::kPreIngress:
        return *kPreIngressString;
    case Stage::kIngress:
        return *kIngressString;
    case Stage::kEgress:
        return *kEgressString;
    default:
        break;
    }
    throw std::invalid_argument(std::string(__func__) + " called with an invalid enum value.");
}

Format FormatFromName(const std::string &name)
{
    if (name == kFormatNone)
        return Format::kNone;
    if (name == kFormatHexString)
        return Format::kHexString;
    if (name == kFormatMac)
        return Format::kMac;
    if (name == kFormatIPv4)
        return Format::kIPv4;
    if (name == kFormatIPv6)
        return Format::kIPv6;
    if (name == kFormatString)
        return Format::kString;
    throw std::invalid_argument("Unknown format enum: " + name);
}

const std::string &FormatName(Format format)
{
    static const auto *const kNoneString = new std::string(kFormatNone);
    static const auto *const kHexStringString = new std::string(kFormatHexString);
    static const auto *const kMacString = new std::string(kFormatMac);
    static const auto *const kIPv4String = new std::string(kFormatIPv4);
    static const auto *const kIPv6String = new std::string(kFormatIPv6);
    static const auto *const kStringString = new std::string(kFormatString);
    switch (format)
    {
    case Format::kNone:
        return *kNoneString;
    case Format::kHexString:
        return *kHexStringString;
    case Format::kMac:
        return *kMacString;
    case Format::kIPv4:
        return *kIPv4String;
    case Format::kIPv6:
        return *kIPv6String;
    case Format::kString:
        return *kStringString;
    default:
        break;
    }
    throw std::invalid_argument(std::string(__func__) + " called with an invalid enum value.");
}

const MatchFieldSchema &MatchFieldSchemaByName(const std::string &match_field_name)
{
    static const auto *const kMatchFields = new std::unordered_map<std::string, MatchFieldSchema>({
        {"SAI_ACL_TABLE_ATTR_FIELD_SRC_IPV6",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress}, .format = Format::kIPv6, .bitwidth = 128}},
        {"SAI_ACL_TABLE_ATTR_FIELD_SRC_IPV6_WORD3",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress}, .format = Format::kIPv6, .bitwidth = 32}},
        {"SAI_ACL_TABLE_ATTR_FIELD_SRC_IPV6_WORD2",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress}, .format = Format::kIPv6, .bitwidth = 32}},
        {"SAI_ACL_TABLE_ATTR_FIELD_SRC_IPV6_WORD1",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress}, .format = Format::kIPv6, .bitwidth = 32}},
        {"SAI_ACL_TABLE_ATTR_FIELD_SRC_IPV6_WORD0",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress}, .format = Format::kIPv6, .bitwidth = 32}},
        {"SAI_ACL_TABLE_ATTR_FIELD_DST_IPV6",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress}, .format = Format::kIPv6, .bitwidth = 128}},
        {"SAI_ACL_TABLE_ATTR_FIELD_DST_IPV6_WORD3",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress}, .format = Format::kIPv6, .bitwidth = 32}},
        {"SAI_ACL_TABLE_ATTR_FIELD_DST_IPV6_WORD2",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress}, .format = Format::kIPv6, .bitwidth = 32}},
        {"SAI_ACL_TABLE_ATTR_FIELD_DST_IPV6_WORD1",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress}, .format = Format::kIPv6, .bitwidth = 32}},
        {"SAI_ACL_TABLE_ATTR_FIELD_DST_IPV6_WORD0",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress}, .format = Format::kIPv6, .bitwidth = 32}},
        // SAI_ACL_TABLE_ATTR_FIELD_INNER_SRC_IPV6
        // SAI_ACL_TABLE_ATTR_FIELD_INNER_DST_IPV6
        {"SAI_ACL_TABLE_ATTR_FIELD_SRC_MAC",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress}, .format = Format::kMac, .bitwidth = 48}},
        {"SAI_ACL_TABLE_ATTR_FIELD_DST_MAC",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress}, .format = Format::kMac, .bitwidth = 48}},
        {"SAI_ACL_TABLE_ATTR_FIELD_SRC_IP",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress}, .format = Format::kIPv4, .bitwidth = 32}},
        {"SAI_ACL_TABLE_ATTR_FIELD_DST_IP",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress}, .format = Format::kIPv4, .bitwidth = 32}},
        // SAI_ACL_TABLE_ATTR_FIELD_INNER_SRC_IP
        // SAI_ACL_TABLE_ATTR_FIELD_INNER_DST_IP
        // SAI_ACL_TABLE_ATTR_FIELD_IN_PORTS
        // SAI_ACL_TABLE_ATTR_FIELD_OUT_PORTS
        {"SAI_ACL_TABLE_ATTR_FIELD_IN_PORT",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress}, .format = Format::kString, .bitwidth = 0}},
        {"SAI_ACL_TABLE_ATTR_FIELD_OUT_PORT",
         {.stages = {Stage::kIngress, Stage::kEgress}, .format = Format::kString, .bitwidth = 0}},
        // SAI_ACL_TABLE_ATTR_FIELD_SRC_PORT
        {"SAI_ACL_TABLE_ATTR_FIELD_OUTER_VLAN_ID",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 12}},
        {"SAI_ACL_TABLE_ATTR_FIELD_OUTER_VLAN_PRI",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 3}},
        {"SAI_ACL_TABLE_ATTR_FIELD_OUTER_VLAN_CFI",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 1}},
        {"SAI_ACL_TABLE_ATTR_FIELD_INNER_VLAN_ID",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 12}},
        {"SAI_ACL_TABLE_ATTR_FIELD_INNER_VLAN_PRI",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 3}},
        {"SAI_ACL_TABLE_ATTR_FIELD_INNER_VLAN_CFI",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 1}},
        {"SAI_ACL_TABLE_ATTR_FIELD_L4_SRC_PORT",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 16}},
        {"SAI_ACL_TABLE_ATTR_FIELD_L4_DST_PORT",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 16}},
        // SAI_ACL_TABLE_ATTR_FIELD_INNER_L4_SRC_PORT
        // SAI_ACL_TABLE_ATTR_FIELD_INNER_L4_DST_PORT
        {"SAI_ACL_TABLE_ATTR_FIELD_ETHER_TYPE",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 16}},
        // SAI_ACL_TABLE_ATTR_FIELD_INNER_ETHER_TYPE
        {"SAI_ACL_TABLE_ATTR_FIELD_IP_PROTOCOL",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 8}},
        // SAI_ACL_TABLE_ATTR_FIELD_INNER_IP_PROTOCOL
        // SAI_ACL_TABLE_ATTR_FIELD_IP_IDENTIFICATION
        {"SAI_ACL_TABLE_ATTR_FIELD_DSCP",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 6}},
        {"SAI_ACL_TABLE_ATTR_FIELD_ECN",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 2}},
        {"SAI_ACL_TABLE_ATTR_FIELD_TTL",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 8}},
        {"SAI_ACL_TABLE_ATTR_FIELD_TOS",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 8}},
        {"SAI_ACL_TABLE_ATTR_FIELD_IP_FLAGS",
         {.stages = {Stage::kIngress}, .format = Format::kHexString, .bitwidth = 3}},
        {"SAI_ACL_TABLE_ATTR_FIELD_TCP_FLAGS",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 6}},
        {"SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE/ANY",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 1}},
        {"SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE/IP",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 1}},
        {"SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE/NON_IP",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 1}},
        {"SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE/IPV4ANY",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 1}},
        {"SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE/NON_IPV4",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 1}},
        {"SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE/IPV6ANY",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 1}},
        {"SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE/NON_IPV6",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 1}},
        {"SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE/ARP",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 1}},
        {"SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE/ARP_REQUEST",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 1}},
        {"SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_TYPE/ARP_REPLY",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 1}},
        {"SAI_ACL_TABLE_ATTR_FIELD_ACL_IP_FRAG",
         {.stages = {Stage::kIngress}, .format = Format::kHexString, .bitwidth = 1}},
        {"SAI_ACL_TABLE_ATTR_FIELD_IPV6_FLOW_LABEL",
         {.stages = {Stage::kIngress}, .format = Format::kHexString, .bitwidth = 20}},
        {"SAI_ACL_TABLE_ATTR_FIELD_TC",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 8}},
        {"SAI_ACL_TABLE_ATTR_FIELD_ICMP_TYPE",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 8}},
        {"SAI_ACL_TABLE_ATTR_FIELD_ICMP_CODE",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 8}},
        {"SAI_ACL_TABLE_ATTR_FIELD_ICMPV6_TYPE",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 8}},
        {"SAI_ACL_TABLE_ATTR_FIELD_ICMPV6_CODE",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 8}},
        // SAI_ACL_TABLE_ATTR_FIELD_PACKET_VLAN
        // SAI_ACL_TABLE_ATTR_FIELD_TUNNEL_VNI
        // SAI_ACL_TABLE_ATTR_FIELD_HAS_VLAN_TAG
        // SAI_ACL_TABLE_ATTR_FIELD_MACSEC_SCI
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL0_LABEL
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL0_TTL
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL0_EXP
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL0_BOS
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL1_LABEL
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL1_TTL
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL1_EXP
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL1_BOS
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL2_LABEL
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL2_TTL
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL2_EXP
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL2_BOS
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL3_LABEL
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL3_TTL
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL3_EXP
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL3_BOS
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL4_LABEL
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL4_TTL
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL4_EXP
        // SAI_ACL_TABLE_ATTR_FIELD_MPLS_LABEL4_BOS
        // SAI_ACL_TABLE_ATTR_FIELD_FDB_DST_USER_META
        {"SAI_ACL_TABLE_ATTR_FIELD_ROUTE_DST_USER_META",
         {.stages = {Stage::kIngress, Stage::kEgress}, .format = Format::kHexString, .bitwidth = 32}},
        // SAI_ACL_TABLE_ATTR_FIELD_NEIGHBOR_DST_USER_META
        // SAI_ACL_TABLE_ATTR_FIELD_PORT_USER_META
        // SAI_ACL_TABLE_ATTR_FIELD_VLAN_USER_META
        {"SAI_ACL_TABLE_ATTR_FIELD_ACL_USER_META",
         {.stages = {Stage::kIngress, Stage::kEgress}, .format = Format::kHexString, .bitwidth = 8}},
        // SAI_ACL_TABLE_ATTR_FIELD_FDB_NPU_META_DST_HIT
        // SAI_ACL_TABLE_ATTR_FIELD_NEIGHBOR_NPU_META_DST_HIT
        // SAI_ACL_TABLE_ATTR_FIELD_ROUTE_NPU_META_DST_HIT
        // SAI_ACL_TABLE_ATTR_FIELD_BTH_OPCODE
        // SAI_ACL_TABLE_ATTR_FIELD_AETH_SYNDROME
        // SAI_ACL_TABLE_ATTR_FIELD_ACL_RANGE_TYPE
        {"SAI_ACL_TABLE_ATTR_FIELD_IPV6_NEXT_HEADER",
         {.stages = {Stage::kPreIngress, Stage::kIngress, Stage::kEgress},
          .format = Format::kHexString,
          .bitwidth = 8}},
        // SAI_ACL_TABLE_ATTR_FIELD_GRE_KEY
        // SAI_ACL_TABLE_ATTR_FIELD_TAM_INT_TYPE
    });
    auto lookup = kMatchFields->find(match_field_name);
    if (lookup == kMatchFields->end())
    {
        throw std::invalid_argument("Unknown match field: " + match_field_name);
    }
    return lookup->second;
}

const ActionSchema &ActionSchemaByName(const std::string &action_name)
{
    static const auto *const kActions = new std::unordered_map<std::string, ActionSchema>({
        {"SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT", {.format = Format::kString, .bitwidth = 0}},
        {"SAI_ACL_ENTRY_ATTR_ACTION_ENDPOINT_IP", {.format = Format::kIPv4, .bitwidth = 32}},
        // SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT_LIST
        // SAI_ACL_ENTRY_ATTR_ACTION_PACKET_ACTION
        {"SAI_ACL_ENTRY_ATTR_ACTION_FLOOD", {.format = Format::kNone, .bitwidth = 0}},
        // SAI_ACL_ENTRY_ATTR_ACTION_COUNTER
        {"SAI_ACL_ENTRY_ATTR_ACTION_MIRROR_INGRESS", {.format = Format::kString, .bitwidth = 0}},
        {"SAI_ACL_ENTRY_ATTR_ACTION_MIRROR_EGRESS", {.format = Format::kString, .bitwidth = 0}},
        // SAI_ACL_ENTRY_ATTR_ACTION_SET_POLICER
        {"SAI_ACL_ENTRY_ATTR_ACTION_DECREMENT_TTL", {.format = Format::kHexString, .bitwidth = 1}},
        {"SAI_ACL_ENTRY_ATTR_ACTION_SET_TC", {.format = Format::kHexString, .bitwidth = 8}},
        // SAI_ACL_ENTRY_ATTR_ACTION_SET_PACKET_COLOR
        {"SAI_ACL_ENTRY_ATTR_ACTION_SET_INNER_VLAN_ID", {.format = Format::kHexString, .bitwidth = 12}},
        {"SAI_ACL_ENTRY_ATTR_ACTION_SET_INNER_VLAN_PRI", {.format = Format::kHexString, .bitwidth = 3}},
        {"SAI_ACL_ENTRY_ATTR_ACTION_SET_OUTER_VLAN_ID", {.format = Format::kHexString, .bitwidth = 12}},
        {"SAI_ACL_ENTRY_ATTR_ACTION_SET_OUTER_VLAN_PRI", {.format = Format::kHexString, .bitwidth = 3}},
        {"SAI_ACL_ENTRY_ATTR_ACTION_ADD_VLAN_ID", {.format = Format::kHexString, .bitwidth = 12}},
        {"SAI_ACL_ENTRY_ATTR_ACTION_ADD_VLAN_PRI", {.format = Format::kHexString, .bitwidth = 3}},
        {"SAI_ACL_ENTRY_ATTR_ACTION_SET_SRC_MAC", {.format = Format::kMac, .bitwidth = 48}},
        {"SAI_ACL_ENTRY_ATTR_ACTION_SET_DST_MAC", {.format = Format::kMac, .bitwidth = 48}},
        // SAI_ACL_ENTRY_ATTR_ACTION_SET_SRC_IP
        // SAI_ACL_ENTRY_ATTR_ACTION_SET_DST_IP
        // SAI_ACL_ENTRY_ATTR_ACTION_SET_SRC_IPV6
        // SAI_ACL_ENTRY_ATTR_ACTION_SET_DST_IPV6
        {"SAI_ACL_ENTRY_ATTR_ACTION_SET_DSCP", {.format = Format::kHexString, .bitwidth = 6}},
        {"SAI_ACL_ENTRY_ATTR_ACTION_SET_ECN", {.format = Format::kHexString, .bitwidth = 2}},
        {"SAI_ACL_ENTRY_ATTR_ACTION_SET_L4_SRC_PORT", {.format = Format::kHexString, .bitwidth = 16}},
        {"SAI_ACL_ENTRY_ATTR_ACTION_SET_L4_DST_PORT", {.format = Format::kHexString, .bitwidth = 16}},
        // SAI_ACL_ENTRY_ATTR_ACTION_INGRESS_SAMPLEPACKET_ENABLE
        // SAI_ACL_ENTRY_ATTR_ACTION_EGRESS_SAMPLEPACKET_ENABLE
        {"SAI_ACL_ENTRY_ATTR_ACTION_SET_ACL_META_DATA", {.format = Format::kHexString, .bitwidth = 8}},
        // SAI_ACL_ENTRY_ATTR_ACTION_EGRESS_BLOCK_PORT_LIST
        // SAI_ACL_ENTRY_ATTR_ACTION_SET_USER_TRAP_ID
        {"SAI_ACL_ENTRY_ATTR_ACTION_SET_DO_NOT_LEARN", {.format = Format::kNone, .bitwidth = 0}},
        // SAI_ACL_ENTRY_ATTR_ACTION_ACL_DTEL_FLOW_OP
        // SAI_ACL_ENTRY_ATTR_ACTION_DTEL_INT_SESSION
        // SAI_ACL_ENTRY_ATTR_ACTION_DTEL_DROP_REPORT_ENABLE
        // SAI_ACL_ENTRY_ATTR_ACTION_DTEL_TAIL_DROP_REPORT_ENABLE
        // SAI_ACL_ENTRY_ATTR_ACTION_DTEL_FLOW_SAMPLE_PERCENT
        // SAI_ACL_ENTRY_ATTR_ACTION_DTEL_REPORT_ALL_PACKETS
        // SAI_ACL_ENTRY_ATTR_ACTION_NO_NAT
        // SAI_ACL_ENTRY_ATTR_ACTION_INT_INSERT
        // SAI_ACL_ENTRY_ATTR_ACTION_INT_DELETE
        // SAI_ACL_ENTRY_ATTR_ACTION_INT_REPORT_FLOW
        // SAI_ACL_ENTRY_ATTR_ACTION_INT_REPORT_DROPS
        // SAI_ACL_ENTRY_ATTR_ACTION_INT_REPORT_TAIL_DROPS
        // SAI_ACL_ENTRY_ATTR_ACTION_TAM_INT_OBJECT
        // SAI_ACL_ENTRY_ATTR_ACTION_SET_ISOLATION_GROUP
        // SAI_ACL_ENTRY_ATTR_ACTION_MACSEC_FLOW
        // SAI_ACL_ENTRY_ATTR_ACTION_SET_LAG_HASH_ID
        // SAI_ACL_ENTRY_ATTR_ACTION_SET_ECMP_HASH_ID
        {"SAI_ACL_ENTRY_ATTR_ACTION_SET_VRF", {.format = Format::kString, .bitwidth = 0}},
        // SAI_ACL_ENTRY_ATTR_ACTION_SET_FORWARDING_CLASS
        {"SAI_PACKET_ACTION_DROP", {.format = Format::kNone, .bitwidth = 0}},
        {"SAI_PACKET_ACTION_FORWARD", {.format = Format::kNone, .bitwidth = 0}},
        {"SAI_PACKET_ACTION_COPY", {.format = Format::kNone, .bitwidth = 0}},
        {"SAI_PACKET_ACTION_COPY_CANCEL", {.format = Format::kNone, .bitwidth = 0}},
        {"SAI_PACKET_ACTION_TRAP", {.format = Format::kNone, .bitwidth = 0}},
        {"SAI_PACKET_ACTION_LOG", {.format = Format::kNone, .bitwidth = 0}},
        {"SAI_PACKET_ACTION_DENY", {.format = Format::kNone, .bitwidth = 0}},
        // SAI_PACKET_ACTION_TRANSIT
        {"QOS_QUEUE", {.format = Format::kString, .bitwidth = 0}},
    });

    auto lookup = kActions->find(action_name);
    if (lookup == kActions->end())
    {
        throw std::invalid_argument("Unknown action: " + action_name);
    }
    return lookup->second;
}

} // namespace acl
} // namespace swss
