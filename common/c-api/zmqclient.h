#ifndef SWSS_COMMON_C_API_ZMQCLIENT_H
#define SWSS_COMMON_C_API_ZMQCLIENT_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct SWSSZmqClientOpaque *SWSSZmqClient;

SWSSZmqClient SWSSZmqClient_new(const char *endpoint);

void SWSSZmqClient_free(SWSSZmqClient zmqc);

// Returns 0 for false, 1 for true
int8_t SWSSZmqClient_isConnected(SWSSZmqClient zmqc);

void SWSSZmqClient_connect(SWSSZmqClient zmqc);

void SWSSZmqClient_sendMsg(SWSSZmqClient zmqc, const char *dbName, const char *tableName,
                           SWSSKeyOpFieldValuesArray kcos);

#ifdef __cplusplus
}
#endif

#endif
