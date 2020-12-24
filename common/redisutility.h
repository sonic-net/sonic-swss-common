#pragma once

#include "rediscommand.h"
#include "logger.h"
#include "stringutility.h"

#include <boost/algorithm/string.hpp>
#include <boost/optional.hpp>

#include <vector>
#include <algorithm>

namespace swss
{

enum FieldNameStrategy
{
    CASE_SENSITIVE,
    CASE_INSENSITIVE,
};

static inline boost::optional<std::string> fvtGetValue(
    const std::vector<FieldValueTuple> &fvt,
    const std::string &field,
    FieldNameStrategy strategy = FieldNameStrategy::CASE_SENSITIVE)
{
    SWSS_LOG_ENTER();

    for (auto itr = fvt.begin(); itr != fvt.end(); itr++)
    {
        bool is_equal = false;
        switch(strategy)
        {
            case FieldNameStrategy::CASE_SENSITIVE:
                is_equal = boost::equals(fvField(*itr), field);
                break;
            case FieldNameStrategy::CASE_INSENSITIVE:
                is_equal = boost::iequals(fvField(*itr), field);
                break;
            default:
                is_equal = boost::equals(fvField(*itr), field);
        }
        if (is_equal)
        {
            return boost::optional<std::string>(fvValue(*itr));
        }
    }

    SWSS_LOG_DEBUG("Cannot find field : %s", field.c_str());

    return boost::optional<std::string>();
}
}
