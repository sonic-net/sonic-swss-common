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
