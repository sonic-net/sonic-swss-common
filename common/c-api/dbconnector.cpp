#include <cstring>
#include <string>

#include "../dbconnector.h"
#include "dbconnector.h"
#include "util.h"

using namespace swss;
using namespace std;

SWSSDBConnector SWSSDBConnector_new_tcp(int32_t dbId, const char *hostname, uint16_t port,
                                        uint32_t timeout) {
    SWSSTry(return (SWSSDBConnector) new DBConnector(dbId, string(hostname), port, timeout));
}

SWSSDBConnector SWSSDBConnector_new_unix(int32_t dbId, const char *sock_path, uint32_t timeout) {
    SWSSTry(return (SWSSDBConnector) new DBConnector(dbId, string(sock_path), timeout));
}

void SWSSDBConnector_free(SWSSDBConnector db) {
    delete (DBConnector *)db;
}

int8_t SWSSDBConnector_del(SWSSDBConnector db, const char *key) {
    SWSSTry(return ((DBConnector *)db)->del(string(key)) ? 1 : 0);
}

void SWSSDBConnector_set(SWSSDBConnector db, const char *key, const char *value) {
    SWSSTry(((DBConnector *)db)->set(string(key), string(value)));
}

char *SWSSDBConnector_get(SWSSDBConnector db, const char *key) {
    SWSSTry({
        shared_ptr<string> s = ((DBConnector *)db)->get(string(key));
        return s ? strdup(s->c_str()) : nullptr;
    });
}

int8_t SWSSDBConnector_exists(SWSSDBConnector db, const char *key) {
    SWSSTry(return ((DBConnector *)db)->exists(string(key)) ? 1 : 0);
}

int8_t SWSSDBConnector_hdel(SWSSDBConnector db, const char *key, const char *field) {
    SWSSTry(return ((DBConnector *)db)->hdel(string(key), string(field)) ? 1 : 0);
}

void SWSSDBConnector_hset(SWSSDBConnector db, const char *key, const char *field,
                          const char *value) {
    SWSSTry(((DBConnector *)db)->hset(string(key), string(field), string(value)));
}

char *SWSSDBConnector_hget(SWSSDBConnector db, const char *key, const char *field) {
    SWSSTry({
        shared_ptr<string> s = ((DBConnector *)db)->hget(string(key), string(field));
        return s ? strdup(s->c_str()) : nullptr;
    });
}

SWSSFieldValueArray SWSSDBConnector_hgetall(SWSSDBConnector db, const char *key) {
    SWSSTry({
        auto map = ((DBConnector *)db)->hgetall(key);
        return makeFieldValueArray(map);
    });
}

int8_t SWSSDBConnector_hexists(SWSSDBConnector db, const char *key, const char *field) {
    SWSSTry(return ((DBConnector *)db)->hexists(string(key), string(field)) ? 1 : 0);
}

int8_t SWSSDBConnector_flushdb(SWSSDBConnector db) {
    SWSSTry(return ((DBConnector *)db)->flushdb() ? 1 : 0);
}
