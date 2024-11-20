#include <cstring>
#include <string>

#include "../dbconnector.h"
#include "../producerstatetable.h"
#include "dbconnector.h"
#include "producerstatetable.h"
#include "util.h"

using namespace swss;
using namespace std;

SWSSProducerStateTable SWSSProducerStateTable_new(SWSSDBConnector db, const char *tableName) {
    SWSSTry(return (SWSSProducerStateTable) new ProducerStateTable((DBConnector *)db,
                                                                   string(tableName)));
}

void SWSSProducerStateTable_free(SWSSProducerStateTable tbl) {
    SWSSTry(delete ((ProducerStateTable *)tbl));
}

void SWSSProducerStateTable_setBuffered(SWSSProducerStateTable tbl, uint8_t buffered) {
    SWSSTry(((ProducerStateTable *)tbl)->setBuffered((bool)buffered))
}

void SWSSProducerStateTable_set(SWSSProducerStateTable tbl, const char *key,
                                SWSSFieldValueArray values) {
    SWSSTry(((ProducerStateTable *)tbl)->set(string(key), takeFieldValueArray(std::move(values))));
}

void SWSSProducerStateTable_del(SWSSProducerStateTable tbl, const char *key) {
    SWSSTry(((ProducerStateTable *)tbl)->del(string(key)));
}

void SWSSProducerStateTable_flush(SWSSProducerStateTable tbl) {
    SWSSTry(((ProducerStateTable *)tbl)->flush());
}

int64_t SWSSProducerStateTable_count(SWSSProducerStateTable tbl) {
    SWSSTry(return ((ProducerStateTable *)tbl)->count());
}

void SWSSProducerStateTable_clear(SWSSProducerStateTable tbl) {
    SWSSTry(((ProducerStateTable *)tbl)->clear());
}

void SWSSProducerStateTable_create_temp_view(SWSSProducerStateTable tbl) {
    SWSSTry(((ProducerStateTable *)tbl)->create_temp_view());
}

void SWSSProducerStateTable_apply_temp_view(SWSSProducerStateTable tbl) {
    SWSSTry(((ProducerStateTable *)tbl)->apply_temp_view());
}
