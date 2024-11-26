#include <boost/numeric/conversion/cast.hpp>

#include "zmqproducerstatetable.h"
#include "../zmqproducerstatetable.h"

using namespace std;
using namespace swss;
using boost::numeric_cast;

SWSSZmqProducerStateTable SWSSZmqProducerStateTable_new(SWSSDBConnector db, const char *tableName,
                                                        SWSSZmqClient zmqc, uint8_t dbPersistence) {

    SWSSTry(return (SWSSZmqProducerStateTable) new ZmqProducerStateTable(
        (DBConnector *)db, string(tableName), *(ZmqClient *)zmqc, dbPersistence));
}

void SWSSZmqProducerStateTable_free(SWSSZmqProducerStateTable tbl) {
    SWSSTry(delete (ZmqProducerStateTable *)tbl);
}

void SWSSZmqProducerStateTable_set(SWSSZmqProducerStateTable tbl, const char *key,
                                   SWSSFieldValueArray values) {
    SWSSTry(((ZmqProducerStateTable *)tbl)->set(string(key), takeFieldValueArray(values)));
}

void SWSSZmqProducerStateTable_del(SWSSZmqProducerStateTable tbl, const char *key) {
    SWSSTry(((ZmqProducerStateTable *)tbl)->del(string(key)));
}

uint64_t SWSSZmqProducerStateTable_dbUpdaterQueueSize(SWSSZmqProducerStateTable tbl) {
    SWSSTry(return numeric_cast<uint64_t>(((ZmqProducerStateTable *)tbl)->dbUpdaterQueueSize()));
}
