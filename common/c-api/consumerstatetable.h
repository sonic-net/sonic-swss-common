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

#ifdef __cplusplus
}
#endif

#endif
