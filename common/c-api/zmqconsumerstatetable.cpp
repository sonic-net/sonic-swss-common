#include <boost/numeric/conversion/cast.hpp>
#include "../zmqconsumerstatetable.h"
#include "../table.h"
#include "util.h"
#include "zmqconsumerstatetable.h"
#include "zmqserver.h"
#include <boost/numeric/conversion/cast.hpp>

using namespace swss;
using namespace std;
using boost::numeric_cast;

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

uint32_t SWSSZmqConsumerStateTable_getFd(SWSSZmqConsumerStateTable tbl) {
    SWSSTry(return numeric_cast<uint32_t>(((ZmqConsumerStateTable *)tbl)->getFd()));
}

SWSSSelectResult SWSSZmqConsumerStateTable_readData(SWSSZmqConsumerStateTable tbl,
                                                    uint32_t timeout_ms,
                                                    uint8_t interrupt_on_signal) {
    SWSSTry(return selectOne((ZmqConsumerStateTable *)tbl, timeout_ms, interrupt_on_signal));
}

const struct SWSSDBConnectorOpaque *
SWSSZmqConsumerStateTable_getDbConnector(SWSSZmqConsumerStateTable tbl) {
    SWSSTry(return (const SWSSDBConnectorOpaque *)((ZmqConsumerStateTable *)tbl)->getDbConnector());
}
