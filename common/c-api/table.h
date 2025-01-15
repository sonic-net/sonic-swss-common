#ifndef SWSS_COMMON_C_API_TABLE_H
#define SWSS_COMMON_C_API_TABLE_H

#include "dbconnector.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct SWSSTableOpaque *SWSSTable;

SWSSTable SWSSTable_new(SWSSDBConnector db, const char *tableName);

void SWSSTable_free(SWSSTable tbl);

// If the key exists, populates outValues with the table's values and returns 1. 
// If the key doesn't exist, returns 0.
int8_t SWSSTable_get(SWSSTable tbl, const char *key, SWSSFieldValueArray *outValues);

// If the key and field exist, populates outValue with the field's value and returns 1. 
// If the key doesn't exist, returns 0.
int8_t SWSSTable_hget(SWSSTable tbl, const char *key, const char *field, SWSSString *outValue);

void SWSSTable_set(SWSSTable tbl, const char *key, SWSSFieldValueArray values);

void SWSSTable_hset(SWSSTable tbl, const char *key, const char *field, SWSSStrRef value);

void SWSSTable_del(SWSSTable tbl, const char *key);

void SWSSTable_hdel(SWSSTable tbl, const char *key, const char *field);

SWSSStringArray SWSSTable_getKeys(SWSSTable tbl);

#ifdef __cplusplus
}
#endif

#endif
