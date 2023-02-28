#pragma once

#include <cstdint>
#include <list>
#include <unordered_map>

namespace rv64_emulator::libs {

template <typename Tkey = uint64_t, typename Tval = int64_t>
class LRUCache {
private:
    uint64_t m_capacity;

    // inline initialization need C++11 and above
    uint64_t m_current_size = 0;
    uint64_t m_hit_count    = 0;
    uint64_t m_miss_count   = 0;

    std::list<std::pair<Tkey, Tval>>                                              m_list;
    std::unordered_map<Tkey, typename std::list<std::pair<Tkey, Tval>>::iterator> m_cache;

public:
    LRUCache(const uint64_t capacity)
        : m_capacity(capacity) {
    }

    bool Get(Tkey key, Tval& ret_val) {
        if (m_cache.find(key) == m_cache.end()) {
            m_miss_count++;
            return false;
        }

        m_hit_count++;
        m_list.splice(m_list.begin(), m_list, m_cache[key]);
        m_cache[key] = m_list.begin();
        ret_val      = m_list.begin()->second;
        return true;
    }

    void Set(Tkey key, Tval value) {
        if (m_current_size < m_capacity) {
            if (m_cache.find(key) == m_cache.end()) {
                m_list.push_front({ key, value });
                m_current_size++;
                m_cache.emplace(key, m_list.begin());
            } else {
                m_list.splice(m_list.begin(), m_list, m_cache[key]);
                m_cache[key]           = m_list.begin();
                m_list.begin()->second = value;
            }
            return;
        }

        if (m_cache.find(key) != m_cache.end()) {
            m_list.splice(m_list.begin(), m_list, m_cache[key]);
            m_cache[key]           = m_list.begin();
            m_list.begin()->second = value;
            return;
        } else {
            m_cache.erase(m_list.back().first);

            m_list.splice(m_list.begin(), m_list, std::prev(m_list.end()));
            m_list.begin()->first  = key;
            m_list.begin()->second = value;

            m_cache.emplace(key, m_list.begin());
        }
    }

    void Reset() {
        m_current_size = 0;
        m_hit_count    = 0;
        m_miss_count   = 0;
        m_list.clear();
        m_cache.clear();
    }
};

} // namespace rv64_emulator::libs
