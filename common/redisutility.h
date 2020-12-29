#pragma once

#include "rediscommand.h"

#include <boost/optional.hpp>

#include <vector>
#include <algorithm>

namespace swss
{
boost::optional<std::string> fvsGetValue(
    const std::vector<FieldValueTuple> &fvt,
    const std::string &field,
    bool case_insensitive = false);
}
