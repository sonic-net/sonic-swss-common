#ifndef __BINARY_SERIALIZER__
#define __BINARY_SERIALIZER__

#include <string>
#include <vector>

#include "table.h"

namespace swss {
namespace BinarySerializer {

struct BufferSlice {
    const char *data;
    size_t len;
};

const size_t SERIALIZE_SHARED_BUFFER_SIZE = 1024 * 1024 * 16;

// Serialize into and return a shared, thread-local buffer. Since the buffer is reused, the returned
// BufferSlice is invalidated upon calling this function again.
BufferSlice serializeSharedBuffer(const std::string &dbName, const std::string &tableName,
                                  const std::vector<KeyOpFieldsValuesTuple> &kcos);

size_t serializeBuffer(char *buffer, size_t size, const std::string &dbName,
                       const std::string &tableName,
                       const std::vector<swss::KeyOpFieldsValuesTuple> &kcos);

void deserializeBuffer(const char *buffer, size_t size, std::vector<swss::FieldValueTuple> &values);

void deserializeBuffer(const char *buffer, const size_t size, std::string &dbName,
                       std::string &tableName,
                       std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> &kcos);

} // namespace BinarySerializer
} // namespace swss
#endif
