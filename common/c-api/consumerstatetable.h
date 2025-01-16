#ifndef SWSS_COMMON_C_API_CONSUMERSTATETABLE_H
#define SWSS_COMMON_C_API_CONSUMERSTATETABLE_H

#include "dbconnector.h"
#include "result.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct SWSSConsumerStateTableOpaque *SWSSConsumerStateTable;

SWSSResult SWSSConsumerStateTable_new(SWSSDBConnector db, const char *tableName,
                                      const int32_t *popBatchSize, const int32_t *pri,
                                      SWSSConsumerStateTable *outTbl);

SWSSResult SWSSConsumerStateTable_free(SWSSConsumerStateTable tbl);

// Result array and all of its members must be freed using free()
SWSSResult SWSSConsumerStateTable_pops(SWSSConsumerStateTable tbl, SWSSKeyOpFieldValuesArray *outArr);

// Outputs the underlying fd for polling/selecting on.
// Callers must NOT read/write on the fd, it may only be used for epoll or similar.
// After the fd becomes readable, SWSSConsumerStateTable_readData must be used to
// reset the fd and read data into internal data structures.
SWSSResult SWSSConsumerStateTable_getFd(SWSSConsumerStateTable tbl, uint32_t *outFd);

// Block until data is available to read or until a timeout elapses.
// A timeout of 0 means the call will return immediately.
SWSSResult SWSSConsumerStateTable_readData(SWSSConsumerStateTable tbl, uint32_t timeout_ms,
                                           uint8_t interrupt_on_signal, SWSSSelectResult *outResult);

#ifdef __cplusplus
}
#endif

#endif
