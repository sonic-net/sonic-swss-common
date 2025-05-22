#include <boost/numeric/conversion/cast.hpp>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <system_error>

#include "../dbconnector.h"
#include "../subscriberstatetable.h"
#include "../table.h"
#include "subscriberstatetable.h"
#include "util.h"

using namespace swss;
using namespace std;
using boost::numeric_cast;

SWSSResult SWSSSubscriberStateTable_new(SWSSDBConnector db, const char *tableName,
                                        const int32_t *p_popBatchSize, const int32_t *p_pri,
                                        SWSSSubscriberStateTable *outTbl) {
    SWSSTry({
        int popBatchSize = p_popBatchSize ? numeric_cast<int>(*p_popBatchSize)
                                          : TableConsumable::DEFAULT_POP_BATCH_SIZE;
        int pri = p_pri ? numeric_cast<int>(*p_pri) : 0;
        *outTbl = (SWSSSubscriberStateTable) new SubscriberStateTable(
            (DBConnector *)db, string(tableName), popBatchSize, pri);
    });
}

SWSSResult SWSSSubscriberStateTable_free(SWSSSubscriberStateTable tbl) {
    SWSSTry(delete (SubscriberStateTable *)tbl);
}

SWSSResult SWSSSubscriberStateTable_pops(SWSSSubscriberStateTable tbl,
                                         SWSSKeyOpFieldValuesArray *outArr) {
    SWSSTry({
        deque<KeyOpFieldsValuesTuple> vkco;
        ((SubscriberStateTable *)tbl)->pops(vkco);
        *outArr = makeKeyOpFieldValuesArray(vkco);
    });
}

SWSSResult SWSSSubscriberStateTable_getFd(SWSSSubscriberStateTable tbl, uint32_t *outFd) {
    SWSSTry(*outFd = numeric_cast<uint32_t>(((SubscriberStateTable *)tbl)->getFd()));
}

SWSSResult SWSSSubscriberStateTable_readData(SWSSSubscriberStateTable tbl, uint32_t timeout_ms,
                                             uint8_t interrupt_on_signal,
                                             SWSSSelectResult *outResult) {
    SWSSTry(*outResult = selectOne((SubscriberStateTable *)tbl, timeout_ms, interrupt_on_signal));
}
