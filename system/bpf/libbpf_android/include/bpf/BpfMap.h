/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BPF_BPFMAP_H
#define BPF_BPFMAP_H

#include <linux/bpf.h>

#include <android-base/result.h>
#include <android-base/stringprintf.h>
#include <android-base/unique_fd.h>
#include <utils/Log.h>
#include "bpf/BpfUtils.h"

namespace android {
namespace bpf {

// This is a class wrapper for eBPF maps. The eBPF map is a special in-kernel
// data structure that stores data in <Key, Value> pairs. It can be read/write
// from userspace by passing syscalls with the map file descriptor. This class
// is used to generalize the procedure of interacting with eBPF maps and hide
// the implementation detail from other process. Besides the basic syscalls
// wrapper, it also provides some useful helper functions as well as an iterator
// nested class to iterate the map more easily.
//
// NOTE: A kernel eBPF map may be accessed by both kernel and userspace
// processes at the same time. Or if the map is pinned as a virtual file, it can
// be obtained by multiple eBPF map class object and accessed concurrently.
// Though the map class object and the underlying kernel map are thread safe, it
// is not safe to iterate over a map while another thread or process is deleting
// from it. In this case the iteration can return duplicate entries.
template <class Key, class Value>
class BpfMap {
  public:
    BpfMap<Key, Value>() {};

  protected:
    // flag must be within BPF_OBJ_FLAG_MASK, ie. 0, BPF_F_RDONLY, BPF_F_WRONLY
    BpfMap<Key, Value>(const char* pathname, uint32_t flags) {
        int map_fd = mapRetrieve(pathname, flags);
        if (map_fd >= 0) mMapFd.reset(map_fd);
    }

  public:
    explicit BpfMap<Key, Value>(const char* pathname) : BpfMap<Key, Value>(pathname, 0) {}

    BpfMap<Key, Value>(bpf_map_type map_type, uint32_t max_entries, uint32_t map_flags = 0) {
        int map_fd = createMap(map_type, sizeof(Key), sizeof(Value), max_entries, map_flags);
        if (map_fd >= 0) mMapFd.reset(map_fd);
    }

    base::Result<Key> getFirstKey() const {
        Key firstKey;
        if (getFirstMapKey(mMapFd, &firstKey)) {
            return ErrnoErrorf("Get firstKey map {} failed", mMapFd.get());
        }
        return firstKey;
    }

    base::Result<Key> getNextKey(const Key& key) const {
        Key nextKey;
        if (getNextMapKey(mMapFd, &key, &nextKey)) {
            return ErrnoErrorf("Get next key of map {} failed", mMapFd.get());
        }
        return nextKey;
    }

    base::Result<void> writeValue(const Key& key, const Value& value, uint64_t flags) {
        if (writeToMapEntry(mMapFd, &key, &value, flags)) {
            return ErrnoErrorf("Write to map {} failed", mMapFd.get());
        }
        return {};
    }

    base::Result<Value> readValue(const Key key) const {
        Value value;
        if (findMapEntry(mMapFd, &key, &value)) {
            return ErrnoErrorf("Read value of map {} failed", mMapFd.get());
        }
        return value;
    }

    base::Result<void> deleteValue(const Key& key) {
        if (deleteMapEntry(mMapFd, &key)) {
            return ErrnoErrorf("Delete entry from map {} failed", mMapFd.get());
        }
        return {};
    }

    // Function that tries to get map from a pinned path.
    base::Result<void> init(const char* path);

    // Iterate through the map and handle each key retrieved based on the filter
    // without modification of map content.
    base::Result<void> iterate(
            const std::function<base::Result<void>(const Key& key, const BpfMap<Key, Value>& map)>&
                    filter) const;

    // Iterate through the map and get each <key, value> pair, handle each <key,
    // value> pair based on the filter without modification of map content.
    base::Result<void> iterateWithValue(
            const std::function<base::Result<void>(const Key& key, const Value& value,
                                                   const BpfMap<Key, Value>& map)>& filter) const;

    // Iterate through the map and handle each key retrieved based on the filter
    base::Result<void> iterate(
            const std::function<base::Result<void>(const Key& key, BpfMap<Key, Value>& map)>&
                    filter);

    // Iterate through the map and get each <key, value> pair, handle each <key,
    // value> pair based on the filter.
    base::Result<void> iterateWithValue(
            const std::function<base::Result<void>(const Key& key, const Value& value,
                                                   BpfMap<Key, Value>& map)>& filter);

    const base::unique_fd& getMap() const { return mMapFd; };

