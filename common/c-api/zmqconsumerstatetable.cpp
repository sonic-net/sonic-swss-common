#include "../zmqconsumerstatetable.h"
#include "../table.h"
#include "util.h"
#include "zmqconsumerstatetable.h"
#include "zmqserver.h"

using namespace swss;
using namespace std;

// Pass NULL for popBatchSize and/or pri to use the default values
SWSSZmqConsumerStateTable SWSSZmqConsumerStateTable_new(SWSSDBConnector db, const char *tableName,
                                                        SWSSZmqServer zmqs,
                                                        const int32_t *p_popBatchSize,
                                                        const int32_t *p_pri) {

    int popBatchSize = p_popBatchSize ? numeric_cast<int>(*p_popBatchSize)
                                      : TableConsumable::DEFAULT_POP_BATCH_SIZE;
    int pri = p_pri ? numeric_cast<int>(*p_pri) : 0;
    SWSSTry(return (SWSSZmqConsumerStateTable) new ZmqConsumerStateTable(
        (DBConnector *)db, string(tableName), *(ZmqServer *)zmqs, popBatchSize, pri));
}

void SWSSZmqConsumerStateTable_free(SWSSZmqConsumerStateTable tbl) {
    SWSSTry(delete (ZmqConsumerStateTable *)tbl);
}

SWSSKeyOpFieldValuesArray SWSSZmqConsumerStateTable_pops(SWSSZmqConsumerStateTable tbl) {
    SWSSTry({
        deque<KeyOpFieldsValuesTuple> vkco;
        ((ZmqConsumerStateTable *)tbl)->pops(vkco);
        return makeKeyOpFieldValuesArray(vkco);
    });
}

int32_t SWSSZmqConsumerStateTable_getFd(SWSSZmqConsumerStateTable tbl) {
    SWSSTry(return numeric_cast<int32_t>(((ZmqConsumerStateTable *)tbl)->getFd()));
}

uint64_t SWSSZmqConsumerStateTable_readData(SWSSZmqConsumerStateTable tbl) {
    SWSSTry(return numeric_cast<uint64_t>(((ZmqConsumerStateTable *)tbl)->readData()));
}

// Returns 0 for false, 1 for true
uint8_t SWSSZmqConsumerStateTable_hasData(SWSSZmqConsumerStateTable tbl) {
    SWSSTry(return ((ZmqConsumerStateTable *)tbl)->hasData() ? 1 : 0);
}

// Returns 0 for false, 1 for true
uint8_t SWSSZmqConsumerStateTable_hasCachedData(SWSSZmqConsumerStateTable tbl) {
    SWSSTry(return ((ZmqConsumerStateTable *)tbl)->hasCachedData() ? 1 : 0);
}

// Returns 0 for false, 1 for true
uint8_t SWSSZmqConsumerStateTable_initializedWithData(SWSSZmqConsumerStateTable tbl) {
    SWSSTry(return ((ZmqConsumerStateTable *)tbl)->hasData() ? 1 : 0);
}

const struct SWSSDBConnectorOpaque *SWSSZmqConsumerStateTable_getDbConnector(SWSSZmqConsumerStateTable tbl) {
    SWSSTry(return (const SWSSDBConnectorOpaque *)((ZmqConsumerStateTable *)tbl)->getDbConnector());
}

uint64_t SWSSZmqConsumerStateTable_dbUpdaterQueueSize(SWSSZmqConsumerStateTable tbl) {
    SWSSTry(return numeric_cast<uint64_t>(((ZmqConsumerStateTable *)tbl)->dbUpdaterQueueSize()));
}
