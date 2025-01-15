#include <string>
#include <vector>

#include "../dbconnector.h"
#include "../table.h"
#include "table.h"
#include "util.h"

using namespace swss;
using namespace std;

SWSSTable SWSSTable_new(SWSSDBConnector db, const char *tableName) {
    SWSSTry(return (SWSSTable) new Table((DBConnector *)db, string(tableName)));
}

void SWSSTable_free(SWSSTable tbl) {
    SWSSTry(delete (Table *)tbl);
}

int8_t SWSSTable_get(SWSSTable tbl, const char *key, SWSSFieldValueArray *outValues) {
    SWSSTry({
        vector<FieldValueTuple> fvs;
        bool exists = ((Table *)tbl)->get(string(key), fvs);
        if (exists) {
            *outValues = makeFieldValueArray(fvs);
            return 1;
        } else {
            return 0;
        }
    });
}

int8_t SWSSTable_hget(SWSSTable tbl, const char *key, const char *field, SWSSString *outValue) {
    SWSSTry({
        string s;
        bool exists = ((Table *)tbl)->hget(string(key), string(field), s);
        if (exists) {
            *outValue = makeString(move(s));
            return 1;
        } else {
            return 0;
        }
    });
}

void SWSSTable_set(SWSSTable tbl, const char *key, SWSSFieldValueArray values) {
    SWSSTry({
        vector<FieldValueTuple> fvs = takeFieldValueArray(values);
        ((Table *)tbl)->set(string(key), fvs);
    });
}

void SWSSTable_hset(SWSSTable tbl, const char *key, const char *field, SWSSStrRef value) {
    SWSSTry({ ((Table *)tbl)->hset(string(key), string(field), takeStrRef(value)); });
}

void SWSSTable_del(SWSSTable tbl, const char *key) {
    SWSSTry({ ((Table *)tbl)->del(string(key)); });
}

void SWSSTable_hdel(SWSSTable tbl, const char *key, const char *field) {
    SWSSTry({ ((Table *)tbl)->hdel(string(key), string(field)); });
}

SWSSStringArray SWSSTable_getKeys(SWSSTable tbl) {
    SWSSTry({
        vector<string> keys;
        ((Table *)tbl)->getKeys(keys);
        return makeStringArray(move(keys));
    })
}
