#pragma once

#include <cstdint>
#include <list>
#include <unordered_map>
#include <utility>

namespace rv64_emulator::libs {

template <typename TKey = uint64_t, typename TVal = int64_t>
class LRUCache {
 private:
  uint64_t capacity_;

  uint64_t cur_size_{0};
  uint64_t hit_count_{0};
  uint64_t miss_count_{0};

  std::list<std::pair<TKey, TVal>> kv_node_;

  std::unordered_map<TKey, typename std::list<std::pair<TKey, TVal>>::iterator>
      cache_;

 public:
  explicit LRUCache(const uint64_t capacity) : capacity_(capacity) {}

  bool Get(TKey key, TVal* ret_val) {
    if (cache_.find(key) == cache_.end()) {
      miss_count_++;
      return false;
    }

    hit_count_++;
    kv_node_.splice(kv_node_.begin(), kv_node_, cache_[key]);
    cache_[key] = kv_node_.begin();
    *ret_val = kv_node_.begin()->second;
    return true;
  }

  void Set(TKey key, TVal value) {
    if (cur_size_ < capacity_) {
      if (cache_.find(key) == cache_.end()) {
        kv_node_.push_front({key, value});
        cur_size_++;
        cache_.emplace(key, kv_node_.begin());
      } else {
        kv_node_.splice(kv_node_.begin(), kv_node_, cache_[key]);
        cache_[key] = kv_node_.begin();
        kv_node_.begin()->second = value;
      }
      return;
    }

    if (cache_.find(key) != cache_.end()) {
      kv_node_.splice(kv_node_.begin(), kv_node_, cache_[key]);
      cache_[key] = kv_node_.begin();
      kv_node_.begin()->second = value;
    }

    cache_.erase(kv_node_.back().first);
    kv_node_.splice(kv_node_.begin(), kv_node_, std::prev(kv_node_.end()));
    kv_node_.begin()->first = key;
    kv_node_.begin()->second = value;
    cache_.emplace(key, kv_node_.begin());
  }

  const std::list<std::pair<TKey, TVal>>& View() const { return kv_node_; }

  void Reset() {
    cur_size_ = 0;
    hit_count_ = 0;
    miss_count_ = 0;
    kv_node_.clear();
    cache_.clear();
  }
};

}  // namespace rv64_emulator::libs
