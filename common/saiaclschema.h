#ifndef SONIC_SWSS_COMMON_COMMON_SAIACLSCHEMA_H_
#define SONIC_SWSS_COMMON_COMMON_SAIACLSCHEMA_H_

#include <set>
#include <string>
#include <unordered_map>

// This file describes the AppDB ACL schema values between the P4RT app and
// OrchAgent and the capabilities of OrchAgent.

namespace swss
{
namespace acl
{

// Enumeration of ACL stages.
enum class Stage
{
    kPreIngress,
    kIngress,
    kEgress
};

// Enumeration of expected data formats for match fields and action parameters.
enum class Format
{
    kNone,      // No value is expected (e.g. DROP action).
    kHexString, // Numerical value represented as hexadecimal. "0x1234"
    kMac,       // MAC address. "11:22:33:44:55:66"
    kIPv4,      // IPv4 address in canonical format. "128.0.0.1"
    kIPv6,      // IPv6 address in canonical format. "2002:1:128::"
    kString     // String value. "Port0"
};

// Schema information for deciphering match fields.
struct MatchFieldSchema
{
    std::set<Stage> stages; // Stages where this match field can be used.
    Format format;          // Expected format for this match field in AppDB.
    int bitwidth;           // Bitwidth of the match field (if not string).
};

// Schema information for deciphering actions and action parameters.
struct ActionSchema
{
    Format format; // Expected format for the action parameter in AppDB.
    int bitwidth;  // Bitwidth of the action parameter (if not string & not none).
};

// Returns the SAI stage enum matching the AppDB name.
// "PRE_INGRESS"  --> SAI_ACL_STAGE_PRE_INGRESS
// "INGRESS"      --> SAI_ACL_STAGE_INGRESS
// "EGRESS"       --> SAI_ACL_STAGE_EGRESS
//
// Throws std::invalid_argument if the name does not match a stage.
Stage StageFromName(const std::string &name);

// Returns the AppDB name for a SAI stage enum. See StageFromName.
const std::string &StageName(Stage stage);

// Returns the Format enum matching the AppDB format name.
// "HEX_STRING" --> kHexString
// "MAC"        --> kMac
// "IP"         --> kIP
// "IPV6"       --> kIPv6
// "IPV4"       --> kIPv4
// "STRING"     --> kString
//
// Throws std::invalid_argument if the name does not match a Format.
Format FormatFromName(const std::string &name);

// Returns the AppDB format name for a Format enum.
const std::string &FormatName(Format format);

// Returns the schema for a match field.
//
// Throws std::invalid_argument for unknown match fields and match fields
// without schemas.
const MatchFieldSchema &MatchFieldSchemaByName(const std::string &match_field_name);

// Returns the schema for an action.
//
// Throws std::invalid_argument for unknown actions and actions without schemas.
const ActionSchema &ActionSchemaByName(const std::string &action_name);

} // namespace acl
} // namespace swss

#endif // SONIC_SWSS_COMMON_COMMON_SAIACLSCHEMA_H_
