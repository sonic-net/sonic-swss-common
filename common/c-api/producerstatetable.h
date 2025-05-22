#ifndef SWSS_COMMON_C_API_PRODUCERSTATETABLE_H
#define SWSS_COMMON_C_API_PRODUCERSTATETABLE_H

#include "dbconnector.h"
#include "result.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct SWSSProducerStateTableOpaque *SWSSProducerStateTable;
SWSSResult SWSSProducerStateTable_new(SWSSDBConnector db, const char *tableName,
                                      SWSSProducerStateTable *outTbl);

SWSSResult SWSSProducerStateTable_free(SWSSProducerStateTable tbl);

SWSSResult SWSSProducerStateTable_setBuffered(SWSSProducerStateTable tbl, uint8_t buffered);

SWSSResult SWSSProducerStateTable_set(SWSSProducerStateTable tbl, const char *key,
                                      SWSSFieldValueArray values);

SWSSResult SWSSProducerStateTable_del(SWSSProducerStateTable tbl, const char *key);

SWSSResult SWSSProducerStateTable_flush(SWSSProducerStateTable tbl);

SWSSResult SWSSProducerStateTable_count(SWSSProducerStateTable tbl, int64_t *outCount);

SWSSResult SWSSProducerStateTable_clear(SWSSProducerStateTable tbl);

SWSSResult SWSSProducerStateTable_create_temp_view(SWSSProducerStateTable tbl);

SWSSResult SWSSProducerStateTable_apply_temp_view(SWSSProducerStateTable tbl);

#ifdef __cplusplus
}
#endif

#endif
