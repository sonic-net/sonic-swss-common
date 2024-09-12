#ifndef SWSS_COMMON_C_API_ZMQSERVER_H
#define SWSS_COMMON_C_API_ZMQSERVER_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*SWSSZmqMessageHandlerCallback)(void *data, const SWSSKeyOpFieldValuesArray *kcos);

typedef struct SWSSZmqMessageHandlerOpaque *SWSSZmqMessageHandler;

SWSSZmqMessageHandler SWSSZmqMessageHandler_new(void *data, SWSSZmqMessageHandlerCallback callback);

void SWSSZmqMessageHandler_free(SWSSZmqMessageHandler handler);

typedef struct SWSSZmqServerOpaque *SWSSZmqServer;

SWSSZmqServer SWSSZmqServer_new(const char *endpoint);

void SWSSZmqServer_free(SWSSZmqServer zmqs);

void SWSSZmqServer_registerMessageHandler(SWSSZmqServer zmqs, const char *dbName,
                                          const char *tableName, SWSSZmqMessageHandler handler);

#ifdef __cplusplus
}
#endif

#endif
