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

#define LOG_TAG "BpfUtils"

#include "bpf/BpfUtils.h"

#include <elf.h>
#include <inttypes.h>
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/pfkeyv2.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sstream>
#include <string>

#include <android-base/properties.h>
#include <android-base/unique_fd.h>
#include <log/log.h>
#include <processgroup/processgroup.h>

using android::base::unique_fd;

// The buffer size for the buffer that records program loading logs, needs to be large enough for
// the largest kernel program.

namespace android {
namespace bpf {

uint64_t getSocketCookie(int sockFd) {
    uint64_t sock_cookie;
    socklen_t cookie_len = sizeof(sock_cookie);
    int res = getsockopt(sockFd, SOL_SOCKET, SO_COOKIE, &sock_cookie, &cookie_len);
    if (res < 0) {
        res = -errno;
        ALOGE("Failed to get socket cookie: %s\n", strerror(errno));
        errno = -res;
        // 0 is an invalid cookie. See sock_gen_cookie.
        return NONEXISTENT_COOKIE;
    }
    return sock_cookie;
}

int synchronizeKernelRCU() {
    // This is a temporary hack for network stats map swap on devices running
    // 4.9 kernels. The kernel code of socket release on pf_key socket will
    // explicitly call synchronize_rcu() which is exactly what we need.
    int pfSocket = socket(AF_KEY, SOCK_RAW | SOCK_CLOEXEC, PF_KEY_V2);

    if (pfSocket < 0) {
        int ret = -errno;
        ALOGE("create PF_KEY socket failed: %s", strerror(errno));
        return ret;
    }

    // When closing socket, synchronize_rcu() gets called in sock_release().
    if (close(pfSocket)) {
        int ret = -errno;
        ALOGE("failed to close the PF_KEY socket: %s", strerror(errno));
        return ret;
    }
    return 0;
}

int setrlimitForTest() {
    // Set the memory rlimit for the test process if the default MEMLOCK rlimit is not enough.
    struct rlimit limit = {
            .rlim_cur = 1073741824,  // 1 GiB
            .rlim_max = 1073741824,  // 1 GiB
    };
    int res = setrlimit(RLIMIT_MEMLOCK, &limit);
    if (res) {
        ALOGE("Failed to set the default MEMLOCK rlimit: %s", strerror(errno));
    }
    return res;
}

#define KVER(a, b, c) ((a)*65536 + (b)*256 + (c))

unsigned kernelVersion() {
    struct utsname buf;
    int ret = uname(&buf);
    if (ret) return 0;

    unsigned kver_major;
    unsigned kver_minor;
    unsigned kver_sub;
    char dummy;
    ret = sscanf(buf.release, "%u.%u.%u%c", &kver_major, &kver_minor, &kver_sub, &dummy);
    // Check the device kernel version
    if (ret < 3) return 0;

    return KVER(kver_major, kver_minor, kver_sub);
}

std::string BpfLevelToString(BpfLevel bpfLevel) {
    switch (bpfLevel) {
        case BpfLevel::NONE:
            return "None [pre-4.9 or pre-P]";
        case BpfLevel::BASIC_4_9:
            return "Basic [4.9 P+]";
        case BpfLevel::EXTENDED_4_14:
            return "Extended [4.14]";
        case BpfLevel::EXTENDED_4_19:
            return "Extended [4.19]";
        case BpfLevel::EXTENDED_5_4:
            return "Extended [5.4+]";
            // No default statement. We want to see errors of the form:
            // "enumeration value 'BPF_LEVEL_xxx' not handled in switch [-Werror,-Wswitch]".
    }
}

static BpfLevel getUncachedBpfSupportLevel() {
    unsigned kver = kernelVersion();

    if (kver >= KVER(5, 4, 0)) return BpfLevel::EXTENDED_5_4;
    if (kver >= KVER(4, 19, 0)) return BpfLevel::EXTENDED_4_19;
    if (kver >= KVER(4, 14, 0)) return BpfLevel::EXTENDED_4_14;

    // Override for devices launched with O but now on a 4.9-P+ kernel.
    bool ebpf_supported = base::GetBoolProperty("ro.kernel.ebpf.supported", false);
    if (ebpf_supported) return BpfLevel::BASIC_4_9;

    uint64_t api_level = base::GetUintProperty<uint64_t>("ro.product.first_api_level", 0);
    if (api_level == 0) {
        ALOGE("Cannot determine initial API level of the device");
        api_level = base::GetUintProperty<uint64_t>("ro.build.version.sdk", 0);
    }

    // Check if the device is shipped originally with android P.
    if (api_level < MINIMUM_API_REQUIRED) return BpfLevel::NONE;

    if (kver >= KVER(4, 9, 0)) return BpfLevel::BASIC_4_9;

    return BpfLevel::NONE;
}

BpfLevel getBpfSupportLevel() {
    static BpfLevel cache = getUncachedBpfSupportLevel();
    return cache;
}

}  // namespace bpf
}  // namespace android
