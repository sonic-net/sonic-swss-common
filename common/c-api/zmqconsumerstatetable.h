#ifndef SWSS_COMMON_C_API_ZMQCONSUMERSTATETABLE_H
#define SWSS_COMMON_C_API_ZMQCONSUMERSTATETABLE_H

#include "dbconnector.h"
#include "util.h"
#include "zmqserver.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct SWSSZmqConsumerStateTableOpaque *SWSSZmqConsumerStateTable;

// Pass NULL for popBatchSize and/or pri to use the default values
SWSSZmqConsumerStateTable SWSSZmqConsumerStateTable_new(SWSSDBConnector db, const char *tableName,
                                                        SWSSZmqServer zmqs,
                                                        const int32_t *popBatchSize,
                                                        const int32_t *pri);

void SWSSZmqConsumerStateTable_free(SWSSZmqConsumerStateTable tbl);

// Result array and all of its members must be freed using free()
SWSSKeyOpFieldValuesArray SWSSZmqConsumerStateTable_pops(SWSSZmqConsumerStateTable tbl);

// Block until data is available to read or until a timeout elapses.
// A timeout of 0 means the call will return immediately.
SWSSSelectResult SWSSZmqConsumerStateTable_readData(SWSSZmqConsumerStateTable tbl,
                                                    uint32_t timeout_ms);

// Returns 0 for false, 1 for true
uint8_t SWSSZmqConsumerStateTable_hasData(SWSSZmqConsumerStateTable tbl);

// Returns 0 for false, 1 for true
uint8_t SWSSZmqConsumerStateTable_hasCachedData(SWSSZmqConsumerStateTable tbl);

// Returns 0 for false, 1 for true
uint8_t SWSSZmqConsumerStateTable_initializedWithData(SWSSZmqConsumerStateTable tbl);

const struct SWSSDBConnectorOpaque *
SWSSZmqConsumerStateTable_getDbConnector(SWSSZmqConsumerStateTable tbl);

#ifdef __cplusplus
}
#endif

#endif
