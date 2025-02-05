#include <string>
#include <vector>

#include "../dbconnector.h"
#include "../table.h"
#include "table.h"
#include "util.h"

using namespace swss;
using namespace std;

SWSSResult SWSSTable_new(SWSSDBConnector db, const char *tableName, SWSSTable *outTbl) {
    SWSSTry(*outTbl = (SWSSTable) new Table((DBConnector *)db, string(tableName)));
}

SWSSResult SWSSTable_free(SWSSTable tbl) {
    SWSSTry(delete (Table *)tbl);
}

SWSSResult SWSSTable_get(SWSSTable tbl, const char *key, SWSSFieldValueArray *outValues,
                         int8_t *outExists) {
    SWSSTry({
        vector<FieldValueTuple> fvs;
        bool exists = ((Table *)tbl)->get(string(key), fvs);
        if (exists) {
            *outValues = makeFieldValueArray(fvs);
            *outExists = 1;
        } else {
            *outExists = 0;
        }
    });
}

SWSSResult SWSSTable_hget(SWSSTable tbl, const char *key, const char *field, SWSSString *outValue,
                          int8_t *outExists) {
    SWSSTry({
        string s;
        bool exists = ((Table *)tbl)->hget(string(key), string(field), s);
        if (exists) {
            *outValue = makeString(move(s));
            *outExists = 1;
        } else {
            *outExists = 0;
        }
    });
}

SWSSResult SWSSTable_set(SWSSTable tbl, const char *key, SWSSFieldValueArray values) {
    SWSSTry({
        vector<FieldValueTuple> fvs = takeFieldValueArray(values);
        ((Table *)tbl)->set(string(key), fvs);
    });
}

SWSSResult SWSSTable_hset(SWSSTable tbl, const char *key, const char *field, SWSSStrRef value) {
    SWSSTry(((Table *)tbl)->hset(string(key), string(field), takeStrRef(value)));
}

SWSSResult SWSSTable_del(SWSSTable tbl, const char *key) {
    SWSSTry(((Table *)tbl)->del(string(key)));
}

SWSSResult SWSSTable_hdel(SWSSTable tbl, const char *key, const char *field) {
    SWSSTry(((Table *)tbl)->hdel(string(key), string(field)));
}

SWSSResult SWSSTable_getKeys(SWSSTable tbl, SWSSStringArray *outKeys) {
    SWSSTry({
        vector<string> keys;
        ((Table *)tbl)->getKeys(keys);
        *outKeys = makeStringArray(move(keys));
    });
}
