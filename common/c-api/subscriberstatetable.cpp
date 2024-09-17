#include <cstdlib>
#include <cstring>
#include <deque>

#include "../dbconnector.h"
#include "../rediscommand.h"
#include "../subscriberstatetable.h"
#include "../table.h"
#include "subscriberstatetable.h"
#include "util.h"

using namespace swss;
using namespace std;

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

uint8_t SWSSSubscriberStateTable_hasData(SWSSSubscriberStateTable tbl) {
    SWSSTry(return ((SubscriberStateTable *)tbl)->hasData() ? 1 : 0);
}

uint8_t SWSSSubscriberStateTable_hasCachedData(SWSSSubscriberStateTable tbl) {
    SWSSTry(return ((SubscriberStateTable *)tbl)->hasCachedData() ? 1 : 0);
}

uint8_t SWSSSubscriberStateTable_initializedWithData(SWSSSubscriberStateTable tbl) {
    SWSSTry(return ((SubscriberStateTable *)tbl)->initializedWithData() ? 1 : 0);
}

void SWSSSubscriberStateTable_readData(SWSSSubscriberStateTable tbl) {
    SWSSTry(((SubscriberStateTable *)tbl)->readData());
}
