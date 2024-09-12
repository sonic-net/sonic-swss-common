#ifndef SWSS_COMMON_C_API_SUBSCRIBERSTATETABLE_H
#define SWSS_COMMON_C_API_SUBSCRIBERSTATETABLE_H

#include "dbconnector.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct SWSSSubscriberStateTableOpaque *SWSSSubscriberStateTable;

// Pass NULL for popBatchSize and/or pri to use the default values
SWSSSubscriberStateTable SWSSSubscriberStateTable_new(SWSSDBConnector db, const char *tableName,
                                                      const int32_t *popBatchSize,
                                                      const int32_t *pri);

void SWSSSubscriberStateTable_free(SWSSSubscriberStateTable tbl);

// Result array and all of its members must be freed using free()
SWSSKeyOpFieldValuesArray SWSSSubscriberStateTable_pops(SWSSSubscriberStateTable tbl);

// Returns 0 for false, 1 for true
uint8_t SWSSSubscriberStateTable_hasData(SWSSSubscriberStateTable tbl);

// Returns 0 for false, 1 for true
uint8_t SWSSSubscriberStateTable_hasCachedData(SWSSSubscriberStateTable tbl);

// Returns 0 for false, 1 for true
uint8_t SWSSSubscriberStateTable_initializedWithData(SWSSSubscriberStateTable tbl);

void SWSSSubscriberStateTable_readData(SWSSSubscriberStateTable tbl);

#ifdef __cplusplus
}
#endif

#endif
