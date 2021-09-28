/*
 * Copyright (C) 2016 The Android Open Source Project
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
 *
 */

#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_packet.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <poll.h>
#include <sched.h>
#include <sys/capability.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <gtest/gtest.h>

#include <android-base/unique_fd.h>

#define LOG_TAG "NetdTest"
#include "bpf/BpfMap.h"
#include "netdbpf/bpf_shared.h"

#include "OffloadUtils.h"

namespace android {
namespace net {

using base::unique_fd;

TEST(NetUtilsWrapperTest, TestFileCapabilities) {
    errno = 0;
    ASSERT_EQ(NULL, cap_get_file("/system/bin/netutils-wrapper-1.0"));
    ASSERT_EQ(ENODATA, errno);
}

TEST(NetdSELinuxTest, CheckProperMTULabels) {
    // Since we expect the egrep regexp to filter everything out,
    // we thus expect no matches and thus a return code of 1
    // NOLINTNEXTLINE(cert-env33-c)
    ASSERT_EQ(W_EXITCODE(1, 0), system("ls -Z /sys/class/net/*/mtu | egrep -q -v "
                                       "'^u:object_r:sysfs_net:s0 /sys/class/net/'"));
}

// Trivial thread function that simply immediately terminates successfully.
static int thread(void*) {
    return 0;
}

typedef int (*thread_t)(void*);

static void nsTest(int flags, bool success, thread_t newThread) {
    // We need a minimal stack, but not clear if it will grow up or down,
    // So allocate 2 pages and give a pointer to the middle.
    static char stack[PAGE_SIZE * 2];
    errno = 0;
    // VFORK: if thread is successfully created, then kernel will wait for it
    // to terminate before we resume -> hence static stack is safe to reuse.
    int tid = clone(newThread, &stack[PAGE_SIZE], flags | CLONE_VFORK, NULL);
    if (success) {
        ASSERT_EQ(errno, 0);
        ASSERT_GE(tid, 0);
    } else {
        ASSERT_EQ(errno, EINVAL);
        ASSERT_EQ(tid, -1);
    }
}

// Test kernel configuration option CONFIG_NAMESPACES=y
TEST(NetdNamespaceTest, CheckMountNamespaceSupport) {
    nsTest(CLONE_NEWNS, true, thread);
}

// Test kernel configuration option CONFIG_UTS_NS=y
TEST(NetdNamespaceTest, CheckUTSNamespaceSupport) {
    nsTest(CLONE_NEWUTS, true, thread);
}

// Test kernel configuration option CONFIG_NET_NS=y
TEST(NetdNamespaceTest, CheckNetworkNamespaceSupport) {
    nsTest(CLONE_NEWNET, true, thread);
}

// Test kernel configuration option CONFIG_USER_NS=n
TEST(NetdNamespaceTest, /*DISABLED_*/ CheckNoUserNamespaceSupport) {
    nsTest(CLONE_NEWUSER, false, thread);
}

// Test for all of the above
TEST(NetdNamespaceTest, CheckFullNamespaceSupport) {
    nsTest(CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWNET, true, thread);
}

// Test for presence of kernel patch:
//   ANDROID: net: bpf: permit redirect from ingress L3 to egress L2 devices at near max mtu
// on 4.14+ kernels.
TEST(NetdBpfTest, testBpfSkbChangeHeadAboveMTU) {
    SKIP_IF_EXTENDED_BPF_NOT_SUPPORTED;

    constexpr int mtu = 1500;

    errno = 0;

    // Amusingly can't use SIOC... on tun/tap fds.
    int rv = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    ASSERT_EQ(errno, 0);
    ASSERT_GE(rv, 3);
    unique_fd unixfd(rv);

    rv = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
    ASSERT_EQ(errno, 0);
    ASSERT_GE(rv, 3);
    unique_fd tun(rv);

    rv = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
    ASSERT_EQ(errno, 0);
    ASSERT_GE(rv, 3);
    unique_fd tap(rv);

    struct ifreq tun_ifr = {
            .ifr_flags = IFF_TUN | IFF_NO_PI,
            .ifr_name = "tun_bpftest",
    };

    struct ifreq tap_ifr = {
            .ifr_flags = IFF_TAP | IFF_NO_PI,
            .ifr_name = "tap_bpftest",
    };

    rv = ioctl(tun, TUNSETIFF, &tun_ifr);
    ASSERT_EQ(errno, 0);
    ASSERT_EQ(rv, 0);

    rv = ioctl(tap, TUNSETIFF, &tap_ifr);
    ASSERT_EQ(errno, 0);
    ASSERT_EQ(rv, 0);

    // prevents kernel from sending us spurious ipv6 packets
    rv = open("/proc/sys/net/ipv6/conf/tap_bpftest/disable_ipv6", O_WRONLY);
    ASSERT_EQ(errno, 0);
    ASSERT_GE(rv, 3);
    unique_fd f(rv);

    rv = write(f, "1\n", 2);
    ASSERT_EQ(errno, 0);
    ASSERT_EQ(rv, 2);

    rv = close(f.release());
    ASSERT_EQ(errno, 0);
    ASSERT_EQ(rv, 0);

    int tunif = if_nametoindex(tun_ifr.ifr_name);
    ASSERT_GE(tunif, 2);

    int tapif = if_nametoindex(tap_ifr.ifr_name);
    ASSERT_GE(tapif, 2);

    tun_ifr.ifr_mtu = mtu;
    rv = ioctl(unixfd, SIOCSIFMTU, &tun_ifr);
    ASSERT_EQ(errno, 0);
    ASSERT_EQ(rv, 0);

    tap_ifr.ifr_mtu = mtu;
    rv = ioctl(unixfd, SIOCSIFMTU, &tap_ifr);
    ASSERT_EQ(errno, 0);
    ASSERT_EQ(rv, 0);

    rv = ioctl(unixfd, SIOCGIFFLAGS, &tun_ifr);
    ASSERT_EQ(errno, 0);
    ASSERT_EQ(rv, 0);

    rv = ioctl(unixfd, SIOCGIFFLAGS, &tap_ifr);
    ASSERT_EQ(errno, 0);
    ASSERT_EQ(rv, 0);

    tun_ifr.ifr_flags |= IFF_UP | IFF_RUNNING;

    tap_ifr.ifr_flags |= IFF_UP | IFF_RUNNING;

    rv = ioctl(unixfd, SIOCSIFFLAGS, &tun_ifr);
    ASSERT_EQ(errno, 0);
    ASSERT_EQ(rv, 0);

    rv = ioctl(unixfd, SIOCSIFFLAGS, &tap_ifr);
    ASSERT_EQ(errno, 0);
    ASSERT_EQ(rv, 0);

    rv = tcQdiscAddDevClsact(tunif);
    ASSERT_EQ(rv, 0);

    int bpfFd = getTetherIngressProgFd(/* ethernet */ false);
    ASSERT_EQ(errno, 0);
    ASSERT_GE(bpfFd, 3);

    rv = tcFilterAddDevIngressTether(tunif, bpfFd, /* ethernet*/ false);
    ASSERT_EQ(rv, 0);

    bpf::BpfMap<TetherIngressKey, TetherIngressValue> bpfIngressMap;
    bpf::BpfMap<uint32_t, TetherStatsValue> bpfStatsMap;
    bpf::BpfMap<uint32_t, uint64_t> bpfLimitMap;

    rv = getTetherIngressMapFd();
    ASSERT_GE(rv, 3);
    bpfIngressMap.reset(rv);

    rv = getTetherStatsMapFd();
    ASSERT_GE(rv, 3);
    bpfStatsMap.reset(rv);

    rv = getTetherLimitMapFd();
    ASSERT_GE(rv, 3);
    bpfLimitMap.reset(rv);

    TetherIngressKey key = {
            .iif = static_cast<uint32_t>(tunif),
            //.neigh6 = ,
    };

    ethhdr hdr = {
            .h_proto = htons(ETH_P_IPV6),
    };

    TetherIngressValue value = {
            .oif = static_cast<uint32_t>(tapif),
            .macHeader = hdr,
            .pmtu = mtu,
    };

#define ASSERT_OK(status) ASSERT_TRUE((status).ok())

    ASSERT_OK(bpfIngressMap.writeValue(key, value, BPF_ANY));

    uint32_t k = tunif;
    TetherStatsValue stats = {};
    ASSERT_OK(bpfStatsMap.writeValue(k, stats, BPF_NOEXIST));

    uint64_t limit = ~0uLL;
    ASSERT_OK(bpfLimitMap.writeValue(k, limit, BPF_NOEXIST));

    // minimal 'acceptable' 40-byte hoplimit 255 IPv6 packet, src ip 2000::
    uint8_t pkt[mtu] = {
            0x60, 0, 0, 0, 0, 40, 0, 255, 0x20,
    };

    // Iterate over all packet sizes from minimal ipv6 packet to mtu.
    // Tethering ebpf program should forward the packet from tun to tap interface.
    // TUN is L3, TAP is L2, so it will add a 14 byte ethernet header.
    for (int pkt_size = 40; pkt_size <= mtu; ++pkt_size) {
        rv = write(tun, pkt, pkt_size);
        ASSERT_EQ(errno, 0);
        ASSERT_EQ(rv, pkt_size);

        struct pollfd p = {
                .fd = tap,
                .events = POLLIN,
        };

        rv = poll(&p, 1, 1000 /*milliseconds*/);
        if (rv == 0) {
            // we hit a timeout at this packet size, log it
            EXPECT_EQ(pkt_size, -1);
            // this particular packet size is where it fails without the oneline kernel fix
            if (pkt_size + ETH_HLEN == mtu + 1) EXPECT_EQ("detected missing kernel patch", "");
            break;
        }
        EXPECT_EQ(errno, 0);
        EXPECT_EQ(rv, 1);
        EXPECT_EQ(p.revents, POLLIN);

        // use a buffer 1 byte larger then what we expect so we don't simply get truncated down
        uint8_t buf[ETH_HLEN + mtu + 1];
        rv = read(tap, buf, sizeof(buf));
        EXPECT_EQ(errno, 0);
        EXPECT_EQ(rv, ETH_HLEN + pkt_size);
        errno = 0;
        if (rv < 0) break;
    }

    ASSERT_OK(bpfIngressMap.deleteValue(key));
    ASSERT_OK(bpfStatsMap.deleteValue(k));
    ASSERT_OK(bpfLimitMap.deleteValue(k));
}

}  // namespace net
}  // namespace android
