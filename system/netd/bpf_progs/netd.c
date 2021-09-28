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

#include <bpf_helpers.h>
#include <linux/bpf.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <stdbool.h>
#include <stdint.h>
#include "bpf_net_helpers.h"
#include "netdbpf/bpf_shared.h"

// This is defined for cgroup bpf filter only.
#define BPF_DROP_UNLESS_DNS 2
#define BPF_PASS 1
#define BPF_DROP 0

// This is used for xt_bpf program only.
#define BPF_NOMATCH 0
#define BPF_MATCH 1

#define BPF_EGRESS 0
#define BPF_INGRESS 1

#define IP_PROTO_OFF offsetof(struct iphdr, protocol)
#define IPV6_PROTO_OFF offsetof(struct ipv6hdr, nexthdr)
#define IPPROTO_IHL_OFF 0
#define TCP_FLAG_OFF 13
#define RST_OFFSET 2

DEFINE_BPF_MAP_GRO(cookie_tag_map, HASH, uint64_t, UidTagValue, COOKIE_UID_MAP_SIZE,
                   AID_NET_BW_ACCT)
DEFINE_BPF_MAP_GRO(uid_counterset_map, HASH, uint32_t, uint8_t, UID_COUNTERSET_MAP_SIZE,
                   AID_NET_BW_ACCT)
DEFINE_BPF_MAP_GRO(app_uid_stats_map, HASH, uint32_t, StatsValue, APP_STATS_MAP_SIZE,
                   AID_NET_BW_STATS)
DEFINE_BPF_MAP_GRW(stats_map_A, HASH, StatsKey, StatsValue, STATS_MAP_SIZE, AID_NET_BW_STATS)
DEFINE_BPF_MAP_GRW(stats_map_B, HASH, StatsKey, StatsValue, STATS_MAP_SIZE, AID_NET_BW_STATS)
DEFINE_BPF_MAP_GRO(iface_stats_map, HASH, uint32_t, StatsValue, IFACE_STATS_MAP_SIZE,
                   AID_NET_BW_STATS)
DEFINE_BPF_MAP_GRO(configuration_map, HASH, uint32_t, uint8_t, CONFIGURATION_MAP_SIZE,
                   AID_NET_BW_STATS)
DEFINE_BPF_MAP(uid_owner_map, HASH, uint32_t, UidOwnerValue, UID_OWNER_MAP_SIZE)

/* never actually used from ebpf */
DEFINE_BPF_MAP_GRO(iface_index_name_map, HASH, uint32_t, IfaceValue, IFACE_INDEX_NAME_MAP_SIZE,
                   AID_NET_BW_STATS)

static __always_inline int is_system_uid(uint32_t uid) {
    return (uid <= MAX_SYSTEM_UID) && (uid >= MIN_SYSTEM_UID);
}

/*
 * Note: this blindly assumes an MTU of 1500, and that packets > MTU are always TCP,
 * and that TCP is using the Linux default settings with TCP timestamp option enabled
 * which uses 12 TCP option bytes per frame.
 *
 * These are not unreasonable assumptions:
 *
 * The internet does not really support MTUs greater than 1500, so most TCP traffic will
 * be at that MTU, or slightly below it (worst case our upwards adjustment is too small).
 *
 * The chance our traffic isn't IP at all is basically zero, so the IP overhead correction
 * is bound to be needed.
 *
 * Furthermore, the likelyhood that we're having to deal with GSO (ie. > MTU) packets that
 * are not IP/TCP is pretty small (few other things are supported by Linux) and worse case
 * our extra overhead will be slightly off, but probably still better than assuming none.
 *
 * Most servers are also Linux and thus support/default to using TCP timestamp option
 * (and indeed TCP timestamp option comes from RFC 1323 titled "TCP Extensions for High
 * Performance" which also defined TCP window scaling and are thus absolutely ancient...).
 *
 * All together this should be more correct than if we simply ignored GSO frames
 * (ie. counted them as single packets with no extra overhead)
 *
 * Especially since the number of packets is important for any future clat offload correction.
 * (which adjusts upward by 20 bytes per packet to account for ipv4 -> ipv6 header conversion)
 */
