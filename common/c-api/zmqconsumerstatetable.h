#ifndef SWSS_COMMON_C_API_ZMQCONSUMERSTATETABLE_H
#define SWSS_COMMON_C_API_ZMQCONSUMERSTATETABLE_H

#include "dbconnector.h"
#include "result.h"
#include "util.h"
#include "zmqserver.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct SWSSZmqConsumerStateTableOpaque *SWSSZmqConsumerStateTable;

SWSSResult SWSSZmqConsumerStateTable_new(SWSSDBConnector db, const char *tableName,
                                         SWSSZmqServer zmqs, const int32_t *popBatchSize,
                                         const int32_t *pri, SWSSZmqConsumerStateTable *outTbl);

// Outputs NULL for popBatchSize and/or pri to use the default values
SWSSResult SWSSZmqConsumerStateTable_free(SWSSZmqConsumerStateTable tbl);

// Result array and all of its members must be freed using free()
SWSSResult SWSSZmqConsumerStateTable_pops(SWSSZmqConsumerStateTable tbl,
                                          SWSSKeyOpFieldValuesArray *outArr);

// Outputs the underlying fd for polling/selecting on.
// Callers must NOT read/write on fd, it may only be used for epoll or similar.
// After the fd becomes readable, SWSSZmqConsumerStateTable_readData must be used to
// reset the fd and read data into internal data structures.
SWSSResult SWSSZmqConsumerStateTable_getFd(SWSSZmqConsumerStateTable tbl, uint32_t *outFd);

// Block until data is available to read or until a timeout elapses.
// A timeout of 0 means the call will return immediately.
SWSSResult SWSSZmqConsumerStateTable_readData(SWSSZmqConsumerStateTable tbl, uint32_t timeout_ms,
                                              uint8_t interrupt_on_signal,
                                              SWSSSelectResult *outResult);

SWSSResult
SWSSZmqConsumerStateTable_getDbConnector(SWSSZmqConsumerStateTable tbl,
                                         const struct SWSSDBConnectorOpaque **outDbConnector);

#ifdef __cplusplus
}
#endif

#endif
