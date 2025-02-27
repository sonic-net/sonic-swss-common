#ifndef SWSS_COMMON_C_API_ZMQCLIENT_H
#define SWSS_COMMON_C_API_ZMQCLIENT_H

#include "result.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct SWSSZmqClientOpaque *SWSSZmqClient;

SWSSResult SWSSZmqClient_new(const char *endpoint, SWSSZmqClient *outZmqc);

SWSSResult SWSSZmqClient_free(SWSSZmqClient zmqc);

// Outputs 0 for false, 1 for true
SWSSResult SWSSZmqClient_isConnected(SWSSZmqClient zmqc, int8_t *outIsConnected);

SWSSResult SWSSZmqClient_connect(SWSSZmqClient zmqc);

SWSSResult SWSSZmqClient_sendMsg(SWSSZmqClient zmqc, const char *dbName, const char *tableName,
                                 SWSSKeyOpFieldValuesArray kcos);

#ifdef __cplusplus
}
#endif

#endif