#define DEFINE_UPDATE_STATS(the_stats_map, TypeOfKey)                                          \
    static __always_inline inline void update_##the_stats_map(struct __sk_buff* skb,           \
                                                              int direction, TypeOfKey* key) { \
        StatsValue* value = bpf_##the_stats_map##_lookup_elem(key);                            \
        if (!value) {                                                                          \
            StatsValue newValue = {};                                                          \
            bpf_##the_stats_map##_update_elem(key, &newValue, BPF_NOEXIST);                    \
            value = bpf_##the_stats_map##_lookup_elem(key);                                    \
        }                                                                                      \
        if (value) {                                                                           \
            const int mtu = 1500;                                                              \
            uint64_t packets = 1;                                                              \
            uint64_t bytes = skb->len;                                                         \
            if (bytes > mtu) {                                                                 \
                bool is_ipv6 = (skb->protocol == htons(ETH_P_IPV6));                           \
                int ip_overhead = (is_ipv6 ? sizeof(struct ipv6hdr) : sizeof(struct iphdr));   \
                int tcp_overhead = ip_overhead + sizeof(struct tcphdr) + 12;                   \
                int mss = mtu - tcp_overhead;                                                  \
                uint64_t payload = bytes - tcp_overhead;                                       \
                packets = (payload + mss - 1) / mss;                                           \
                bytes = tcp_overhead * packets + payload;                                      \
            }                                                                                  \
            if (direction == BPF_EGRESS) {                                                     \
                __sync_fetch_and_add(&value->txPackets, packets);                              \
                __sync_fetch_and_add(&value->txBytes, bytes);                                  \
            } else if (direction == BPF_INGRESS) {                                             \
                __sync_fetch_and_add(&value->rxPackets, packets);                              \
                __sync_fetch_and_add(&value->rxBytes, bytes);                                  \
            }                                                                                  \
        }                                                                                      \
    }

DEFINE_UPDATE_STATS(app_uid_stats_map, uint32_t)
DEFINE_UPDATE_STATS(iface_stats_map, uint32_t)
DEFINE_UPDATE_STATS(stats_map_A, StatsKey)
DEFINE_UPDATE_STATS(stats_map_B, StatsKey)

static inline bool skip_owner_match(struct __sk_buff* skb) {
    int offset = -1;
    int ret = 0;
    if (skb->protocol == htons(ETH_P_IP)) {
        offset = IP_PROTO_OFF;
        uint8_t proto, ihl;
        uint8_t flag;
        ret = bpf_skb_load_bytes(skb, offset, &proto, 1);
        if (!ret) {
            if (proto == IPPROTO_ESP) {
                return true;
            } else if (proto == IPPROTO_TCP) {
                ret = bpf_skb_load_bytes(skb, IPPROTO_IHL_OFF, &ihl, 1);
                ihl = ihl & 0x0F;
                ret = bpf_skb_load_bytes(skb, ihl * 4 + TCP_FLAG_OFF, &flag, 1);
                if (ret == 0 && (flag >> RST_OFFSET & 1)) {
                    return true;
                }
            }
        }
    } else if (skb->protocol == htons(ETH_P_IPV6)) {
        offset = IPV6_PROTO_OFF;
        uint8_t proto;
        ret = bpf_skb_load_bytes(skb, offset, &proto, 1);
        if (!ret) {
            if (proto == IPPROTO_ESP) {
                return true;
            } else if (proto == IPPROTO_TCP) {
                uint8_t flag;
                ret = bpf_skb_load_bytes(skb, sizeof(struct ipv6hdr) + TCP_FLAG_OFF, &flag, 1);
                if (ret == 0 && (flag >> RST_OFFSET & 1)) {
                    return true;
                }
            }
        }
    }
    return false;
}

static __always_inline BpfConfig getConfig(uint32_t configKey) {
    uint32_t mapSettingKey = configKey;
    BpfConfig* config = bpf_configuration_map_lookup_elem(&mapSettingKey);
    if (!config) {
        // Couldn't read configuration entry. Assume everything is disabled.
        return DEFAULT_CONFIG;
    }
    return *config;
}

