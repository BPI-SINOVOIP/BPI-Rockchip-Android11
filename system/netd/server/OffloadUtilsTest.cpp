/*
 * Copyright 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * OffloadUtilsTest.cpp - unit tests for OffloadUtils.cpp
 */

#include <gtest/gtest.h>

#include "OffloadUtils.h"

#include <linux/if_arp.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "bpf/BpfUtils.h"
#include "netdbpf/bpf_shared.h"

namespace android {
namespace net {

class OffloadUtilsTest : public ::testing::Test {
  public:
    void SetUp() {}
};

TEST_F(OffloadUtilsTest, HardwareAddressTypeOfNonExistingIf) {
    ASSERT_EQ(-ENODEV, hardwareAddressType("not_existing_if"));
}

TEST_F(OffloadUtilsTest, HardwareAddressTypeOfLoopback) {
    ASSERT_EQ(ARPHRD_LOOPBACK, hardwareAddressType("lo"));
}

// If wireless 'wlan0' interface exists it should be Ethernet.
TEST_F(OffloadUtilsTest, HardwareAddressTypeOfWireless) {
    int type = hardwareAddressType("wlan0");
    if (type == -ENODEV) return;

    ASSERT_EQ(ARPHRD_ETHER, type);
}

// If cellular 'rmnet_data0' interface exists it should
// *probably* not be Ethernet and instead be RawIp.
TEST_F(OffloadUtilsTest, HardwareAddressTypeOfCellular) {
    int type = hardwareAddressType("rmnet_data0");
    if (type == -ENODEV) return;

    ASSERT_NE(ARPHRD_ETHER, type);

    // ARPHRD_RAWIP is 530 on some pre-4.14 Qualcomm devices.
    if (type == 530) return;

    ASSERT_EQ(ARPHRD_RAWIP, type);
}

TEST_F(OffloadUtilsTest, IsEthernetOfNonExistingIf) {
    auto res = isEthernet("not_existing_if");
    ASSERT_FALSE(res.ok());
    ASSERT_EQ(ENODEV, res.error().code());
}

TEST_F(OffloadUtilsTest, IsEthernetOfLoopback) {
    auto res = isEthernet("lo");
    ASSERT_FALSE(res.ok());
    ASSERT_EQ(EAFNOSUPPORT, res.error().code());
}

// If wireless 'wlan0' interface exists it should be Ethernet.
// See also HardwareAddressTypeOfWireless.
TEST_F(OffloadUtilsTest, IsEthernetOfWireless) {
    auto res = isEthernet("wlan0");
    if (!res.ok() && res.error().code() == ENODEV) return;

    ASSERT_RESULT_OK(res);
    ASSERT_TRUE(res.value());
}

// If cellular 'rmnet_data0' interface exists it should
// *probably* not be Ethernet and instead be RawIp.
// See also HardwareAddressTypeOfCellular.
TEST_F(OffloadUtilsTest, IsEthernetOfCellular) {
    auto res = isEthernet("rmnet_data0");
    if (!res.ok() && res.error().code() == ENODEV) return;

    ASSERT_RESULT_OK(res);
    ASSERT_FALSE(res.value());
}

TEST_F(OffloadUtilsTest, GetClatEgressMapFd) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    int fd = getClatEgressMapFd();
    ASSERT_GE(fd, 3);  // 0,1,2 - stdin/out/err, thus fd >= 3
    EXPECT_EQ(FD_CLOEXEC, fcntl(fd, F_GETFD));
    close(fd);
}

TEST_F(OffloadUtilsTest, GetClatEgressRawIpProgFd) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    int fd = getClatEgressProgFd(RAWIP);
    ASSERT_GE(fd, 3);
    EXPECT_EQ(FD_CLOEXEC, fcntl(fd, F_GETFD));
    close(fd);
}

TEST_F(OffloadUtilsTest, GetClatEgressEtherProgFd) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    int fd = getClatEgressProgFd(ETHER);
    ASSERT_GE(fd, 3);
    EXPECT_EQ(FD_CLOEXEC, fcntl(fd, F_GETFD));
    close(fd);
}

TEST_F(OffloadUtilsTest, GetClatIngressMapFd) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    int fd = getClatIngressMapFd();
    ASSERT_GE(fd, 3);  // 0,1,2 - stdin/out/err, thus fd >= 3
    EXPECT_EQ(FD_CLOEXEC, fcntl(fd, F_GETFD));
    close(fd);
}

TEST_F(OffloadUtilsTest, GetClatIngressRawIpProgFd) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    int fd = getClatIngressProgFd(RAWIP);
    ASSERT_GE(fd, 3);
    EXPECT_EQ(FD_CLOEXEC, fcntl(fd, F_GETFD));
    close(fd);
}

TEST_F(OffloadUtilsTest, GetClatIngressEtherProgFd) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    int fd = getClatIngressProgFd(ETHER);
    ASSERT_GE(fd, 3);
    EXPECT_EQ(FD_CLOEXEC, fcntl(fd, F_GETFD));
    close(fd);
}

