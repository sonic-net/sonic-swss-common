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

SWSSSubscriberStateTable SWSSSubscriberStateTable_new(SWSSDBConnector db, const char *tableName,
                                                      const int32_t *p_popBatchSize,
                                                      const int32_t *p_pri) {
    int popBatchSize = p_popBatchSize ? numeric_cast<int>(*p_popBatchSize)
                                      : TableConsumable::DEFAULT_POP_BATCH_SIZE;
    int pri = p_pri ? numeric_cast<int>(*p_pri) : 0;
    SWSSTry(return (SWSSSubscriberStateTable) new SubscriberStateTable(
        (DBConnector *)db, string(tableName), popBatchSize, pri));
}

void SWSSSubscriberStateTable_free(SWSSSubscriberStateTable tbl) {
    delete (SubscriberStateTable *)tbl;
}

SWSSKeyOpFieldValuesArray SWSSSubscriberStateTable_pops(SWSSSubscriberStateTable tbl) {
    SWSSTry({
        deque<KeyOpFieldsValuesTuple> vkco;
        ((SubscriberStateTable *)tbl)->pops(vkco);
        return makeKeyOpFieldValuesArray(vkco);
    });
}

uint32_t SWSSSubscriberStateTable_getFd(SWSSSubscriberStateTable tbl) {
    SWSSTry(return numeric_cast<uint32_t>(((SubscriberStateTable *)tbl)->getFd()));
}

SWSSSelectResult SWSSSubscriberStateTable_readData(SWSSSubscriberStateTable tbl,
                                                   uint32_t timeout_ms,
                                                   uint8_t interrupt_on_signal) {
    SWSSTry(return selectOne((SubscriberStateTable *)tbl, timeout_ms, interrupt_on_signal));
}