static inline int bpf_owner_match(struct __sk_buff* skb, uint32_t uid, int direction) {
    if (skip_owner_match(skb)) return BPF_PASS;

    if (is_system_uid(uid)) return BPF_PASS;

    BpfConfig enabledRules = getConfig(UID_RULES_CONFIGURATION_KEY);

    UidOwnerValue* uidEntry = bpf_uid_owner_map_lookup_elem(&uid);
    uint8_t uidRules = uidEntry ? uidEntry->rule : 0;
    uint32_t allowed_iif = uidEntry ? uidEntry->iif : 0;

    if (enabledRules) {
        if ((enabledRules & DOZABLE_MATCH) && !(uidRules & DOZABLE_MATCH)) {
            return BPF_DROP;
        }
        if ((enabledRules & STANDBY_MATCH) && (uidRules & STANDBY_MATCH)) {
            return BPF_DROP;
        }
        if ((enabledRules & POWERSAVE_MATCH) && !(uidRules & POWERSAVE_MATCH)) {
            return BPF_DROP;
        }
    }
    if (direction == BPF_INGRESS && (uidRules & IIF_MATCH)) {
        // Drops packets not coming from lo nor the whitelisted interface
        if (allowed_iif && skb->ifindex != 1 && skb->ifindex != allowed_iif) {
            return BPF_DROP_UNLESS_DNS;
        }
    }
    return BPF_PASS;
}

static __always_inline inline void update_stats_with_config(struct __sk_buff* skb, int direction,
                                                            StatsKey* key, uint8_t selectedMap) {
    if (selectedMap == SELECT_MAP_A) {
        update_stats_map_A(skb, direction, key);
    } else if (selectedMap == SELECT_MAP_B) {
        update_stats_map_B(skb, direction, key);
    }
}

static __always_inline inline int bpf_traffic_account(struct __sk_buff* skb, int direction) {
    uint32_t sock_uid = bpf_get_socket_uid(skb);
    // Always allow and never count clat traffic. Only the IPv4 traffic on the stacked
    // interface is accounted for and subject to usage restrictions.
    if (sock_uid == AID_CLAT) {
        return BPF_PASS;
    }

    int match = bpf_owner_match(skb, sock_uid, direction);
    if ((direction == BPF_EGRESS) && (match == BPF_DROP)) {
        // If an outbound packet is going to be dropped, we do not count that
        // traffic.
        return match;
    }

    uint64_t cookie = bpf_get_socket_cookie(skb);
    UidTagValue* utag = bpf_cookie_tag_map_lookup_elem(&cookie);
    uint32_t uid, tag;
    if (utag) {
        uid = utag->uid;
        tag = utag->tag;
    } else {
        uid = sock_uid;
        tag = 0;
    }

// Workaround for secureVPN with VpnIsolation enabled, refer to b/159994981 for details.
// Keep TAG_SYSTEM_DNS in sync with DnsResolver/include/netd_resolv/resolv.h
// and TrafficStatsConstants.java
#define TAG_SYSTEM_DNS 0xFFFFFF82
    if (tag == TAG_SYSTEM_DNS && uid == AID_DNS) {
        uid = sock_uid;
        if (match == BPF_DROP_UNLESS_DNS) match = BPF_PASS;
    } else {
        if (match == BPF_DROP_UNLESS_DNS) match = BPF_DROP;
    }

    StatsKey key = {.uid = uid, .tag = tag, .counterSet = 0, .ifaceIndex = skb->ifindex};

    uint8_t* counterSet = bpf_uid_counterset_map_lookup_elem(&uid);
    if (counterSet) key.counterSet = (uint32_t)*counterSet;

    uint32_t mapSettingKey = CURRENT_STATS_MAP_CONFIGURATION_KEY;
    uint8_t* selectedMap = bpf_configuration_map_lookup_elem(&mapSettingKey);
    if (!selectedMap) {
        return match;
    }

    if (key.tag) {
        update_stats_with_config(skb, direction, &key, *selectedMap);
        key.tag = 0;
    }

    update_stats_with_config(skb, direction, &key, *selectedMap);
    update_app_uid_stats_map(skb, direction, &uid);
    return match;
}

SEC("cgroupskb/ingress/stats")
int bpf_cgroup_ingress(struct __sk_buff* skb) {
    return bpf_traffic_account(skb, BPF_INGRESS);
}

SEC("cgroupskb/egress/stats")
int bpf_cgroup_egress(struct __sk_buff* skb) {
    return bpf_traffic_account(skb, BPF_EGRESS);
}

