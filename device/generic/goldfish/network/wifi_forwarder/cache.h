/*
 * Copyright 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <inttypes.h>
#include "log.h"

#include <chrono>
#include <unordered_map>

// Default amount of time until a cache entry expires
static constexpr auto kDefaultCacheTimeout = std::chrono::seconds(30);

template<typename Key,
         typename Value,
         typename Timestamp = std::chrono::steady_clock::time_point>
class Cache {
public:
    using TimedValue = std::pair<Timestamp, Value>;

    using MapType = std::unordered_map<Key, TimedValue>;
    using key_type = typename MapType::key_type;
    using mapped_type = Value;

    class ConstIterator {
    public:
        class IterPair {
        public:
            IterPair(const Key& key, const Value& value) 
                : first(key), second(value) {
            }

            const IterPair* operator->() const { return this; }
            const Key& first;
            const Value& second;
        private:
        };

        ConstIterator(typename MapType::const_iterator current)
            : mCurrent(current) { }

        IterPair operator->() const {
            return IterPair(mCurrent->first, mCurrent->second.second);
        }

        IterPair operator*() const {
            return IterPair(mCurrent->first, mCurrent->second.second);
        }

        bool operator==(const ConstIterator& other) const {
            return mCurrent == other.mCurrent;
        }

        bool operator!=(const ConstIterator& other) const {
            return mCurrent != other.mCurrent;
        }

        typename MapType::const_iterator internal() const { return mCurrent; }

    private:
        typename MapType::const_iterator mCurrent;
    };
    class Iterator {
    public:
        class IterPair {
        public:
            IterPair(const Key& key, Value& value) : first(key), second(value) { }

            IterPair* operator->() { return this; }
            const Key& first;
            Value& second;
        private:
        };

        Iterator(typename MapType::iterator current) : mCurrent(current) { }

        IterPair operator->() {
            return IterPair(mCurrent->first, mCurrent->second.second);
        }

        IterPair operator*() {
            return IterPair(mCurrent->first, mCurrent->second.second);
        }

        bool operator==(const Iterator& other) const {
            return mCurrent == other.mCurrent;
        }

        bool operator!=(const Iterator& other) const {
            return mCurrent != other.mCurrent;
        }

        typename MapType::iterator internal() { return mCurrent; }

    private:
        typename MapType::iterator mCurrent;
    };

    using iterator = Iterator;
    using const_iterator = ConstIterator;
    using insert_return_type = std::pair<const_iterator, bool>;

    Cache(std::chrono::milliseconds timeout = kDefaultCacheTimeout)
        : mTimeout(timeout) {
    }

    template<typename M>
    insert_return_type insert_or_assign(const key_type& key, M&& value) {
        std::pair<typename MapType::iterator,bool> inserted =
            mMap.insert_or_assign(key, TimedValue(mCurrentTime,
                                                  std::move(value)));
        return insert_return_type(inserted.first, inserted.second);
    }

    mapped_type& operator[](const key_type& key) {
        TimedValue& v = mMap[key];
        v.first = mCurrentTime;
        return v.second;
    }

    iterator find(const key_type& key) {
        return iterator(mMap.find(key));
    }

    const_iterator find(const key_type& key) const {
        return const_iterator(mMap.find(key));
    }

    iterator erase(const_iterator pos) {
        return iterator(mMap.erase(pos.internal()));
    }

    iterator erase(iterator pos) {
        return iterator(mMap.erase(pos.internal()));
    }

    size_t erase(const key_type& key) {
        return mMap.erase(key);
    }

    iterator begin() {
        return iterator(mMap.begin());
    }

    iterator end() {
        return iterator(mMap.end());
    }

    const_iterator begin() const {
        return const_iterator(mMap.begin());
    }

    const_iterator end() const {
        return const_iterator(mMap.end());
    }

    void setCurrentTime(Timestamp currentTime) {
        mCurrentTime = currentTime;
    }

    void expireEntries() {
        for (auto it = mMap.begin(); it != mMap.end(); ) {
            const Timestamp timestamp = it->second.first;
            if (mCurrentTime > timestamp &&
                (mCurrentTime - timestamp) > mTimeout) {
                // This entry has expired, remove it
                it = mMap.erase(it);
            } else {
                ++it;
            }
        }
    }
private:
    const std::chrono::milliseconds mTimeout;
    Timestamp mCurrentTime;
    MapType mMap;
};

