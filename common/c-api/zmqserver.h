#ifndef SWSS_COMMON_C_API_ZMQSERVER_H
#define SWSS_COMMON_C_API_ZMQSERVER_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SWSSZmqServerOpaque *SWSSZmqServer;

SWSSZmqServer SWSSZmqServer_new(const char *endpoint);

void SWSSZmqServer_free(SWSSZmqServer zmqs);

#ifdef __cplusplus
}
#endif

#endif
