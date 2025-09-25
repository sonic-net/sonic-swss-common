#include <string>

#include "common/timestamp.h"
#include "gtest/gtest.h"
#include <unistd.h>

using namespace std;
using namespace swss;

TEST(TIMESTAMP, getTimestamp)
{
    // The actual implementation uses the localtime() function, so
    // there's no easy, clean way to override that function (messier 
    // ways are available). For simplicity of testing, just make sure
    // that the function returns a non-zero string, and that calling it
    // at two different times results in different timestamps being
    // returned.
    
    string first_timestamp = getTimestamp();
    ASSERT_GE(first_timestamp.length(), 0);

    usleep(500000);

    string second_timestamp = getTimestamp();
    ASSERT_GE(second_timestamp.length(), 0);

    ASSERT_NE(first_timestamp, second_timestamp);
}
