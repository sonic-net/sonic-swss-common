#ifndef SWSS_COMMON_C_API_LOGGER_H
#define SWSS_COMMON_C_API_LOGGER_H

#include "result.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef void (*SWSSOutputChangeNotify) (const char* component, const char* outputStr);
typedef void (*SWSSPriorityChangeNotify) (const char* component, const char* outputStr);
SWSSResult SWSSLogger_linkToDbWithOutput(const char *dbName, SWSSPriorityChangeNotify prioChangeNotify, const char *defLogLevel, SWSSOutputChangeNotify outputChangeNotify, const char *defOutput);
SWSSResult SWSSLogger_restartLogger();

#ifdef __cplusplus
}
#endif

#endif
