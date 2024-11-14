#ifndef SWSS_COMMON_C_API_PRODUCERSTATETABLE_H
#define SWSS_COMMON_C_API_PRODUCERSTATETABLE_H

#include "dbconnector.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct SWSSProducerStateTableOpaque *SWSSProducerStateTable;

SWSSProducerStateTable SWSSProducerStateTable_new(SWSSDBConnector db, const char *tableName);

void SWSSProducerStateTable_free(SWSSProducerStateTable tbl);

void SWSSProducerStateTable_setBuffered(SWSSProducerStateTable tbl, uint8_t buffered);

void SWSSProducerStateTable_set(SWSSProducerStateTable tbl, const char *key, SWSSFieldValueArray values);

void SWSSProducerStateTable_del(SWSSProducerStateTable tbl, const char *key);

void SWSSProducerStateTable_flush(SWSSProducerStateTable tbl);

int64_t SWSSProducerStateTable_count(SWSSProducerStateTable tbl);

void SWSSProducerStateTable_clear(SWSSProducerStateTable tbl);

void SWSSProducerStateTable_create_temp_view(SWSSProducerStateTable tbl);

void SWSSProducerStateTable_apply_temp_view(SWSSProducerStateTable tbl);

#ifdef __cplusplus
}
#endif

#endif