    // Copy assignment operator
    BpfMap<Key, Value>& operator=(const BpfMap<Key, Value>& other) {
        if (this != &other) mMapFd.reset(fcntl(other.mMapFd.get(), F_DUPFD_CLOEXEC, 0));
        return *this;
    }

    // Move constructor
    void operator=(BpfMap<Key, Value>&& other) noexcept {
        mMapFd = std::move(other.mMapFd);
        other.reset(-1);
    }

    void reset(base::unique_fd fd) = delete;

    void reset(int fd) { mMapFd.reset(fd); }

    bool isValid() const { return mMapFd != -1; }

    base::Result<void> clear() {
        while (true) {
            auto key = getFirstKey();
            if (!key.ok()) {
                if (key.error().code() == ENOENT) return {};  // empty: success
                return key.error();                           // Anything else is an error
            }
            auto res = deleteValue(key.value());
            if (!res.ok()) {
                // Someone else could have deleted the key, so ignore ENOENT
                if (res.error().code() == ENOENT) continue;
                ALOGE("Failed to delete data %s", strerror(res.error().code()));
                return res.error();
            }
        }
    }

    base::Result<bool> isEmpty() const {
        auto key = getFirstKey();
        if (!key.ok()) {
            // Return error code ENOENT means the map is empty
            if (key.error().code() == ENOENT) return true;
            return key.error();
        }
        return false;
    }

  private:
    base::unique_fd mMapFd;
};

template <class Key, class Value>
base::Result<void> BpfMap<Key, Value>::init(const char* path) {
    mMapFd = base::unique_fd(mapRetrieveRW(path));
    if (mMapFd == -1) {
        return ErrnoErrorf("Pinned map not accessible or does not exist: ({})", path);
    }
    return {};
}

template <class Key, class Value>
base::Result<void> BpfMap<Key, Value>::iterate(
        const std::function<base::Result<void>(const Key& key, const BpfMap<Key, Value>& map)>&
                filter) const {
    base::Result<Key> curKey = getFirstKey();
    while (curKey.ok()) {
        const base::Result<Key>& nextKey = getNextKey(curKey.value());
        base::Result<void> status = filter(curKey.value(), *this);
        if (!status.ok()) return status;
        curKey = nextKey;
    }
    if (curKey.error().code() == ENOENT) return {};
    return curKey.error();
}

template <class Key, class Value>
base::Result<void> BpfMap<Key, Value>::iterateWithValue(
        const std::function<base::Result<void>(const Key& key, const Value& value,
                                               const BpfMap<Key, Value>& map)>& filter) const {
    base::Result<Key> curKey = getFirstKey();
    while (curKey.ok()) {
        const base::Result<Key>& nextKey = getNextKey(curKey.value());
        base::Result<Value> curValue = readValue(curKey.value());
        if (!curValue.ok()) return curValue.error();
        base::Result<void> status = filter(curKey.value(), curValue.value(), *this);
        if (!status.ok()) return status;
        curKey = nextKey;
    }
    if (curKey.error().code() == ENOENT) return {};
    return curKey.error();
}

template <class Key, class Value>
base::Result<void> BpfMap<Key, Value>::iterate(
        const std::function<base::Result<void>(const Key& key, BpfMap<Key, Value>& map)>& filter) {
    base::Result<Key> curKey = getFirstKey();
    while (curKey.ok()) {
        const base::Result<Key>& nextKey = getNextKey(curKey.value());
        base::Result<void> status = filter(curKey.value(), *this);
        if (!status.ok()) return status;
        curKey = nextKey;
    }
    if (curKey.error().code() == ENOENT) return {};
    return curKey.error();
}

template <class Key, class Value>
base::Result<void> BpfMap<Key, Value>::iterateWithValue(
        const std::function<base::Result<void>(const Key& key, const Value& value,
                                               BpfMap<Key, Value>& map)>& filter) {
    base::Result<Key> curKey = getFirstKey();
    while (curKey.ok()) {
        const base::Result<Key>& nextKey = getNextKey(curKey.value());
        base::Result<Value> curValue = readValue(curKey.value());
        if (!curValue.ok()) return curValue.error();
        base::Result<void> status = filter(curKey.value(), curValue.value(), *this);
        if (!status.ok()) return status;
        curKey = nextKey;
    }
    if (curKey.error().code() == ENOENT) return {};
    return curKey.error();
}

template <class Key, class Value>
class BpfMapRO : public BpfMap<Key, Value> {
  public:
    explicit BpfMapRO<Key, Value>(const char* pathname)
        : BpfMap<Key, Value>(pathname, BPF_F_RDONLY) {}
};

}  // namespace bpf
}  // namespace android

#endif
