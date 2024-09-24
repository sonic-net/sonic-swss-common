#include <cstring>

#include "binaryserializer.h"
#include "table.h"

using namespace std;
using namespace swss;

template <class T> static inline T read_unaligned(const char *buffer) {
    T t;
    std::memcpy(&t, buffer, sizeof(T));
    return t;
}

template <class T> static inline void write_unaligned(char *buffer, T t) {
    std::memcpy(buffer, &t, sizeof(T));
}

class SerializerHelper {
    char *m_buffer;
    size_t m_buffer_size;
    char *m_current_position;
    size_t m_kvp_count;

  public:
    SerializerHelper(char *buffer, size_t size)
        : m_buffer(buffer), m_buffer_size(size), m_current_position(buffer + sizeof(size_t)),
          m_kvp_count(0) {}

    void setKeyAndValue(const char *key, size_t klen, const char *value, size_t vlen) {
        setData(key, klen);
        setData(value, vlen);
        m_kvp_count++;
    }

    size_t finalize() {
        // Write key/value pair count to the beginning of the buffer
        write_unaligned(m_buffer, m_kvp_count);

        // return total serialized length
        return static_cast<size_t>(m_current_position - m_buffer);
    }

    void setData(const char *data, size_t datalen) {
        if ((static_cast<size_t>(m_current_position - m_buffer) + datalen + sizeof(size_t)) >
            m_buffer_size) {
            SWSS_LOG_THROW("There is not enough buffer to serialize: current key count: %zu, "
                           "current data length: %zu, buffer size: %zu",
                           m_kvp_count, datalen, m_buffer_size);
        }
        write_unaligned(m_current_position, datalen);
        m_current_position += sizeof(size_t);

        std::memcpy(m_current_position, data, datalen);
        m_current_position += datalen;
    }
};

namespace swss {

BufferSlice serializeSharedBuffer(const std::string &dbName, const std::string &tableName,
                                  const std::vector<KeyOpFieldsValuesTuple> &kcos) {
    static thread_local vector<char> sharedBuffer;
    sharedBuffer.resize(SERIALIZE_SHARED_BUFFER_SIZE);
    size_t serializedLen =
        serializeBuffer(sharedBuffer.data(), sharedBuffer.size(), dbName, tableName, kcos);
    return {.data = sharedBuffer.data(), .len = serializedLen};
}

size_t serializeBuffer(char *buffer, size_t size, const string &dbName, const string &tableName,
                       const vector<swss::KeyOpFieldsValuesTuple> &kcos) {

    auto tmpSerializer = SerializerHelper(buffer, size);

    // Set the first pair as DB name and table name.
    tmpSerializer.setKeyAndValue(dbName.c_str(), dbName.length(), tableName.c_str(),
                                 tableName.length());
    for (auto &kco : kcos) {
        auto &key = kfvKey(kco);
        auto &fvs = kfvFieldsValues(kco);
        std::string fvs_len = std::to_string(fvs.size());
        // For each request, the first pair is the key and the number of attributes,
        // followed by the attribute pairs.
        // The operation is not set, when there is no attribute, it is a DEL request.
        tmpSerializer.setKeyAndValue(key.c_str(), key.length(), fvs_len.c_str(), fvs_len.length());
        for (auto &fv : fvs) {
            auto &field = fvField(fv);
            auto &value = fvValue(fv);
            tmpSerializer.setKeyAndValue(field.c_str(), field.length(), value.c_str(),
                                         value.length());
        }
    }

    return tmpSerializer.finalize();
}

void deserializeBuffer(const char *buffer, size_t size,
                       std::vector<swss::FieldValueTuple> &values) {
    size_t kvp_count = read_unaligned<size_t>(buffer);
    auto tmp_buffer = buffer + sizeof(size_t);
    while (kvp_count > 0) {
        kvp_count--;

        // read field name
        size_t keylen = read_unaligned<size_t>(tmp_buffer);
        tmp_buffer += sizeof(size_t);
        if ((size_t)(tmp_buffer - buffer) + keylen > size) {
            SWSS_LOG_THROW(
                "serialized key data was truncated, key length: %zu, increase buffer size: %zu",
                keylen, size);
        }
        auto pkey = string(tmp_buffer, keylen);
        tmp_buffer += keylen;

        // read value
        size_t vallen = read_unaligned<size_t>(tmp_buffer);
        tmp_buffer += sizeof(size_t);
        if ((size_t)(tmp_buffer - buffer) + vallen > size) {
            SWSS_LOG_THROW("serialized value data was truncated, value length: %zu increase "
                           "buffer size: %zu",
                           vallen, size);
        }
        auto pval = string(tmp_buffer, vallen);
        tmp_buffer += vallen;

        values.push_back(std::make_pair(pkey, pval));
    }
}

void deserializeBuffer(const char *buffer, const size_t size, std::string &dbName,
                       std::string &tableName,
                       std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>> &kcos) {
    std::vector<FieldValueTuple> values;
    deserializeBuffer(buffer, size, values);
    int fvs_size = -1;
    KeyOpFieldsValuesTuple kco;
    auto &key = kfvKey(kco);
    auto &op = kfvOp(kco);
    auto &fvs = kfvFieldsValues(kco);
    for (auto &fv : values) {
        auto &field = fvField(fv);
        auto &value = fvValue(fv);
        // The first pair is the DB name and the table name.
        if (fvs_size < 0) {
            dbName = field;
            tableName = value;
            fvs_size = 0;
            continue;
        }
        // This is the beginning of a request.
        // The first pair is the key and the number of attributes.
        // If the attribute count is zero, it is a DEL request.
        if (fvs_size == 0) {
            key = field;
            fvs_size = std::stoi(value);
            op = (fvs_size == 0) ? DEL_COMMAND : SET_COMMAND;
            fvs.clear();
        }
        // This is an attribute pair.
        else {
            fvs.push_back(fv);
            --fvs_size;
        }
        // We got the last attribute pair. This is the end of a request.
        if (fvs_size == 0) {
            kcos.push_back(std::make_shared<KeyOpFieldsValuesTuple>(kco));
        }
    }
}

} // namespace swss
