/******************************************************************************
 *
 *  Copyright 2020 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#pragma once

#include <functional>
#include <iterator>
#include <list>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>

#include <base/logging.h>

namespace bluetooth {

namespace common {

template <typename K, typename V>
class LruCache {
 public:
  using Node = std::pair<K, V>;
  /**
   * Constructor of the cache
   *
   * @param capacity maximum size of the cache
   * @param log_tag, keyword to put at the head of log.
   */
  LruCache(const size_t& capacity, const std::string& log_tag)
      : capacity_(capacity) {
    if (capacity_ == 0) {
      // don't allow invalid capacity
      LOG(FATAL) << log_tag << " unable to have 0 LRU Cache capacity";
    }
  }

  // delete copy constructor
  LruCache(LruCache const&) = delete;
  LruCache& operator=(LruCache const&) = delete;

  ~LruCache() { Clear(); }

  /**
   * Clear the cache
   */
  void Clear() {
    std::lock_guard<std::recursive_mutex> lock(lru_mutex_);
    lru_map_.clear();
    node_list_.clear();
  }

  /**
   * Same as Get, but return an iterator to the accessed element
   *
   * Modifying the returned iterator does not warm up the cache
   *
   * @param key
   * @return pointer to the underlying value to allow in-place modification
   * nullptr when not found, will be invalidated when the key is evicted
   */
  V* Find(const K& key) {
    std::lock_guard<std::recursive_mutex> lock(lru_mutex_);
    auto map_iterator = lru_map_.find(key);
    if (map_iterator == lru_map_.end()) {
      return nullptr;
    }
    node_list_.splice(node_list_.begin(), node_list_, map_iterator->second);
    return &(map_iterator->second->second);
  }

  /**
   * Get the value of a key, and move the key to the head of cache, if there is
   * one
   *
   * @param key
   * @param value, output parameter of value of the key
   * @return true if the cache has the key
   */
  bool Get(const K& key, V* value) {
    CHECK(value != nullptr);
    std::lock_guard<std::recursive_mutex> lock(lru_mutex_);
    auto value_ptr = Find(key);
    if (value_ptr == nullptr) {
      return false;
    }
    *value = *value_ptr;
    return true;
  }

  /**
   * Check if the cache has the input key, move the key to the head
   * if there is one
   *
   * @param key
   * @return true if the cache has the key
   */
  bool HasKey(const K& key) {
    std::lock_guard<std::recursive_mutex> lock(lru_mutex_);
    return Find(key) != nullptr;
  }

  /**
   * Put a key-value pair to the head of cache
   *
   * @param key
   * @param value
   * @return evicted node if tail value is popped, std::nullopt if no value
   * is popped. std::optional can be treated as a boolean as well
   */
  std::optional<Node> Put(const K& key, V value) {
    std::lock_guard<std::recursive_mutex> lock(lru_mutex_);
    auto value_ptr = Find(key);
    if (value_ptr != nullptr) {
      // hasKey() calls get(), therefore already move the node to the head
      *value_ptr = std::move(value);
      return std::nullopt;
    }

    // remove tail
    std::optional<Node> ret = std::nullopt;
    if (lru_map_.size() == capacity_) {
      lru_map_.erase(node_list_.back().first);
      ret = std::move(node_list_.back());
      node_list_.pop_back();
    }
    // insert to dummy next;
    node_list_.emplace_front(key, std::move(value));
    lru_map_.emplace(key, node_list_.begin());
    return ret;
  }

  /**
   * Delete a key from cache
   *
   * @param key
   * @return true if deleted successfully
   */
  bool Remove(const K& key) {
    std::lock_guard<std::recursive_mutex> lock(lru_mutex_);
    auto map_iterator = lru_map_.find(key);
    if (map_iterator == lru_map_.end()) {
      return false;
    }

    // remove from the list
    node_list_.erase(map_iterator->second);

    // delete key from map
    lru_map_.erase(map_iterator);

    return true;
  }

  /**
   * Return size of the cache
   *
   * @return size of the cache
   */
  int Size() const {
    std::lock_guard<std::recursive_mutex> lock(lru_mutex_);
    return lru_map_.size();
  }

 private:
  std::list<Node> node_list_;
  size_t capacity_;
  std::unordered_map<K, typename std::list<Node>::iterator> lru_map_;
  mutable std::recursive_mutex lru_mutex_;
};

}  // namespace common
}  // namespace bluetooth