TEST_F(OffloadUtilsTest, GetTetherIngressMapFd) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    int fd = getTetherIngressMapFd();
    ASSERT_GE(fd, 3);  // 0,1,2 - stdin/out/err, thus fd >= 3
    EXPECT_EQ(FD_CLOEXEC, fcntl(fd, F_GETFD));
    close(fd);
}

TEST_F(OffloadUtilsTest, GetTetherIngressRawIpProgFd) {
    // Currently only implementing downstream direction offload.
    // RX Rawip -> TX Ether requires header adjustments and thus 4.14.
    SKIP_IF_EXTENDED_BPF_NOT_SUPPORTED;

    int fd = getTetherIngressProgFd(RAWIP);
    ASSERT_GE(fd, 3);
    EXPECT_EQ(FD_CLOEXEC, fcntl(fd, F_GETFD));
    close(fd);
}

TEST_F(OffloadUtilsTest, GetTetherIngressEtherProgFd) {
    // Currently only implementing downstream direction offload.
    // RX Ether -> TX Ether does not require header adjustments
    SKIP_IF_BPF_NOT_SUPPORTED;

    int fd = getTetherIngressProgFd(ETHER);
    ASSERT_GE(fd, 3);
    EXPECT_EQ(FD_CLOEXEC, fcntl(fd, F_GETFD));
    close(fd);
}

TEST_F(OffloadUtilsTest, GetTetherStatsMapFd) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    int fd = getTetherStatsMapFd();
    ASSERT_GE(fd, 3);  // 0,1,2 - stdin/out/err, thus fd >= 3
    EXPECT_EQ(FD_CLOEXEC, fcntl(fd, F_GETFD));
    close(fd);
}

TEST_F(OffloadUtilsTest, GetTetherLimitMapFd) {
    SKIP_IF_BPF_NOT_SUPPORTED;

    int fd = getTetherLimitMapFd();
    ASSERT_GE(fd, 3);  // 0,1,2 - stdin/out/err, thus fd >= 3
    EXPECT_EQ(FD_CLOEXEC, fcntl(fd, F_GETFD));
    close(fd);
}

// The SKIP_IF_BPF_NOT_SUPPORTED macro is effectively a check for 4.9+ kernel
// combined with a launched on P device.  Ie. it's a test for 4.9-P or better.

// NET_SCH_INGRESS is only enabled starting with 4.9-Q and as such we need
// a separate way to test for this...
int doKernelSupportsNetSchIngress(void) {
    // NOLINTNEXTLINE(cert-env33-c)
    return system("zcat /proc/config.gz | egrep -q '^CONFIG_NET_SCH_INGRESS=[my]$'");
}

// NET_CLS_BPF is only enabled starting with 4.9-Q...
int doKernelSupportsNetClsBpf(void) {
    // NOLINTNEXTLINE(cert-env33-c)
    return system("zcat /proc/config.gz | egrep -q '^CONFIG_NET_CLS_BPF=[my]$'");
}

// Make sure the above functions actually execute correctly rather than failing
// due to missing binary or execution failure...
TEST_F(OffloadUtilsTest, KernelSupportsNetFuncs) {
    // Make sure the file is present and readable and decompressable.
    // NOLINTNEXTLINE(cert-env33-c)
    ASSERT_EQ(W_EXITCODE(0, 0), system("zcat /proc/config.gz > /dev/null"));

    int v = doKernelSupportsNetSchIngress();
    int w = doKernelSupportsNetClsBpf();

    // They should always either return 0 (match) or 1 (no match),
    // anything else is some sort of exec/environment/etc failure.
    if (v != W_EXITCODE(1, 0)) ASSERT_EQ(v, W_EXITCODE(0, 0));
    if (w != W_EXITCODE(1, 0)) ASSERT_EQ(w, W_EXITCODE(0, 0));
}

// True iff CONFIG_NET_SCH_INGRESS is enabled in /proc/config.gz
bool kernelSupportsNetSchIngress(void) {
    return doKernelSupportsNetSchIngress() == W_EXITCODE(0, 0);
}

// True iff CONFIG_NET_CLS_BPF is enabled in /proc/config.gz
bool kernelSupportsNetClsBpf(void) {
    return doKernelSupportsNetClsBpf() == W_EXITCODE(0, 0);
}

// See Linux kernel source in include/net/flow.h
#define LOOPBACK_IFINDEX 1

