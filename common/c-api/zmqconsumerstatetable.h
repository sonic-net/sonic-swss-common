#ifndef SWSS_COMMON_C_API_ZMQCONSUMERSTATETABLE_H
#define SWSS_COMMON_C_API_ZMQCONSUMERSTATETABLE_H

#include "dbconnector.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct SWSSZmqConsumerStateTableOpaque *SWSSZmqConsumerStateTable;

// Pass NULL for popBatchSize and/or pri to use the default values
SWSSZmqConsumerStateTable SWSSZmqConsumerStateTable_new(SWSSDBConnector db, const char *tableName,
                                                     const int32_t *popBatchSize,
                                                     const int32_t *pri);

void SWSSZmqConsumerStateTable_free(SWSSZmqConsumerStateTable tbl);

// Result array and all of its members must be freed using free()
// SWSSKeyOpFieldValuesArray SWSSConsumerStateTable_pops(SWSSConsumerStateTable tbl);

#ifdef __cplusplus
}
#endif

#endif
