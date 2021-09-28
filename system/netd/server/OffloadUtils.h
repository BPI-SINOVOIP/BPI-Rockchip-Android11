/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <android-base/result.h>
#include <errno.h>
#include <linux/if_ether.h>
#include <linux/rtnetlink.h>

#include <string>

#include "bpf/BpfUtils.h"
#include "netdbpf/bpf_shared.h"

namespace android {
namespace net {

// For better code clarity - do not change values - used for booleans like
// with_ethernet_header or isEthernet.
constexpr bool RAWIP = false;
constexpr bool ETHER = true;

// For better code clarity when used for 'bool ingress' parameter.
constexpr bool EGRESS = false;
constexpr bool INGRESS = true;

// The priority of clat/tether hooks - smaller is higher priority.
constexpr uint16_t PRIO_CLAT = 1;
constexpr uint16_t PRIO_TETHER = 2;

// this returns an ARPHRD_* constant or a -errno
int hardwareAddressType(const std::string& interface);

base::Result<bool> isEthernet(const std::string& interface);

inline int getClatEgressMapFd(void) {
    const int fd = bpf::mapRetrieveRW(CLAT_EGRESS_MAP_PATH);
    return (fd == -1) ? -errno : fd;
}

inline int getClatEgressProgFd(bool with_ethernet_header) {
    const int fd = bpf::retrieveProgram(with_ethernet_header ? CLAT_EGRESS_PROG_ETHER_PATH
                                                             : CLAT_EGRESS_PROG_RAWIP_PATH);
    return (fd == -1) ? -errno : fd;
}

inline int getClatIngressMapFd(void) {
    const int fd = bpf::mapRetrieveRW(CLAT_INGRESS_MAP_PATH);
    return (fd == -1) ? -errno : fd;
}

inline int getClatIngressProgFd(bool with_ethernet_header) {
    const int fd = bpf::retrieveProgram(with_ethernet_header ? CLAT_INGRESS_PROG_ETHER_PATH
                                                             : CLAT_INGRESS_PROG_RAWIP_PATH);
    return (fd == -1) ? -errno : fd;
}

inline int getTetherIngressMapFd(void) {
    const int fd = bpf::mapRetrieveRW(TETHER_INGRESS_MAP_PATH);
    return (fd == -1) ? -errno : fd;
}

inline int getTetherIngressProgFd(bool with_ethernet_header) {
    const int fd = bpf::retrieveProgram(with_ethernet_header ? TETHER_INGRESS_PROG_ETHER_PATH
                                                             : TETHER_INGRESS_PROG_RAWIP_PATH);
    return (fd == -1) ? -errno : fd;
}

inline int getTetherStatsMapFd(void) {
    const int fd = bpf::mapRetrieveRW(TETHER_STATS_MAP_PATH);
    return (fd == -1) ? -errno : fd;
}

inline int getTetherLimitMapFd(void) {
    const int fd = bpf::mapRetrieveRW(TETHER_LIMIT_MAP_PATH);
    return (fd == -1) ? -errno : fd;
}

int doTcQdiscClsact(int ifIndex, uint16_t nlMsgType, uint16_t nlMsgFlags);

inline int tcQdiscAddDevClsact(int ifIndex) {
    return doTcQdiscClsact(ifIndex, RTM_NEWQDISC, NLM_F_EXCL | NLM_F_CREATE);
}

inline int tcQdiscReplaceDevClsact(int ifIndex) {
    return doTcQdiscClsact(ifIndex, RTM_NEWQDISC, NLM_F_CREATE | NLM_F_REPLACE);
}

inline int tcQdiscDelDevClsact(int ifIndex) {
    return doTcQdiscClsact(ifIndex, RTM_DELQDISC, 0);
}

// tc filter add dev .. in/egress prio 1 protocol ipv6/ip bpf object-pinned /sys/fs/bpf/...
// direct-action
int tcFilterAddDevBpf(int ifIndex, bool ingress, uint16_t prio, uint16_t proto, int bpfFd,
                      bool ethernet);

// tc filter add dev .. ingress prio 1 protocol ipv6 bpf object-pinned /sys/fs/bpf/... direct-action
inline int tcFilterAddDevIngressClatIpv6(int ifIndex, int bpfFd, bool ethernet) {
    return tcFilterAddDevBpf(ifIndex, INGRESS, PRIO_CLAT, ETH_P_IPV6, bpfFd, ethernet);
}

// tc filter add dev .. egress prio 1 protocol ip bpf object-pinned /sys/fs/bpf/... direct-action
inline int tcFilterAddDevEgressClatIpv4(int ifIndex, int bpfFd, bool ethernet) {
    return tcFilterAddDevBpf(ifIndex, EGRESS, PRIO_CLAT, ETH_P_IP, bpfFd, ethernet);
}

// tc filter add dev .. ingress prio 2 protocol ipv6 bpf object-pinned /sys/fs/bpf/... direct-action
inline int tcFilterAddDevIngressTether(int ifIndex, int bpfFd, bool ethernet) {
    return tcFilterAddDevBpf(ifIndex, INGRESS, PRIO_TETHER, ETH_P_IPV6, bpfFd, ethernet);
}

// tc filter del dev .. in/egress prio .. protocol ..
int tcFilterDelDev(int ifIndex, bool ingress, uint16_t prio, uint16_t proto);

// tc filter del dev .. ingress prio 1 protocol ipv6
inline int tcFilterDelDevIngressClatIpv6(int ifIndex) {
    return tcFilterDelDev(ifIndex, INGRESS, PRIO_CLAT, ETH_P_IPV6);
}

// tc filter del dev .. egress prio 1 protocol ip
inline int tcFilterDelDevEgressClatIpv4(int ifIndex) {
    return tcFilterDelDev(ifIndex, EGRESS, PRIO_CLAT, ETH_P_IP);
}

// tc filter del dev .. ingress prio 2 protocol ipv6
inline int tcFilterDelDevIngressTether(int ifIndex) {
    return tcFilterDelDev(ifIndex, INGRESS, PRIO_TETHER, ETH_P_IPV6);
}

}  // namespace net
}  // namespace android
