#include <cstdint>
#include <unistd.h>
#include <sys/time.h>
#include <ctime>
#include <inttypes.h>
#include "timestamp.h"

using namespace std;

namespace swss {

string getTimestamp()
{
    char buffer[64];
    struct timeval tv;
    gettimeofday(&tv, NULL);

    size_t size = strftime(buffer, 32 ,"%Y-%m-%d.%T.", localtime(&tv.tv_sec));
    snprintf(&buffer[size], 32, "%06" PRIu64, (uint64_t) tv.tv_usec);

    return string(buffer);
}

}
