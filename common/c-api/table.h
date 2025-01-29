#ifndef SWSS_COMMON_C_API_TABLE_H
#define SWSS_COMMON_C_API_TABLE_H

#include "dbconnector.h"
#include "result.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct SWSSTableOpaque *SWSSTable;

SWSSResult SWSSTable_new(SWSSDBConnector db, const char *tableName, SWSSTable *outTbl);

SWSSResult SWSSTable_free(SWSSTable tbl);

// If the key exists, populates outValues with the table's values and outputs 1.
// If the key doesn't exist, outputs 0 and does not touch outValues.
SWSSResult SWSSTable_get(SWSSTable tbl, const char *key, SWSSFieldValueArray *outValues,
                         int8_t *outExists);

// If the key and field exist, populates outValue with the field's value and outputs 1.
// If the key doesn't exist, outputs 0 and does not touch outValue.
SWSSResult SWSSTable_hget(SWSSTable tbl, const char *key, const char *field, SWSSString *outValue,
                          int8_t *outExists);

SWSSResult SWSSTable_set(SWSSTable tbl, const char *key, SWSSFieldValueArray values);

SWSSResult SWSSTable_hset(SWSSTable tbl, const char *key, const char *field, SWSSStrRef value);

SWSSResult SWSSTable_del(SWSSTable tbl, const char *key);

SWSSResult SWSSTable_hdel(SWSSTable tbl, const char *key, const char *field);

SWSSResult SWSSTable_getKeys(SWSSTable tbl, SWSSStringArray *outKeys);

#ifdef __cplusplus
}
#endif

#endif
