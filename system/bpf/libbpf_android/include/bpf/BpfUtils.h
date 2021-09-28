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

#ifndef BPF_BPFUTILS_H
#define BPF_BPFUTILS_H

#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/unistd.h>
#include <net/if.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include <string>

#include "android-base/unique_fd.h"

#define ptr_to_u64(x) ((uint64_t)(uintptr_t)(x))

namespace android {
namespace bpf {

enum class BpfLevel {
    // Devices shipped before P or kernel version is lower than 4.9 do not
    // have eBPF enabled.
    NONE,
    // Devices shipped in P with android 4.9 kernel only have the basic eBPF
    // functionalities such as xt_bpf and cgroup skb filter.
    BASIC_4_9,
    // For devices that have 4.14 kernel. It supports advanced features like
    // map_in_map and cgroup socket filter.
    EXTENDED_4_14,
    EXTENDED_4_19,
    EXTENDED_5_4,
};

constexpr const int OVERFLOW_COUNTERSET = 2;

constexpr const uint64_t NONEXISTENT_COOKIE = 0;

constexpr const int MINIMUM_API_REQUIRED = 28;

/* Note: bpf_attr is a union which might have a much larger size then the anonymous struct portion
 * of it that we are using.  The kernel's bpf() system call will perform a strict check to ensure
 * all unused portions are zero.  It will fail with E2BIG if we don't fully zero bpf_attr.
 */

inline int bpf(int cmd, const bpf_attr& attr) {
    return syscall(__NR_bpf, cmd, &attr, sizeof(attr));
}

inline int createMap(bpf_map_type map_type, uint32_t key_size, uint32_t value_size,
                     uint32_t max_entries, uint32_t map_flags) {
    return bpf(BPF_MAP_CREATE, {
                                       .map_type = map_type,
                                       .key_size = key_size,
                                       .value_size = value_size,
                                       .max_entries = max_entries,
                                       .map_flags = map_flags,
                               });
}

inline int writeToMapEntry(const base::unique_fd& map_fd, const void* key, const void* value,
                           uint64_t flags) {
    return bpf(BPF_MAP_UPDATE_ELEM, {
                                            .map_fd = static_cast<__u32>(map_fd.get()),
                                            .key = ptr_to_u64(key),
                                            .value = ptr_to_u64(value),
                                            .flags = flags,
                                    });
}

inline int findMapEntry(const base::unique_fd& map_fd, const void* key, void* value) {
    return bpf(BPF_MAP_LOOKUP_ELEM, {
                                            .map_fd = static_cast<__u32>(map_fd.get()),
                                            .key = ptr_to_u64(key),
                                            .value = ptr_to_u64(value),
                                    });
}

inline int deleteMapEntry(const base::unique_fd& map_fd, const void* key) {
    return bpf(BPF_MAP_DELETE_ELEM, {
                                            .map_fd = static_cast<__u32>(map_fd.get()),
                                            .key = ptr_to_u64(key),
                                    });
}

inline int getNextMapKey(const base::unique_fd& map_fd, const void* key, void* next_key) {
    return bpf(BPF_MAP_GET_NEXT_KEY, {
                                             .map_fd = static_cast<__u32>(map_fd.get()),
                                             .key = ptr_to_u64(key),
                                             .next_key = ptr_to_u64(next_key),
                                     });
}

inline int getFirstMapKey(const base::unique_fd& map_fd, void* firstKey) {
    return getNextMapKey(map_fd, NULL, firstKey);
}

inline int bpfFdPin(const base::unique_fd& map_fd, const char* pathname) {
    return bpf(BPF_OBJ_PIN, {
                                    .pathname = ptr_to_u64(pathname),
                                    .bpf_fd = static_cast<__u32>(map_fd.get()),
                            });
}

inline int bpfFdGet(const char* pathname, uint32_t flag) {
    return bpf(BPF_OBJ_GET, {
                                    .pathname = ptr_to_u64(pathname),
                                    .file_flags = flag,
                            });
}

inline int mapRetrieve(const char* pathname, uint32_t flag) {
    return bpfFdGet(pathname, flag);
}

inline int mapRetrieveRW(const char* pathname) {
    return mapRetrieve(pathname, 0);
}

inline int mapRetrieveRO(const char* pathname) {
    return mapRetrieve(pathname, BPF_F_RDONLY);
}

inline int mapRetrieveWO(const char* pathname) {
    return mapRetrieve(pathname, BPF_F_WRONLY);
}

inline int retrieveProgram(const char* pathname) {
    return bpfFdGet(pathname, BPF_F_RDONLY);
}

inline int attachProgram(bpf_attach_type type, const base::unique_fd& prog_fd,
                         const base::unique_fd& cg_fd) {
    return bpf(BPF_PROG_ATTACH, {
                                        .target_fd = static_cast<__u32>(cg_fd.get()),
                                        .attach_bpf_fd = static_cast<__u32>(prog_fd.get()),
                                        .attach_type = type,
                                });
}

inline int detachProgram(bpf_attach_type type, const base::unique_fd& cg_fd) {
    return bpf(BPF_PROG_DETACH, {
                                        .target_fd = static_cast<__u32>(cg_fd.get()),
                                        .attach_type = type,
                                });
}

uint64_t getSocketCookie(int sockFd);
int synchronizeKernelRCU();
int setrlimitForTest();
unsigned kernelVersion();
std::string BpfLevelToString(BpfLevel BpfLevel);
BpfLevel getBpfSupportLevel();

inline bool isBpfSupported() {
    return getBpfSupportLevel() != BpfLevel::NONE;
}

#define SKIP_IF_BPF_NOT_SUPPORTED                                                    \
    do {                                                                             \
        if (!android::bpf::isBpfSupported()) {                                       \
            GTEST_LOG_(INFO) << "This test is skipped since bpf is not available\n"; \
            return;                                                                  \
        }                                                                            \
    } while (0)

#define SKIP_IF_BPF_SUPPORTED                       \
    do {                                            \
        if (android::bpf::isBpfSupported()) return; \
    } while (0)

#define SKIP_IF_EXTENDED_BPF_NOT_SUPPORTED                                                \
    do {                                                                                  \
        if (android::bpf::getBpfSupportLevel() < android::bpf::BpfLevel::EXTENDED_4_14) { \
            GTEST_LOG_(INFO) << "This test is skipped since extended bpf feature"         \
                             << "not supported\n";                                        \
            return;                                                                       \
        }                                                                                 \
    } while (0)

}  // namespace bpf
}  // namespace android

#endif
