#ifndef SWSS_COMMON_C_API_CONSUMERSTATETABLE_H
#define SWSS_COMMON_C_API_CONSUMERSTATETABLE_H

#include "dbconnector.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct SWSSConsumerStateTableOpaque *SWSSConsumerStateTable;

// Pass NULL for popBatchSize and/or pri to use the default values
SWSSConsumerStateTable SWSSConsumerStateTable_new(SWSSDBConnector db, const char *tableName,
                                                  const int32_t *popBatchSize, const int32_t *pri);

void SWSSConsumerStateTable_free(SWSSConsumerStateTable tbl);

// Result array and all of its members must be freed using free()
SWSSKeyOpFieldValuesArray SWSSConsumerStateTable_pops(SWSSConsumerStateTable tbl);

// Return the underlying fd for polling/selecting on.
// Callers must NOT read/write on fd, it may only be used for epoll or similar.
// After the fd becomes readable, SWSSConsumerStateTable_readData must be used to
// reset the fd and read data into internal data structures.
uint32_t SWSSConsumerStateTable_getFd(SWSSConsumerStateTable tbl);

// Block until data is available to read or until a timeout elapses.
// A timeout of 0 means the call will return immediately.
SWSSSelectResult SWSSConsumerStateTable_readData(SWSSConsumerStateTable tbl, uint32_t timeout_ms,
                                                 uint8_t interrupt_on_signal);

#ifdef __cplusplus
}
#endif

#endif
