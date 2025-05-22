#ifndef SWSS_COMMON_C_API_ZMQPRODUCERSTATETABLE_H
#define SWSS_COMMON_C_API_ZMQPRODUCERSTATETABLE_H

#include "dbconnector.h"
#include "result.h"
#include "util.h"
#include "zmqclient.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

typedef struct SWSSZmqProducerStateTableOpaque *SWSSZmqProducerStateTable;

SWSSResult SWSSZmqProducerStateTable_new(SWSSDBConnector db, const char *tableName,
                                         SWSSZmqClient zmqc, uint8_t dbPersistence,
                                         SWSSZmqProducerStateTable *outTbl);

SWSSResult SWSSZmqProducerStateTable_free(SWSSZmqProducerStateTable tbl);

SWSSResult SWSSZmqProducerStateTable_set(SWSSZmqProducerStateTable tbl, const char *key,
                                         SWSSFieldValueArray values);

SWSSResult SWSSZmqProducerStateTable_del(SWSSZmqProducerStateTable tbl, const char *key);

SWSSResult SWSSZmqProducerStateTable_dbUpdaterQueueSize(SWSSZmqProducerStateTable tbl, uint64_t *outSize);

#ifdef __cplusplus
}
#endif

#endif
