#ifndef __BINARY_SERIALIZER__
#define __BINARY_SERIALIZER__

#include <string>
#include <vector>

#include "table.h"

namespace swss {

size_t serializeBuffer(char *buffer, size_t size, const std::string &dbName,
                       const std::string &tableName,
                       const std::vector<swss::KeyOpFieldsValuesTuple> &kcos);

void deserializeBuffer(const char *buffer, size_t size, std::vector<swss::FieldValueTuple> &values);

void deserializeBuffer(const char *buffer, const size_t size, std::string &dbName,
                       std::string &tableName,
                       std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> &kcos);

} // namespace swss
#endif