DEFINE_BPF_PROG("skfilter/egress/xtbpf", AID_ROOT, AID_NET_ADMIN, xt_bpf_egress_prog)
(struct __sk_buff* skb) {
    // Clat daemon does not generate new traffic, all its traffic is accounted for already
    // on the v4-* interfaces (except for the 20 (or 28) extra bytes of IPv6 vs IPv4 overhead,
    // but that can be corrected for later when merging v4-foo stats into interface foo's).
    uint32_t sock_uid = bpf_get_socket_uid(skb);
    if (sock_uid == AID_CLAT) return BPF_NOMATCH;

    uint32_t key = skb->ifindex;
    update_iface_stats_map(skb, BPF_EGRESS, &key);
    return BPF_MATCH;
}

DEFINE_BPF_PROG("skfilter/ingress/xtbpf", AID_ROOT, AID_NET_ADMIN, xt_bpf_ingress_prog)
(struct __sk_buff* skb) {
    // Clat daemon traffic is not accounted by virtue of iptables raw prerouting drop rule
    // (in clat_raw_PREROUTING chain), which triggers before this (in bw_raw_PREROUTING chain).
    // It will be accounted for on the v4-* clat interface instead.
    // Keep that in mind when moving this out of iptables xt_bpf and into tc ingress (or xdp).

    uint32_t key = skb->ifindex;
    update_iface_stats_map(skb, BPF_INGRESS, &key);
    return BPF_MATCH;
}

DEFINE_BPF_PROG("skfilter/whitelist/xtbpf", AID_ROOT, AID_NET_ADMIN, xt_bpf_whitelist_prog)
(struct __sk_buff* skb) {
    uint32_t sock_uid = bpf_get_socket_uid(skb);
    if (is_system_uid(sock_uid)) return BPF_MATCH;

    // 65534 is the overflow 'nobody' uid, usually this being returned means
    // that skb->sk is NULL during RX (early decap socket lookup failure),
    // which commonly happens for incoming packets to an unconnected udp socket.
    // Additionally bpf_get_socket_cookie() returns 0 if skb->sk is NULL
    if ((sock_uid == 65534) && !bpf_get_socket_cookie(skb) && is_received_skb(skb))
        return BPF_MATCH;

    UidOwnerValue* whitelistMatch = bpf_uid_owner_map_lookup_elem(&sock_uid);
    if (whitelistMatch) return whitelistMatch->rule & HAPPY_BOX_MATCH ? BPF_MATCH : BPF_NOMATCH;
    return BPF_NOMATCH;
}

DEFINE_BPF_PROG("skfilter/blacklist/xtbpf", AID_ROOT, AID_NET_ADMIN, xt_bpf_blacklist_prog)
(struct __sk_buff* skb) {
    uint32_t sock_uid = bpf_get_socket_uid(skb);
    UidOwnerValue* blacklistMatch = bpf_uid_owner_map_lookup_elem(&sock_uid);
    if (blacklistMatch) return blacklistMatch->rule & PENALTY_BOX_MATCH ? BPF_MATCH : BPF_NOMATCH;
    return BPF_NOMATCH;
}

DEFINE_BPF_MAP(uid_permission_map, HASH, uint32_t, uint8_t, UID_OWNER_MAP_SIZE)

DEFINE_BPF_PROG_KVER("cgroupsock/inet/create", AID_ROOT, AID_ROOT, inet_socket_create,
                     KVER(4, 14, 0))
(struct bpf_sock* sk) {
    uint64_t gid_uid = bpf_get_current_uid_gid();
    /*
     * A given app is guaranteed to have the same app ID in all the profiles in
     * which it is installed, and install permission is granted to app for all
     * user at install time so we only check the appId part of a request uid at
     * run time. See UserHandle#isSameApp for detail.
     */
    uint32_t appId = (gid_uid & 0xffffffff) % PER_USER_RANGE;
    uint8_t* permissions = bpf_uid_permission_map_lookup_elem(&appId);
    if (!permissions) {
        // UID not in map. Default to just INTERNET permission.
        return 1;
    }

    // A return value of 1 means allow, everything else means deny.
    return (*permissions & BPF_PERMISSION_INTERNET) == BPF_PERMISSION_INTERNET;
}

LICENSE("Apache 2.0");
CRITICAL("netd");
