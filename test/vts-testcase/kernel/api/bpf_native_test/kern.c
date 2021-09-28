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

#include "kern.h"
#include <linux/bpf.h>
#include <stdint.h>
#include "bpf_helpers.h"
#include "bpf_net_helpers.h"

DEFINE_BPF_MAP(test_configuration_map, HASH, uint32_t, uint32_t, 1)
DEFINE_BPF_MAP(test_stats_map_A, HASH, uint64_t, stats_value, NUM_SOCKETS)
DEFINE_BPF_MAP(test_stats_map_B, HASH, uint64_t, stats_value, NUM_SOCKETS)

#define DEFINE_UPDATE_INGRESS_STATS(the_map)                               \
  static inline void update_ingress_##the_map(struct __sk_buff* skb) {     \
    uint64_t sock_cookie = bpf_get_socket_cookie(skb);                     \
    stats_value* value = bpf_##the_map##_lookup_elem(&sock_cookie);        \
    if (!value) {                                                          \
      stats_value newValue = {};                                           \
      bpf_##the_map##_update_elem(&sock_cookie, &newValue, BPF_NOEXIST);   \
      value = bpf_##the_map##_lookup_elem(&sock_cookie);                   \
    }                                                                      \
    if (value) {                                                           \
      __sync_fetch_and_add(&value->rxPackets, 1);                          \
      __sync_fetch_and_add(&value->rxBytes, skb->len);                     \
    }                                                                      \
  }

DEFINE_UPDATE_INGRESS_STATS(test_stats_map_A)
DEFINE_UPDATE_INGRESS_STATS(test_stats_map_B)

SEC("skfilter/test")
int ingress_prog(struct __sk_buff* skb) {
  uint32_t key = 1;
  uint32_t* config = bpf_test_configuration_map_lookup_elem(&key);
  if (config) {
    if (*config) {
      update_ingress_test_stats_map_A(skb);
    } else {
      update_ingress_test_stats_map_B(skb);
    }
  }
  return skb->len;
}

LICENSE("Apache 2.0");
