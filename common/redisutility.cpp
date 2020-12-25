#include "redisutility.h"
#include "stringutility.h"

#include <boost/algorithm/string.hpp>


boost::optional<std::string> swss::fvsGetValue(
    const std::vector<FieldValueTuple> &fvt,
    const std::string &field,
    bool case_insensitive)
{
    boost::optional<std::string> ret;

    for (auto itr = fvt.begin(); itr != fvt.end(); itr++)
    {
        bool is_equal = false;
        if (case_insensitive)
        {
            is_equal = boost::iequals(fvField(*itr), field);
        }
        else
        {
            is_equal = (fvField(*itr) == field);
        }
        if (is_equal)
        {
            ret = fvValue(*itr);
            break;
        }
    }

    return ret;
}