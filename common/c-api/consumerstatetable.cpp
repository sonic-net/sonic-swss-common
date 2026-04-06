#include <boost/numeric/conversion/cast.hpp>
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
using boost::numeric_cast;

SWSSResult SWSSConsumerStateTable_new(SWSSDBConnector db, const char *tableName,
                                      const int32_t *p_popBatchSize, const int32_t *p_pri,
                                      SWSSConsumerStateTable *outTbl) {
    SWSSTry({
        int popBatchSize = p_popBatchSize ? numeric_cast<int>(*p_popBatchSize)
                                          : TableConsumable::DEFAULT_POP_BATCH_SIZE;
        int pri = p_pri ? numeric_cast<int>(*p_pri) : 0;
        *outTbl = (SWSSConsumerStateTable) new ConsumerStateTable(
            (DBConnector *)db, string(tableName), popBatchSize, pri);
    });
}

SWSSResult SWSSConsumerStateTable_free(SWSSConsumerStateTable tbl) {
    SWSSTry(delete (ConsumerStateTable *)tbl);
}

SWSSResult SWSSConsumerStateTable_pops(SWSSConsumerStateTable tbl,
                                       SWSSKeyOpFieldValuesArray *outArr) {
    SWSSTry({
        deque<KeyOpFieldsValuesTuple> vkco;
        ((ConsumerStateTable *)tbl)->pops(vkco);
        *outArr = makeKeyOpFieldValuesArray(vkco);
    });
}

SWSSResult SWSSConsumerStateTable_getFd(SWSSConsumerStateTable tbl, uint32_t *outFd) {
    SWSSTry(*outFd = numeric_cast<uint32_t>(((ConsumerStateTable *)tbl)->getFd()));
}

SWSSResult SWSSConsumerStateTable_readData(SWSSConsumerStateTable tbl, uint32_t timeout_ms,
                                           uint8_t interrupt_on_signal,
                                           SWSSSelectResult *outResult) {
    SWSSTry(*outResult = selectOne((ConsumerStateTable *)tbl, timeout_ms, interrupt_on_signal));
}
