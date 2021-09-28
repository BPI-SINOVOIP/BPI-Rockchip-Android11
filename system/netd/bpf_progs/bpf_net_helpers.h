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

#ifndef NETDBPF_BPF_NET_HELPERS_H
#define NETDBPF_BPF_NET_HELPERS_H

#include <linux/bpf.h>
#include <linux/if_packet.h>
#include <stdbool.h>
#include <stdint.h>

// this returns 0 iff skb->sk is NULL
static uint64_t (*bpf_get_socket_cookie)(struct __sk_buff* skb) = (void*)BPF_FUNC_get_socket_cookie;

static uint32_t (*bpf_get_socket_uid)(struct __sk_buff* skb) = (void*)BPF_FUNC_get_socket_uid;
static int (*bpf_skb_load_bytes)(struct __sk_buff* skb, int off, void* to,
                                 int len) = (void*)BPF_FUNC_skb_load_bytes;

static int64_t (*bpf_csum_diff)(__be32* from, __u32 from_size, __be32* to, __u32 to_size,
                                __wsum seed) = (void*)BPF_FUNC_csum_diff;

static int64_t (*bpf_csum_update)(struct __sk_buff* skb, __wsum csum) = (void*)BPF_FUNC_csum_update;

static int (*bpf_skb_change_proto)(struct __sk_buff* skb, __be16 proto,
                                   __u64 flags) = (void*)BPF_FUNC_skb_change_proto;
static int (*bpf_l3_csum_replace)(struct __sk_buff* skb, __u32 offset, __u64 from, __u64 to,
                                  __u64 flags) = (void*)BPF_FUNC_l3_csum_replace;
static int (*bpf_l4_csum_replace)(struct __sk_buff* skb, __u32 offset, __u64 from, __u64 to,
                                  __u64 flags) = (void*)BPF_FUNC_l4_csum_replace;
static int (*bpf_redirect)(__u32 ifindex, __u64 flags) = (void*)BPF_FUNC_redirect;

static int (*bpf_skb_change_head)(struct __sk_buff* skb, __u32 head_room,
                                  __u64 flags) = (void*)BPF_FUNC_skb_change_head;
static int (*bpf_skb_adjust_room)(struct __sk_buff* skb, __s32 len_diff, __u32 mode,
                                  __u64 flags) = (void*)BPF_FUNC_skb_adjust_room;

// Android only supports little endian architectures
#define htons(x) (__builtin_constant_p(x) ? ___constant_swab16(x) : __builtin_bswap16(x))
#define htonl(x) (__builtin_constant_p(x) ? ___constant_swab32(x) : __builtin_bswap32(x))
#define ntohs(x) htons(x)
#define ntohl(x) htonl(x)

static inline __always_inline __unused bool is_received_skb(struct __sk_buff* skb) {
    return skb->pkt_type == PACKET_HOST || skb->pkt_type == PACKET_BROADCAST ||
           skb->pkt_type == PACKET_MULTICAST;
}

#endif  // NETDBPF_BPF_NET_HELPERS_H
