#ifndef __BINARY_SERIALIZER__
#define __BINARY_SERIALIZER__

#include "common/armhelper.h"

namespace swss {

class BinarySerializer {
public:
    static size_t serializeBuffer(
        const char* buffer,
        const size_t size,
        const std::string& key,
        const std::vector<swss::FieldValueTuple>& values,
        const std::string& command)
    {
        auto tmpSerializer = BinarySerializer(buffer, size);

        tmpSerializer.setKeyAndValue(key.c_str(), key.length(), command.c_str(), command.length());
        for (auto& kvp : values)
        {
            auto& field = fvField(kvp);
            auto& value = fvValue(kvp);
            tmpSerializer.setKeyAndValue(field.c_str(), field.length(), value.c_str(), value.length());
        }

        return tmpSerializer.finalize();
    }

    static void deserializeBuffer(
        const char* buffer,
        const size_t size,
        std::vector<swss::FieldValueTuple>& values)
    {
        WARNINGS_NO_CAST_ALIGN;
        auto pkvp_count = (const size_t*)buffer;
        WARNINGS_RESET;

        size_t kvp_count = *pkvp_count;
        auto tmp_buffer = buffer + sizeof(size_t);
        while (kvp_count > 0)
        {
            kvp_count--;

            // read key and value from buffer
            WARNINGS_NO_CAST_ALIGN;
            auto pkeylen = (const size_t*)tmp_buffer;
            WARNINGS_RESET;

            tmp_buffer += sizeof(size_t);
            if ((size_t)(tmp_buffer - buffer + *pkeylen)  > size)
            {
                SWSS_LOG_THROW("serialized key data was truncated, key length: %zu, increase buffer size: %zu",
                                                                                            *pkeylen,
                                                                                            size);
            }

            auto pkey = tmp_buffer;
            tmp_buffer += *pkeylen;

            WARNINGS_NO_CAST_ALIGN;
            auto pvallen = (const size_t*)tmp_buffer;
            WARNINGS_RESET;

            tmp_buffer += sizeof(size_t);
            if ((size_t)(tmp_buffer - buffer + *pvallen)  > size)
            {
                SWSS_LOG_THROW("serialized value data was truncated,, value length: %zu increase buffer size: %zu",
                                                                                            *pvallen,
                                                                                            size);
            }
            
            auto pval = tmp_buffer;
            tmp_buffer += *pvallen;

            values.push_back(std::make_pair(pkey, pval));
        }
    }

private:
    const char* m_buffer;
    const size_t m_buffer_size;
    char* m_current_position;
    size_t m_kvp_count;

    BinarySerializer(const char* buffer, const size_t size)
        : m_buffer(buffer), m_buffer_size(size)
    {
        resetSerializer();
    }

    void resetSerializer()
    {
        m_current_position = const_cast<char*>(m_buffer) + sizeof(size_t);
        m_kvp_count = 0;
    }

    void setKeyAndValue(const char* key, size_t klen, const char* value, size_t vlen)
    {
        // to improve deserialize performance, copy null-terminated string. 
        setData(key, klen + 1);
        setData(value, vlen + 1);

        m_kvp_count++;
    }

    size_t finalize()
    {
        // set key value pair count to message
        WARNINGS_NO_CAST_ALIGN;
        size_t* pkvp_count = (size_t*)const_cast<char*>(m_buffer);
        WARNINGS_RESET;

        *pkvp_count = m_kvp_count;

        // return size
        return m_current_position - m_buffer;
    }

    void setData(const char* data, size_t datalen)
    {
        if ((size_t)(m_current_position - m_buffer + datalen + sizeof(size_t)) > m_buffer_size)
        {
            SWSS_LOG_THROW("There are not enough buffer for binary serializer to serialize,\
                             key count: %zu, data length %zu, buffer size: %zu",
                                                m_kvp_count,
                                                datalen,
                                                m_buffer_size);
        }

        WARNINGS_NO_CAST_ALIGN;
        size_t* pdatalen = (size_t*)m_current_position;
        WARNINGS_RESET;

        *pdatalen = datalen;
        m_current_position += sizeof(size_t);

        memcpy(m_current_position, data, datalen);
        m_current_position += datalen;
    }
};

}
#endif
