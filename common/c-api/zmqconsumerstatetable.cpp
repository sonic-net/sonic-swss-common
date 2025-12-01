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
SWSSResult SWSSZmqConsumerStateTable_new(SWSSDBConnector db, const char *tableName,
                                         SWSSZmqServer zmqs, const int32_t *p_popBatchSize,
                                         const int32_t *p_pri, SWSSZmqConsumerStateTable *outTbl) {
    SWSSTry({
        int popBatchSize = p_popBatchSize ? numeric_cast<int>(*p_popBatchSize)
                                          : TableConsumable::DEFAULT_POP_BATCH_SIZE;
        int pri = p_pri ? numeric_cast<int>(*p_pri) : 0;
        *outTbl = (SWSSZmqConsumerStateTable) new ZmqConsumerStateTable(
            (DBConnector *)db, string(tableName), *(ZmqServer *)zmqs, popBatchSize, pri);
    });
}

SWSSResult SWSSZmqConsumerStateTable_free(SWSSZmqConsumerStateTable tbl) {
    SWSSTry(delete (ZmqConsumerStateTable *)tbl);
}

SWSSResult SWSSZmqConsumerStateTable_pops(SWSSZmqConsumerStateTable tbl,
                                          SWSSKeyOpFieldValuesArray *outArr) {
    SWSSTry({
        deque<KeyOpFieldsValuesTuple> vkco;
        ((ZmqConsumerStateTable *)tbl)->pops(vkco);
        *outArr = makeKeyOpFieldValuesArray(vkco);
    });
}

SWSSResult SWSSZmqConsumerStateTable_getFd(SWSSZmqConsumerStateTable tbl, uint32_t *outFd) {
    SWSSTry(*outFd = numeric_cast<uint32_t>(((ZmqConsumerStateTable *)tbl)->getFd()));
}

SWSSResult SWSSZmqConsumerStateTable_readData(SWSSZmqConsumerStateTable tbl, uint32_t timeout_ms,
                                              uint8_t interrupt_on_signal,
                                              SWSSSelectResult *outResult) {
    SWSSTry(*outResult = selectOne((ZmqConsumerStateTable *)tbl, timeout_ms, interrupt_on_signal));
}

SWSSResult SWSSZmqConsumerStateTable_getDbConnector(SWSSZmqConsumerStateTable tbl,
                                                    const struct SWSSDBConnectorOpaque **outDb) {
    SWSSTry(*outDb =
                (const SWSSDBConnectorOpaque *)((ZmqConsumerStateTable *)tbl)->getDbConnector());
}
