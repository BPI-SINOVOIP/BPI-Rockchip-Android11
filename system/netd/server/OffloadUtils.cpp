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

#include "OffloadUtils.h"

#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/netlink.h>
#include <linux/pkt_cls.h>
#include <linux/pkt_sched.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define LOG_TAG "OffloadUtils"
#include <log/log.h>

#include "NetlinkCommands.h"
#include "android-base/unique_fd.h"

namespace android {
namespace net {

using std::max;

int hardwareAddressType(const std::string& interface) {
    base::unique_fd ufd(socket(AF_INET6, SOCK_DGRAM | SOCK_CLOEXEC, 0));

    if (ufd < 0) {
        const int err = errno;
        ALOGE("socket(AF_INET6, SOCK_DGRAM | SOCK_CLOEXEC, 0)");
        return -err;
    };

    struct ifreq ifr = {};
    // We use strncpy() instead of strlcpy() since kernel has to be able
    // to handle non-zero terminated junk passed in by userspace anyway,
    // and this way too long interface names (more than IFNAMSIZ-1 = 15
    // characters plus terminating NULL) will not get truncated to 15
    // characters and zero-terminated and thus potentially erroneously
    // match a truncated interface if one were to exist.
    strncpy(ifr.ifr_name, interface.c_str(), sizeof(ifr.ifr_name));

    if (ioctl(ufd, SIOCGIFHWADDR, &ifr, sizeof(ifr))) return -errno;

    return ifr.ifr_hwaddr.sa_family;
}

base::Result<bool> isEthernet(const std::string& interface) {
    int rv = hardwareAddressType(interface);
    if (rv < 0) {
        errno = -rv;
        return ErrnoErrorf("Get hardware address type of interface {} failed", interface);
    }

    switch (rv) {
        case ARPHRD_ETHER:
            return true;
        case ARPHRD_NONE:
        case ARPHRD_RAWIP:  // in Linux 4.14+ rmnet support was upstreamed and this is 519
        case 530:           // this is ARPHRD_RAWIP on some Android 4.9 kernels with rmnet
            return false;
        default:
            errno = EAFNOSUPPORT;  // Address family not supported
            return ErrnoErrorf("Unknown hardware address type {} on interface {}", rv, interface);
    }
}

// TODO: use //system/netd/server/NetlinkCommands.cpp:openNetlinkSocket(protocol)
// and //system/netd/server/SockDiag.cpp:checkError(fd)
static int sendAndProcessNetlinkResponse(const void* req, int len) {
    base::unique_fd fd(socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE));
    if (fd == -1) {
        const int err = errno;
        ALOGE("socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE)");
        return -err;
    }

    static constexpr int on = 1;
    int rv = setsockopt(fd, SOL_NETLINK, NETLINK_CAP_ACK, &on, sizeof(on));
    if (rv) ALOGE("setsockopt(fd, SOL_NETLINK, NETLINK_CAP_ACK, %d)", on);

    // this is needed to get sane strace netlink parsing, it allocates the pid
    rv = bind(fd, (const struct sockaddr*)&KERNEL_NLADDR, sizeof(KERNEL_NLADDR));
    if (rv) {
        const int err = errno;
        ALOGE("bind(fd, {AF_NETLINK, 0, 0})");
        return -err;
    }

    // we do not want to receive messages from anyone besides the kernel
    rv = connect(fd, (const struct sockaddr*)&KERNEL_NLADDR, sizeof(KERNEL_NLADDR));
    if (rv) {
        const int err = errno;
        ALOGE("connect(fd, {AF_NETLINK, 0, 0})");
        return -err;
    }

    rv = send(fd, req, len, 0);
    if (rv == -1) return -errno;
    if (rv != len) return -EMSGSIZE;

    struct {
        nlmsghdr h;
        nlmsgerr e;
        char buf[256];
    } resp = {};

    rv = recv(fd, &resp, sizeof(resp), MSG_TRUNC);

    if (rv == -1) {
        const int err = errno;
        ALOGE("recv() failed");
        return -err;
    }

    if (rv < (int)NLMSG_SPACE(sizeof(struct nlmsgerr))) {
        ALOGE("recv() returned short packet: %d", rv);
        return -EMSGSIZE;
    }

    if (resp.h.nlmsg_len != (unsigned)rv) {
        ALOGE("recv() returned invalid header length: %d != %d", resp.h.nlmsg_len, rv);
        return -EBADMSG;
    }

    if (resp.h.nlmsg_type != NLMSG_ERROR) {
        ALOGE("recv() did not return NLMSG_ERROR message: %d", resp.h.nlmsg_type);
        return -EBADMSG;
    }

