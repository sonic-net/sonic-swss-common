#ifndef __BINARY_SERIALIZER__
#define __BINARY_SERIALIZER__

#include "common/rediscommand.h"
#include "common/table.h"

#include <string>

using namespace std;

namespace swss {

template <class T> static inline T read_unaligned(const char *buffer) {
    T t;
    std::memcpy(&t, buffer, sizeof(T));
    return t;
}

template <class T> static inline void write_unaligned(char *buffer, T t) {
    std::memcpy(buffer, &t, sizeof(T));
}

class BinarySerializer {
public:
    static size_t serializedSize(const string &dbName, const string &tableName,
                                 const vector<KeyOpFieldsValuesTuple> &kcos) {
        size_t n = 0;
        n += dbName.size() + sizeof(size_t);
        n += tableName.size() + sizeof(size_t);

        for (const KeyOpFieldsValuesTuple &kco : kcos) {
            const vector<FieldValueTuple> &fvs = kfvFieldsValues(kco);
            n += kfvKey(kco).size() + sizeof(size_t);
            n += to_string(fvs.size()).size() + sizeof(size_t);

            for (const FieldValueTuple &fv : fvs) {
                n += fvField(fv).size() + sizeof(size_t);
                n += fvValue(fv).size() + sizeof(size_t);
            }
        }

        return n + sizeof(size_t);
    }

    static size_t serializeBuffer(
        char* buffer,
        const size_t size,
        const std::string& dbName,
        const std::string& tableName,
        const std::vector<KeyOpFieldsValuesTuple>& kcos)
    {
        auto tmpSerializer = BinarySerializer(buffer, size);

        // Set the first pair as DB name and table name.
        tmpSerializer.setKeyAndValue(
                                    dbName.c_str(), dbName.length(),
                                    tableName.c_str(), tableName.length());
        for (auto& kco : kcos)
        {
            auto& key = kfvKey(kco);
            auto& fvs = kfvFieldsValues(kco);
            std::string fvs_len = std::to_string(fvs.size());
            // For each request, the first pair is the key and the number of attributes,
            // followed by the attribute pairs.
            // The operation is not set, when there is no attribute, it is a DEL request.
            tmpSerializer.setKeyAndValue(
                                        key.c_str(), key.length(),
                                        fvs_len.c_str(), fvs_len.length());
            for (auto& fv : fvs)
            {
                auto& field = fvField(fv);
                auto& value = fvValue(fv);
                tmpSerializer.setKeyAndValue(
                                            field.c_str(), field.length(),
                                            value.c_str(), value.length());
            }
        }

        return tmpSerializer.finalize();
    }

    static void deserializeBuffer(
        const char* buffer,
        const size_t size,
        std::vector<swss::FieldValueTuple>& values)
    {
        size_t kvp_count = read_unaligned<size_t>(buffer);
        auto tmp_buffer = buffer + sizeof(size_t);
        while (kvp_count > 0)
        {
            kvp_count--;

            // read key and value from buffer
            size_t keylen = read_unaligned<size_t>(tmp_buffer);
            tmp_buffer += sizeof(size_t);
            if (((size_t)(tmp_buffer - buffer) + keylen) > size) {
                SWSS_LOG_THROW("serialized key data was truncated, key length: %zu, increase buffer size: %zu", keylen, size);
            }

            auto pkey = string(tmp_buffer, keylen);
            tmp_buffer += keylen;

            size_t vallen = read_unaligned<size_t>(tmp_buffer);
            tmp_buffer += sizeof(size_t);
            if ((size_t)(tmp_buffer - buffer) + vallen > size) {
                SWSS_LOG_THROW("serialized value data was truncated, value length: %zu, increase buffer size: %zu", vallen, size);
            }
            
            auto pval = string(tmp_buffer, vallen);
            tmp_buffer += vallen;

            values.push_back(std::make_pair(pkey, pval));
        }
    }

    static void deserializeBuffer(
        const char* buffer,
        const size_t size,
        std::string& dbName,
        std::string& tableName,
        std::vector<std::shared_ptr<KeyOpFieldsValuesTuple>>& kcos)
    {
        std::vector<FieldValueTuple> values;
        deserializeBuffer(buffer, size, values);
        int fvs_size = -1;
        KeyOpFieldsValuesTuple kco;
        auto& key = kfvKey(kco);
        auto& op = kfvOp(kco);
        auto& fvs = kfvFieldsValues(kco);
        for (auto& fv : values)
        {
            auto& field = fvField(fv);
            auto& value = fvValue(fv);
            // The first pair is the DB name and the table name.
            if (fvs_size < 0)
            {
                dbName = field;
                tableName = value;
                fvs_size = 0;
                continue;
            }
            // This is the beginning of a request.
            // The first pair is the key and the number of attributes.
            // If the attribute count is zero, it is a DEL request.
            if (fvs_size == 0)
            {
                key = field;
                fvs_size = std::stoi(value);
                op = (fvs_size == 0) ? DEL_COMMAND : SET_COMMAND;
                fvs.clear();
            }
            // This is an attribut pair.
            else
            {
                fvs.push_back(fv);
                --fvs_size;
            }
            // We got the last attribut pair. This is the end of a request.
            if (fvs_size == 0)
            {
                kcos.push_back(std::make_shared<KeyOpFieldsValuesTuple>(kco));
            }
        }
    }

private:
    char* m_buffer;
    const size_t m_buffer_size;
    char* m_current_position;
    size_t m_kvp_count;

    BinarySerializer(char* buffer, const size_t size)
        : m_buffer(buffer), m_buffer_size(size)
    {
        resetSerializer();
    }

    void resetSerializer()
    {
        m_current_position = m_buffer + sizeof(size_t);
        m_kvp_count = 0;
    }

    void setKeyAndValue(const char* key, size_t klen,
                        const char* value, size_t vlen)
    {
        setData(key, klen);
        setData(value, vlen);

        m_kvp_count++;
    }

    size_t finalize()
    {
        // set key value pair count to message
        write_unaligned(m_buffer, m_kvp_count);

        // return size
        return (size_t)(m_current_position - m_buffer);
    }

    void setData(const char* data, size_t datalen)
    {
        if ((size_t)(m_current_position - m_buffer) + datalen + sizeof(size_t) > m_buffer_size)
        {
            SWSS_LOG_THROW("There is not enough buffer for binary serializer to serialize,\n"
                           "  key count: %zu, data length %zu, buffer size: %zu",
                           m_kvp_count, datalen, m_buffer_size);
        }

        write_unaligned(m_current_position, datalen);
        m_current_position += sizeof(size_t);

        memcpy(m_current_position, data, datalen);
        m_current_position += datalen;
    }
};

}
#endif
