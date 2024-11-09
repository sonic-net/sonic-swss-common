#include <cstdlib>
#include <cstring>
#include <deque>

#include "../consumerstatetable.h"
#include "../dbconnector.h"
#include "../table.h"
#include "consumerstatetable.h"
#include "util.h"

using namespace swss;
using namespace std;

SWSSConsumerStateTable SWSSConsumerStateTable_new(SWSSDBConnector db, const char *tableName,
                                                  const int32_t *p_popBatchSize,
                                                  const int32_t *p_pri) {
    int popBatchSize = p_popBatchSize ? numeric_cast<int>(*p_popBatchSize)
                                      : TableConsumable::DEFAULT_POP_BATCH_SIZE;
    int pri = p_pri ? numeric_cast<int>(*p_pri) : 0;
    SWSSTry(return (SWSSConsumerStateTable) new ConsumerStateTable(
        (DBConnector *)db, string(tableName), popBatchSize, pri));
}

void SWSSConsumerStateTable_free(SWSSConsumerStateTable tbl) {
    SWSSTry(delete (ConsumerStateTable *)tbl);
}

SWSSKeyOpFieldValuesArray SWSSConsumerStateTable_pops(SWSSConsumerStateTable tbl) {
    SWSSTry({
        deque<KeyOpFieldsValuesTuple> vkco;
        ((ConsumerStateTable *)tbl)->pops(vkco);
        return makeKeyOpFieldValuesArray(vkco);
    });
}

uint32_t SWSSConsumerStateTable_getFd(SWSSConsumerStateTable tbl) {
    SWSSTry(return numeric_cast<uint32_t>(((ConsumerStateTable *)tbl)->getFd()));
}

SWSSSelectResult SWSSConsumerStateTable_readData(SWSSConsumerStateTable tbl, uint32_t timeout_ms,
                                                 uint8_t interrupt_on_signal) {
    SWSSTry(return selectOne((ConsumerStateTable *)tbl, timeout_ms, interrupt_on_signal));
}