    return resp.e.error;  // returns 0 on success
}

// ADD:     nlMsgType=RTM_NEWQDISC nlMsgFlags=NLM_F_EXCL|NLM_F_CREATE
// REPLACE: nlMsgType=RTM_NEWQDISC nlMsgFlags=NLM_F_CREATE|NLM_F_REPLACE
// DEL:     nlMsgType=RTM_DELQDISC nlMsgFlags=0
int doTcQdiscClsact(int ifIndex, uint16_t nlMsgType, uint16_t nlMsgFlags) {
    // This is the name of the qdisc we are attaching.
    // Some hoop jumping to make this compile time constant with known size,
    // so that the structure declaration is well defined at compile time.
#define CLSACT "clsact"
    // sizeof() includes the terminating NULL
    static constexpr size_t ASCIIZ_LEN_CLSACT = sizeof(CLSACT);

    const struct {
        nlmsghdr n;
        tcmsg t;
        struct {
            nlattr attr;
            char str[NLMSG_ALIGN(ASCIIZ_LEN_CLSACT)];
        } kind;
    } req = {
            .n =
                    {
                            .nlmsg_len = sizeof(req),
                            .nlmsg_type = nlMsgType,
                            .nlmsg_flags = static_cast<__u16>(NETLINK_REQUEST_FLAGS | nlMsgFlags),
                    },
            .t =
                    {
                            .tcm_family = AF_UNSPEC,
                            .tcm_ifindex = ifIndex,
                            .tcm_handle = TC_H_MAKE(TC_H_CLSACT, 0),
                            .tcm_parent = TC_H_CLSACT,
                    },
            .kind =
                    {
                            .attr =
                                    {
                                            .nla_len = NLA_HDRLEN + ASCIIZ_LEN_CLSACT,
                                            .nla_type = TCA_KIND,
                                    },
                            .str = CLSACT,
                    },
    };
#undef CLSACT

    return sendAndProcessNetlinkResponse(&req, sizeof(req));
}

// tc filter add dev .. in/egress prio 1 protocol ipv6/ip bpf object-pinned /sys/fs/bpf/...
// direct-action
int tcFilterAddDevBpf(int ifIndex, bool ingress, uint16_t prio, uint16_t proto, int bpfFd,
                      bool ethernet) {
    // This is the name of the filter we're attaching (ie. this is the 'bpf'
    // packet classifier enabled by kernel config option CONFIG_NET_CLS_BPF.
    //
    // We go through some hoops in order to make this compile time constants
    // so that we can define the struct further down the function with the
    // field for this sized correctly already during the build.
#define BPF "bpf"
    // sizeof() includes the terminating NULL
    static constexpr size_t ASCIIZ_LEN_BPF = sizeof(BPF);

    // This is to replicate program name suffix used by 'tc' Linux cli
    // when it attaches programs.
#define FSOBJ_SUFFIX ":[*fsobj]"

    // This macro expands (from header files) to:
    //   prog_clatd_schedcls_ingress_clat_rawip:[*fsobj]
    // and is the name of the pinned ingress ebpf program for ARPHRD_RAWIP interfaces.
    // (also compatible with anything that has 0 size L2 header)
    static constexpr char name_clat_rx_rawip[] = CLAT_INGRESS_PROG_RAWIP_NAME FSOBJ_SUFFIX;

    // This macro expands (from header files) to:
    //   prog_clatd_schedcls_ingress_clat_ether:[*fsobj]
    // and is the name of the pinned ingress ebpf program for ARPHRD_ETHER interfaces.
    // (also compatible with anything that has standard ethernet header)
    static constexpr char name_clat_rx_ether[] = CLAT_INGRESS_PROG_ETHER_NAME FSOBJ_SUFFIX;

    // This macro expands (from header files) to:
    //   prog_clatd_schedcls_egress_clat_rawip:[*fsobj]
    // and is the name of the pinned egress ebpf program for ARPHRD_RAWIP interfaces.
    // (also compatible with anything that has 0 size L2 header)
    static constexpr char name_clat_tx_rawip[] = CLAT_EGRESS_PROG_RAWIP_NAME FSOBJ_SUFFIX;

    // This macro expands (from header files) to:
    //   prog_clatd_schedcls_egress_clat_ether:[*fsobj]
    // and is the name of the pinned egress ebpf program for ARPHRD_ETHER interfaces.
    // (also compatible with anything that has standard ethernet header)
    static constexpr char name_clat_tx_ether[] = CLAT_EGRESS_PROG_ETHER_NAME FSOBJ_SUFFIX;

    // This macro expands (from header files) to:
    //   prog_offload_schedcls_ingress_tether_rawip:[*fsobj]
    // and is the name of the pinned ingress ebpf program for ARPHRD_RAWIP interfaces.
    // (also compatible with anything that has 0 size L2 header)
    static constexpr char name_tether_rawip[] = TETHER_INGRESS_PROG_RAWIP_NAME FSOBJ_SUFFIX;

    // This macro expands (from header files) to:
    //   prog_offload_schedcls_ingress_tether_ether:[*fsobj]
    // and is the name of the pinned ingress ebpf program for ARPHRD_ETHER interfaces.
    // (also compatible with anything that has standard ethernet header)
    static constexpr char name_tether_ether[] = TETHER_INGRESS_PROG_ETHER_NAME FSOBJ_SUFFIX;

#undef FSOBJ_SUFFIX

    // The actual name we'll use is determined at run time via 'ethernet' and 'ingress'
    // booleans.  We need to compile time allocate enough space in the struct
    // hence this macro magic to make sure we have enough space for either
    // possibility.  In practice some of these are actually the same size.
    static constexpr size_t ASCIIZ_MAXLEN_NAME = max({
            sizeof(name_clat_rx_rawip),
            sizeof(name_clat_rx_ether),
            sizeof(name_clat_tx_rawip),
            sizeof(name_clat_tx_ether),
            sizeof(name_tether_rawip),
            sizeof(name_tether_ether),
    });

    // These are not compile time constants: 'name' is used in strncpy below
    const char* const name_clat_rx = ethernet ? name_clat_rx_ether : name_clat_rx_rawip;
    const char* const name_clat_tx = ethernet ? name_clat_tx_ether : name_clat_tx_rawip;
    const char* const name_clat = ingress ? name_clat_rx : name_clat_tx;
    const char* const name_tether = ethernet ? name_tether_ether : name_tether_rawip;
    const char* const name = (prio == PRIO_TETHER) ? name_tether : name_clat;

    struct {
        nlmsghdr n;
        tcmsg t;
        struct {
            nlattr attr;
            char str[NLMSG_ALIGN(ASCIIZ_LEN_BPF)];
        } kind;
        struct {
            nlattr attr;
            struct {
                nlattr attr;
                __u32 u32;
            } fd;
            struct {
                nlattr attr;
                char str[NLMSG_ALIGN(ASCIIZ_MAXLEN_NAME)];
            } name;
            struct {
                nlattr attr;
                __u32 u32;
            } flags;
        } options;
    } req = {
            .n =
                    {
                            .nlmsg_len = sizeof(req),
                            .nlmsg_type = RTM_NEWTFILTER,
                            .nlmsg_flags = NETLINK_REQUEST_FLAGS | NLM_F_EXCL | NLM_F_CREATE,
                    },
            .t =
                    {
                            .tcm_family = AF_UNSPEC,
                            .tcm_ifindex = ifIndex,
                            .tcm_handle = TC_H_UNSPEC,
                            .tcm_parent = TC_H_MAKE(TC_H_CLSACT,
                                                    ingress ? TC_H_MIN_INGRESS : TC_H_MIN_EGRESS),
                            .tcm_info = static_cast<__u32>((prio << 16) | htons(proto)),
                    },
            .kind =
                    {
                            .attr =
                                    {
                                            .nla_len = sizeof(req.kind),
                                            .nla_type = TCA_KIND,
                                    },
                            .str = BPF,
                    },
            .options =
                    {
                            .attr =
                                    {
                                            .nla_len = sizeof(req.options),
                                            .nla_type = TCA_OPTIONS,
                                    },
                            .fd =
                                    {
                                            .attr =
                                                    {
                                                            .nla_len = sizeof(req.options.fd),
                                                            .nla_type = TCA_BPF_FD,
                                                    },
                                            .u32 = static_cast<__u32>(bpfFd),
                                    },
                            .name =
                                    {
                                            .attr =
                                                    {
                                                            .nla_len = sizeof(req.options.name),
                                                            .nla_type = TCA_BPF_NAME,
                                                    },
                                            // Visible via 'tc filter show', but
                                            // is overwritten by strncpy below
                                            .str = "placeholder",
                                    },
                            .flags =
                                    {
                                            .attr =
                                                    {
                                                            .nla_len = sizeof(req.options.flags),
                                                            .nla_type = TCA_BPF_FLAGS,
                                                    },
                                            .u32 = TCA_BPF_FLAG_ACT_DIRECT,
                                    },
                    },
    };
#undef BPF

    strncpy(req.options.name.str, name, sizeof(req.options.name.str));

    return sendAndProcessNetlinkResponse(&req, sizeof(req));
}

// tc filter del dev .. in/egress prio .. protocol ..
int tcFilterDelDev(int ifIndex, bool ingress, uint16_t prio, uint16_t proto) {
    const struct {
        nlmsghdr n;
        tcmsg t;
    } req = {
            .n =
                    {
                            .nlmsg_len = sizeof(req),
                            .nlmsg_type = RTM_DELTFILTER,
                            .nlmsg_flags = NETLINK_REQUEST_FLAGS,
                    },
            .t =
                    {
                            .tcm_family = AF_UNSPEC,
                            .tcm_ifindex = ifIndex,
                            .tcm_handle = TC_H_UNSPEC,
                            .tcm_parent = TC_H_MAKE(TC_H_CLSACT,
                                                    ingress ? TC_H_MIN_INGRESS : TC_H_MIN_EGRESS),
                            .tcm_info = static_cast<__u32>((prio << 16) | htons(proto)),
                    },
    };

    return sendAndProcessNetlinkResponse(&req, sizeof(req));
}

}  // namespace net
}  // namespace android
