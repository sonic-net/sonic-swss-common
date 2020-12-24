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

static inline boost::optional<std::string> fvtGetValue(
    const std::vector<FieldValueTuple> &fvt,
    const std::string &field)
{
    SWSS_LOG_ENTER();

    for (auto itr = fvt.begin(); itr != fvt.end(); itr++)
    {
        if (boost::iequals(fvField(*itr), field))
        {
            return boost::optional<std::string>(fvValue(*itr));
        }
    }

    SWSS_LOG_DEBUG("Cannot find field : %s", field.c_str());

    return boost::optional<std::string>();
}
}
