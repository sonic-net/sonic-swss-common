#pragma once

#include <map>
#include <string>
#include <mutex>

namespace swss
{
    template <typename K, typename V>
        class ConcurrentMap
        {
            public:

                ConcurrentMap() = default;

            public:

                size_t size()
                {
                    std::lock_guard<std::mutex> _lock(m_mutex);

                    return m_map.size();
                }

                bool contains(const K& key)
                {
                    std::lock_guard<std::mutex> _lock(m_mutex);

                    return m_map.find(key) != m_map.end();
                }

                void insert(const std::pair<K,V>& pair)
                {
                    std::lock_guard<std::mutex> _lock(m_mutex);

                    m_map.insert(pair);
                }

                void set(const K& key, const V& value)
                {
                    std::lock_guard<std::mutex> _lock(m_mutex);

                    m_map[key] = value;
                }

                // return copy
                V get(const K& key)
                {
                    std::lock_guard<std::mutex> _lock(m_mutex);

                    return m_map[key];
                }

                // return copy
                std::map<K,V> getCopy()
                {
                    std::lock_guard<std::mutex> _lock(m_mutex);

                    return m_map;
                }

            private:

                ConcurrentMap(const ConcurrentMap&);
                ConcurrentMap& operator=(const ConcurrentMap&);

            private:

                std::map<K,V> m_map;

                std::mutex m_mutex;
        };
}
