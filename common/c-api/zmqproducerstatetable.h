#ifndef SWSS_COMMON_C_API_ZMQPRODUCERSTATETABLE_H
#define SWSS_COMMON_C_API_ZMQPRODUCERSTATETABLE_H

#include "dbconnector.h"
#include "util.h"
#include "zmqclient.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

typedef struct SWSSZmqProducerStateTableOpaque *SWSSZmqProducerStateTable;

SWSSZmqProducerStateTable SWSSZmqProducerStateTable_new(SWSSDBConnector db, const char *tableName,
                                                        SWSSZmqClient zmqc, uint8_t dbPersistence);

void SWSSZmqProducerStateTable_free(SWSSZmqProducerStateTable tbl);

void SWSSZmqProducerStateTable_set(SWSSZmqProducerStateTable tbl, const char *key,
                                   SWSSFieldValueArray values);

void SWSSZmqProducerStateTable_del(SWSSZmqProducerStateTable tbl, const char *key);

uint64_t SWSSZmqProducerStateTable_dbUpdaterQueueSize(SWSSZmqProducerStateTable tbl);

#ifdef __cplusplus
}
#endif

#endif
