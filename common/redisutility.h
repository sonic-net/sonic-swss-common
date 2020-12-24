#pragma once

#include "rediscommand.h"
#include "logger.h"
#include "stringutility.h"

#include <vector>
#include <algorithm>

namespace swss
{

template <class T>
static bool fvtGetValue(
    const std::vector<FieldValueTuple> &fvt,
    const std::string &field,
    T &value)
{
    SWSS_LOG_ENTER();

    std::string target_field = field;
    std::transform(
        target_field.begin(),
        target_field.end(),
        target_field.begin(),
        ::tolower);
    auto itr = std::find_if(
        fvt.begin(),
        fvt.end(),
        [&](const std::vector<FieldValueTuple>::value_type &entry) {
            std::string available_field = fvField(entry);
            std::transform(
                available_field.begin(),
                available_field.end(),
                available_field.begin(),
                ::tolower);
            return available_field == target_field;
        });
    if (itr != fvt.end())
    {
        value = swss::lexical_cast<T>(fvValue(*itr));
        SWSS_LOG_DEBUG(
            "Set field '%s' as '%s'",
            field.c_str(),
            fvValue(*itr).c_str());
        return true;
    }
    SWSS_LOG_DEBUG("Cannot find field : %s", field.c_str());
    return false;
}
}
