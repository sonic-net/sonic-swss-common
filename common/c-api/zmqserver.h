#ifndef SWSS_COMMON_C_API_ZMQSERVER_H
#define SWSS_COMMON_C_API_ZMQSERVER_H

#include "result.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SWSSZmqServerOpaque *SWSSZmqServer;

SWSSResult SWSSZmqServer_new(const char *endpoint, SWSSZmqServer *outZmqServer);

SWSSResult SWSSZmqServer_free(SWSSZmqServer zmqs);

#ifdef __cplusplus
}
#endif

#endif
