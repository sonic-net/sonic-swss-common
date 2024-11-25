#include <cstring>
#include <string>
#include <utility>

#include "../dbconnector.h"
#include "dbconnector.h"
#include "util.h"

using namespace swss;
using namespace std;

void SWSSSonicDBConfig_initialize(const char *path) {
    SWSSTry(SonicDBConfig::initialize(path));
}

void SWSSSonicDBConfig_initializeGlobalConfig(const char *path) {
    SWSSTry(SonicDBConfig::initializeGlobalConfig(path));
}

SWSSDBConnector SWSSDBConnector_new_tcp(int32_t dbId, const char *hostname, uint16_t port,
                                        uint32_t timeout) {
    SWSSTry(return (SWSSDBConnector) new DBConnector(dbId, string(hostname), port, timeout));
}

SWSSDBConnector SWSSDBConnector_new_unix(int32_t dbId, const char *sock_path, uint32_t timeout) {
    SWSSTry(return (SWSSDBConnector) new DBConnector(dbId, string(sock_path), timeout));
}

SWSSDBConnector SWSSDBConnector_new_named(const char *dbName, uint32_t timeout_ms, uint8_t isTcpConn) {
    SWSSTry(return (SWSSDBConnector) new DBConnector(string(dbName), timeout_ms, isTcpConn));
}

void SWSSDBConnector_free(SWSSDBConnector db) {
    delete (DBConnector *)db;
}

int8_t SWSSDBConnector_del(SWSSDBConnector db, const char *key) {
    SWSSTry(return ((DBConnector *)db)->del(string(key)) ? 1 : 0);
}

void SWSSDBConnector_set(SWSSDBConnector db, const char *key, SWSSStrRef value) {
    SWSSTry(((DBConnector *)db)->set(string(key), takeStrRef(value)));
}

SWSSString SWSSDBConnector_get(SWSSDBConnector db, const char *key) {
    SWSSTry({
        shared_ptr<string> s = ((DBConnector *)db)->get(string(key));
        return s ? makeString(move(*s)) : nullptr;
    });
}

int8_t SWSSDBConnector_exists(SWSSDBConnector db, const char *key) {
    SWSSTry(return ((DBConnector *)db)->exists(string(key)) ? 1 : 0);
}

int8_t SWSSDBConnector_hdel(SWSSDBConnector db, const char *key, const char *field) {
    SWSSTry(return ((DBConnector *)db)->hdel(string(key), string(field)) ? 1 : 0);
}

void SWSSDBConnector_hset(SWSSDBConnector db, const char *key, const char *field,
                          SWSSStrRef value) {
    SWSSTry(((DBConnector *)db)->hset(string(key), string(field), takeStrRef(value)));
}

SWSSString SWSSDBConnector_hget(SWSSDBConnector db, const char *key, const char *field) {
    SWSSTry({
        shared_ptr<string> s = ((DBConnector *)db)->hget(string(key), string(field));
        return s ? makeString(move(*s)) : nullptr;
    });
}

SWSSFieldValueArray SWSSDBConnector_hgetall(SWSSDBConnector db, const char *key) {
    SWSSTry({
        auto map = ((DBConnector *)db)->hgetall(string(key));

        // We can't move keys out of the map, we have to copy them, until C++17 map::extract so we
        // copy them here into a vector to avoid needing an overload on makeFieldValueArray
        vector<pair<string, string>> pairs;
        pairs.reserve(map.size());
        for (auto &pair : map)
            pairs.push_back(make_pair(pair.first, move(pair.second)));

        return makeFieldValueArray(std::move(pairs));
    });
}

int8_t SWSSDBConnector_hexists(SWSSDBConnector db, const char *key, const char *field) {
    SWSSTry(return ((DBConnector *)db)->hexists(string(key), string(field)) ? 1 : 0);
}

int8_t SWSSDBConnector_flushdb(SWSSDBConnector db) {
    SWSSTry(return ((DBConnector *)db)->flushdb() ? 1 : 0);
}
