/*
 * Copyright (C) 2017 The Android Open Source Project
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

#pragma once

#include <stdint.h>
#include <type_traits>
#include <vector>

#include <linux/netlink.h>

class NetlinkMessage {
public:
    NetlinkMessage() = default;
    NetlinkMessage(uint16_t type, uint32_t sequence);
    NetlinkMessage(const char* data, size_t size);

    template<typename T,
             typename = std::enable_if_t<std::is_pod<T>::value>>
    bool getAttribute(int attributeId, T* value) const {
        return getAttribute(attributeId, value, sizeof(T));
    }

    uint16_t type() const;
    uint32_t sequence() const;

    nlmsghdr* header() {
        return reinterpret_cast<nlmsghdr*>(mData.data());
    }
    const nlmsghdr* header() const {
        return reinterpret_cast<const nlmsghdr*>(mData.data());
    }

    template<typename T>
    T* payload() {
        return reinterpret_cast<T*>(NLMSG_DATA(header()));
    }
    template<typename T>
    const T* payload() const {
        return reinterpret_cast<T*>(NLMSG_DATA(header()));
    }

    const uint8_t* data() const { return mData.data(); }
    size_t size() const { return mData.size(); }
private:
    NetlinkMessage(const NetlinkMessage&) = delete;
    NetlinkMessage& operator=(const NetlinkMessage&) = delete;

    bool getAttribute(int attributeId, void* data, size_t size) const;

    std::vector<uint8_t> mData;
};

