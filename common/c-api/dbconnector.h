#ifndef SWSS_COMMON_C_API_DBCONNECTOR_H
#define SWSS_COMMON_C_API_DBCONNECTOR_H

#include "util.h"
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void SWSSSonicDBConfig_initialize(const char *path);

void SWSSSonicDBConfig_initializeGlobalConfig(const char *path);

typedef struct SWSSDBConnectorOpaque *SWSSDBConnector;

// Pass 0 to timeout for infinity
SWSSDBConnector SWSSDBConnector_new_tcp(int32_t dbId, const char *hostname, uint16_t port,
                                        uint32_t timeout_ms);

// Pass 0 to timeout for infinity
SWSSDBConnector SWSSDBConnector_new_unix(int32_t dbId, const char *sock_path, uint32_t timeout_ms);

// Pass 0 to timeout for infinity
SWSSDBConnector SWSSDBConnector_new_named(const char *dbName, uint32_t timeout_ms, uint8_t isTcpConn);

void SWSSDBConnector_free(SWSSDBConnector db);

// Returns 0 when key doesn't exist, 1 when key was deleted
int8_t SWSSDBConnector_del(SWSSDBConnector db, const char *key);

void SWSSDBConnector_set(SWSSDBConnector db, const char *key, const char *value);

// Returns NULL if key doesn't exist.
// Result must be freed using free()
char *SWSSDBConnector_get(SWSSDBConnector db, const char *key);

// Returns 0 for false, 1 for true
int8_t SWSSDBConnector_exists(SWSSDBConnector db, const char *key);

// Returns 0 when key or field doesn't exist, 1 when field was deleted
int8_t SWSSDBConnector_hdel(SWSSDBConnector db, const char *key, const char *field);

void SWSSDBConnector_hset(SWSSDBConnector db, const char *key, const char *field,
                          const char *value);

// Returns NULL if key or field doesn't exist.
// Result must be freed using free()
char *SWSSDBConnector_hget(SWSSDBConnector db, const char *key, const char *field);

// Returns an empty map when the key doesn't exist.
// Result array and all of its elements must be freed using free()
SWSSFieldValueArray SWSSDBConnector_hgetall(SWSSDBConnector db, const char *key);

// Returns 0 when key or field doesn't exist, 1 when field exists
int8_t SWSSDBConnector_hexists(SWSSDBConnector db, const char *key, const char *field);

// std::vector<std::string> keys(const std::string &key);

// std::pair<int, std::vector<std::string>> scan(int cursor = 0, const char
// *match = "", uint32_t count = 10);

// template<typename InputIterator>
// void hmset(const std::string &key, InputIterator start, InputIterator stop);

// void hmset(const std::unordered_map<std::string,
// std::vector<std::pair<std::string, std::string>>>& multiHash);

// std::shared_ptr<std::string> get(const std::string &key);

// std::shared_ptr<std::string> hget(const std::string &key, const std::string
// &field);

// int64_t incr(const std::string &key);

// int64_t decr(const std::string &key);

// int64_t rpush(const std::string &list, const std::string &item);

// std::shared_ptr<std::string> blpop(const std::string &list, int timeout);

// void subscribe(const std::string &pattern);

// void psubscribe(const std::string &pattern);

// void punsubscribe(const std::string &pattern);

// int64_t publish(const std::string &channel, const std::string &message);

// void config_set(const std::string &key, const std::string &value);

// Returns 1 on success, 0 on failure
int8_t SWSSDBConnector_flushdb(SWSSDBConnector db);

// std::map<std::string, std::map<std::string, std::map<std::string,
// std::string>>> getall();
#ifdef __cplusplus
}
#endif

#endif
