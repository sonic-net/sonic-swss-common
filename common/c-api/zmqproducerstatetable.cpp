#include <boost/numeric/conversion/cast.hpp>

#include "../zmqproducerstatetable.h"
#include "zmqproducerstatetable.h"

using namespace std;
using namespace swss;
using boost::numeric_cast;

SWSSResult SWSSZmqProducerStateTable_new(SWSSDBConnector db, const char *tableName,
                                         SWSSZmqClient zmqc, uint8_t dbPersistence,
                                         SWSSZmqProducerStateTable *outTbl) {
    SWSSTry(*outTbl = (SWSSZmqProducerStateTable) new ZmqProducerStateTable(
                (DBConnector *)db, string(tableName), *(ZmqClient *)zmqc, dbPersistence));
}

SWSSResult SWSSZmqProducerStateTable_free(SWSSZmqProducerStateTable tbl) {
    SWSSTry(delete (ZmqProducerStateTable *)tbl);
}

SWSSResult SWSSZmqProducerStateTable_set(SWSSZmqProducerStateTable tbl, const char *key,
                                         SWSSFieldValueArray values) {
    SWSSTry(((ZmqProducerStateTable *)tbl)->set(string(key), takeFieldValueArray(values)));
}

SWSSResult SWSSZmqProducerStateTable_del(SWSSZmqProducerStateTable tbl, const char *key) {
    SWSSTry(((ZmqProducerStateTable *)tbl)->del(string(key)));
}

SWSSResult SWSSZmqProducerStateTable_dbUpdaterQueueSize(SWSSZmqProducerStateTable tbl,
                                                        uint64_t *outSize) {
    SWSSTry(*outSize =
                numeric_cast<uint64_t>(((ZmqProducerStateTable *)tbl)->dbUpdaterQueueSize()));
}