TEST_F(OffloadUtilsTest, AttachReplaceDetachClsactLo) {
    // Technically does not depend on ebpf, but does depend on clsact,
    // and we do not really care if it works on pre-4.9-Q anyway.
    SKIP_IF_BPF_NOT_SUPPORTED;
    if (!kernelSupportsNetSchIngress()) return;

    // This attaches and detaches a configuration-less and thus no-op clsact
    // qdisc to loopback interface (and it takes fractions of a second)
    EXPECT_EQ(0, tcQdiscAddDevClsact(LOOPBACK_IFINDEX));
    EXPECT_EQ(0, tcQdiscReplaceDevClsact(LOOPBACK_IFINDEX));
    EXPECT_EQ(0, tcQdiscDelDevClsact(LOOPBACK_IFINDEX));
    EXPECT_EQ(-EINVAL, tcQdiscDelDevClsact(LOOPBACK_IFINDEX));
}

static void checkAttachDetachBpfFilterClsactLo(const bool ingress, const bool ethernet) {
    // This test requires kernel 4.9-Q or better
    SKIP_IF_BPF_NOT_SUPPORTED;
    if (!kernelSupportsNetSchIngress()) return;
    if (!kernelSupportsNetClsBpf()) return;

    const bool extended =
            (android::bpf::getBpfSupportLevel() >= android::bpf::BpfLevel::EXTENDED_4_14);
    // Older kernels return EINVAL instead of ENOENT due to lacking proper error propagation...
    const int errNOENT =
            (android::bpf::getBpfSupportLevel() >= android::bpf::BpfLevel::EXTENDED_4_19) ? ENOENT
                                                                                          : EINVAL;

    int clatBpfFd = ingress ? getClatIngressProgFd(ethernet) : getClatEgressProgFd(ethernet);
    ASSERT_GE(clatBpfFd, 3);

    int tetherBpfFd = -1;
    if (extended && ingress) {
        tetherBpfFd = getTetherIngressProgFd(ethernet);
        ASSERT_GE(tetherBpfFd, 3);
    }

    // This attaches and detaches a clsact plus ebpf program to loopback
    // interface, but it should not affect traffic by virtue of us not
    // actually populating the ebpf control map.
    // Furthermore: it only takes fractions of a second.
    EXPECT_EQ(-EINVAL, tcFilterDelDevIngressClatIpv6(LOOPBACK_IFINDEX));
    EXPECT_EQ(-EINVAL, tcFilterDelDevEgressClatIpv4(LOOPBACK_IFINDEX));
    EXPECT_EQ(0, tcQdiscAddDevClsact(LOOPBACK_IFINDEX));
    EXPECT_EQ(-errNOENT, tcFilterDelDevIngressClatIpv6(LOOPBACK_IFINDEX));
    EXPECT_EQ(-errNOENT, tcFilterDelDevEgressClatIpv4(LOOPBACK_IFINDEX));
    if (ingress) {
        EXPECT_EQ(0, tcFilterAddDevIngressClatIpv6(LOOPBACK_IFINDEX, clatBpfFd, ethernet));
        if (extended) {
            EXPECT_EQ(0, tcFilterAddDevIngressTether(LOOPBACK_IFINDEX, tetherBpfFd, ethernet));
            EXPECT_EQ(0, tcFilterDelDevIngressTether(LOOPBACK_IFINDEX));
        }
        EXPECT_EQ(0, tcFilterDelDevIngressClatIpv6(LOOPBACK_IFINDEX));
    } else {
        EXPECT_EQ(0, tcFilterAddDevEgressClatIpv4(LOOPBACK_IFINDEX, clatBpfFd, ethernet));
        EXPECT_EQ(0, tcFilterDelDevEgressClatIpv4(LOOPBACK_IFINDEX));
    }
    EXPECT_EQ(-errNOENT, tcFilterDelDevIngressClatIpv6(LOOPBACK_IFINDEX));
    EXPECT_EQ(-errNOENT, tcFilterDelDevEgressClatIpv4(LOOPBACK_IFINDEX));
    EXPECT_EQ(0, tcQdiscDelDevClsact(LOOPBACK_IFINDEX));
    EXPECT_EQ(-EINVAL, tcFilterDelDevIngressClatIpv6(LOOPBACK_IFINDEX));
    EXPECT_EQ(-EINVAL, tcFilterDelDevEgressClatIpv4(LOOPBACK_IFINDEX));

    if (tetherBpfFd != -1) close(tetherBpfFd);
    close(clatBpfFd);
}

TEST_F(OffloadUtilsTest, CheckAttachBpfFilterRawIpClsactEgressLo) {
    checkAttachDetachBpfFilterClsactLo(EGRESS, RAWIP);
}

TEST_F(OffloadUtilsTest, CheckAttachBpfFilterEthernetClsactEgressLo) {
    checkAttachDetachBpfFilterClsactLo(EGRESS, ETHER);
}

TEST_F(OffloadUtilsTest, CheckAttachBpfFilterRawIpClsactIngressLo) {
    checkAttachDetachBpfFilterClsactLo(INGRESS, RAWIP);
}

TEST_F(OffloadUtilsTest, CheckAttachBpfFilterEthernetClsactIngressLo) {
    checkAttachDetachBpfFilterClsactLo(INGRESS, ETHER);
}

}  // namespace net
}  // namespace android
