/*
 * Copyright 2011 Daniel Drown
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
 * config.h - configuration settings
 */
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <linux/if.h>
#include <netinet/in.h>

#include "ring.h"

struct tun_data {
  char device4[IFNAMSIZ];
  int read_fd6, write_fd6, fd4;
  struct packet_ring ring;
};

struct clat_config {
  struct in6_addr ipv6_local_subnet;
  struct in_addr ipv4_local_subnet;
  struct in6_addr plat_subnet;
  const char *native_ipv6_interface;
};

extern struct clat_config Global_Clatd_Config;

/* function: ipv6_prefix_equal
 * compares the /64 prefixes of two ipv6 addresses.
 *   a1 - first address
 *   a2 - second address
 *   returns: 0 if the subnets are different, 1 if they are the same.
 */
static inline int ipv6_prefix_equal(struct in6_addr *a1, struct in6_addr *a2) {
  return !memcmp(a1, a2, 8);
}

#endif /* __CONFIG_H__ */
