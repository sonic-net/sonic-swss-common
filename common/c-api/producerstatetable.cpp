#include <cstring>
#include <string>

#include "../dbconnector.h"
#include "../producerstatetable.h"
#include "dbconnector.h"
#include "producerstatetable.h"
#include "util.h"

using namespace swss;
using namespace std;

SWSSResult SWSSProducerStateTable_new(SWSSDBConnector db, const char *tableName,
                                      SWSSProducerStateTable *outTbl) {
    SWSSTry(*outTbl = (SWSSProducerStateTable) new ProducerStateTable((DBConnector *)db,
                                                                      string(tableName)));
}

SWSSResult SWSSProducerStateTable_free(SWSSProducerStateTable tbl) {
    SWSSTry(delete ((ProducerStateTable *)tbl));
}

SWSSResult SWSSProducerStateTable_setBuffered(SWSSProducerStateTable tbl, uint8_t buffered) {
    SWSSTry(((ProducerStateTable *)tbl)->setBuffered((bool)buffered));
}

SWSSResult SWSSProducerStateTable_set(SWSSProducerStateTable tbl, const char *key,
                                      SWSSFieldValueArray values) {
    SWSSTry(((ProducerStateTable *)tbl)->set(string(key), takeFieldValueArray(std::move(values))));
}

SWSSResult SWSSProducerStateTable_del(SWSSProducerStateTable tbl, const char *key) {
    SWSSTry(((ProducerStateTable *)tbl)->del(string(key)));
}

SWSSResult SWSSProducerStateTable_flush(SWSSProducerStateTable tbl) {
    SWSSTry(((ProducerStateTable *)tbl)->flush());
}

SWSSResult SWSSProducerStateTable_count(SWSSProducerStateTable tbl, int64_t *outCount) {
    SWSSTry(*outCount = ((ProducerStateTable *)tbl)->count());
}

SWSSResult SWSSProducerStateTable_clear(SWSSProducerStateTable tbl) {
    SWSSTry(((ProducerStateTable *)tbl)->clear());
}

SWSSResult SWSSProducerStateTable_create_temp_view(SWSSProducerStateTable tbl) {
    SWSSTry(((ProducerStateTable *)tbl)->create_temp_view());
}

SWSSResult SWSSProducerStateTable_apply_temp_view(SWSSProducerStateTable tbl) {
    SWSSTry(((ProducerStateTable *)tbl)->apply_temp_view());
}
