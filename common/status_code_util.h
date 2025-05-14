#pragma once

#include <map>
#include <string>

namespace swss
{

enum class StatusCode
{
    SWSS_RC_SUCCESS,
    SWSS_RC_INVALID_PARAM,
    SWSS_RC_DEADLINE_EXCEEDED,
    SWSS_RC_UNAVAIL,
    SWSS_RC_NOT_FOUND,
    SWSS_RC_NO_MEMORY,
    SWSS_RC_EXISTS,
    SWSS_RC_PERMISSION_DENIED,
    SWSS_RC_FULL,
    SWSS_RC_IN_USE,
    SWSS_RC_INTERNAL,
    SWSS_RC_UNIMPLEMENTED,
    SWSS_RC_NOT_EXECUTED,
    SWSS_RC_FAILED_PRECONDITION,
    SWSS_RC_UNKNOWN,
};

static const std::map<StatusCode, std::string> statusCodeMapping = {
    {StatusCode::SWSS_RC_SUCCESS, "SWSS_RC_SUCCESS"},
    {StatusCode::SWSS_RC_INVALID_PARAM, "SWSS_RC_INVALID_PARAM"},
    {StatusCode::SWSS_RC_DEADLINE_EXCEEDED, "SWSS_RC_DEADLINE_EXCEEDED"},
    {StatusCode::SWSS_RC_UNAVAIL, "SWSS_RC_UNAVAIL"},
    {StatusCode::SWSS_RC_NOT_FOUND, "SWSS_RC_NOT_FOUND"},
    {StatusCode::SWSS_RC_NO_MEMORY, "SWSS_RC_NO_MEMORY"},
    {StatusCode::SWSS_RC_EXISTS, "SWSS_RC_EXISTS"},
    {StatusCode::SWSS_RC_PERMISSION_DENIED, "SWSS_RC_PERMISSION_DENIED"},
    {StatusCode::SWSS_RC_FULL, "SWSS_RC_FULL"},
    {StatusCode::SWSS_RC_IN_USE, "SWSS_RC_IN_USE"},
    {StatusCode::SWSS_RC_INTERNAL, "SWSS_RC_INTERNAL"},
    {StatusCode::SWSS_RC_UNIMPLEMENTED, "SWSS_RC_UNIMPLEMENTED"},
    {StatusCode::SWSS_RC_NOT_EXECUTED, "SWSS_RC_NOT_EXECUTED"},
    {StatusCode::SWSS_RC_FAILED_PRECONDITION, "SWSS_RC_FAILED_PRECONDITION"},
    {StatusCode::SWSS_RC_UNKNOWN, "SWSS_RC_UNKNOWN"},
};

static const std::map<std::string, StatusCode> StatusCodeLookup = {
    {"SWSS_RC_SUCCESS", StatusCode::SWSS_RC_SUCCESS},
    {"SWSS_RC_INVALID_PARAM", StatusCode::SWSS_RC_INVALID_PARAM},
    {"SWSS_RC_DEADLINE_EXCEEDED", StatusCode::SWSS_RC_DEADLINE_EXCEEDED},
    {"SWSS_RC_UNAVAIL", StatusCode::SWSS_RC_UNAVAIL},
    {"SWSS_RC_NOT_FOUND", StatusCode::SWSS_RC_NOT_FOUND},
    {"SWSS_RC_NO_MEMORY", StatusCode::SWSS_RC_NO_MEMORY},
    {"SWSS_RC_EXISTS", StatusCode::SWSS_RC_EXISTS},
    {"SWSS_RC_PERMISSION_DENIED", StatusCode::SWSS_RC_PERMISSION_DENIED},
    {"SWSS_RC_FULL", StatusCode::SWSS_RC_FULL},
    {"SWSS_RC_IN_USE", StatusCode::SWSS_RC_IN_USE},
    {"SWSS_RC_INTERNAL", StatusCode::SWSS_RC_INTERNAL},
    {"SWSS_RC_UNIMPLEMENTED", StatusCode::SWSS_RC_UNIMPLEMENTED},
    {"SWSS_RC_NOT_EXECUTED", StatusCode::SWSS_RC_NOT_EXECUTED},
    {"SWSS_RC_FAILED_PRECONDITION", StatusCode::SWSS_RC_FAILED_PRECONDITION},
    {"SWSS_RC_UNKNOWN", StatusCode::SWSS_RC_UNKNOWN},
};

static inline std::string statusCodeToStr(const StatusCode &status)
{
    if (statusCodeMapping.find(status) == statusCodeMapping.end())
    {
        return "SWSS_RC_UNKNOWN";
    }
    return statusCodeMapping.at(status);
}

static inline StatusCode strToStatusCode(const std::string &status)
{
    if (StatusCodeLookup.find(status) == StatusCodeLookup.end())
    {
        return StatusCode::SWSS_RC_UNKNOWN;
    }
    return StatusCodeLookup.at(status);
}

} // namespace swss
