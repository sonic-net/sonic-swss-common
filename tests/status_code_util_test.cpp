#include "common/status_code_util.h"

#include <gtest/gtest.h>

namespace
{

using swss::StatusCode;

TEST(StatusCodeUtilTest, StatusCodeUtilTest)
{
    for (int i = static_cast<int>(StatusCode::SWSS_RC_SUCCESS); i <= static_cast<int>(StatusCode::SWSS_RC_UNKNOWN); ++i)
    {
        StatusCode original = static_cast<StatusCode>(i);
        StatusCode final = swss::strToStatusCode(statusCodeToStr(original));
        EXPECT_EQ(original, final);
    }
    EXPECT_EQ(StatusCode::SWSS_RC_UNKNOWN, swss::strToStatusCode("invalid"));
}

} // namespace