/*
 * Copyright 2012 Daniel Drown
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
 * clatd.c - tun interface setup and main event loop
 */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <linux/filter.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <sys/capability.h>
#include <sys/uio.h>

#include <netid_client.h>                       // For MARK_UNSET.
#include <private/android_filesystem_config.h>  // For AID_CLAT.

#include "clatd.h"
#include "config.h"
#include "dump.h"
#include "getaddr.h"
#include "logging.h"
#include "ring.h"
#include "setif.h"
#include "translate.h"

struct clat_config Global_Clatd_Config;

/* 40 bytes IPv6 header - 20 bytes IPv4 header + 8 bytes fragment header */
#define MTU_DELTA 28

volatile sig_atomic_t running = 1;

/* function: stop_loop
 * signal handler: stop the event loop
 */
void stop_loop() { running = 0; }

/* function: configure_packet_socket
 * Binds the packet socket and attaches the receive filter to it.
 *   sock - the socket to configure
 */
int configure_packet_socket(int sock) {
  uint32_t *ipv6 = Global_Clatd_Config.ipv6_local_subnet.s6_addr32;

  // clang-format off
  struct sock_filter filter_code[] = {
    // Load the first four bytes of the IPv6 destination address (starts 24 bytes in).
    // Compare it against the first four bytes of our IPv6 address, in host byte order (BPF loads
    // are always in host byte order). If it matches, continue with next instruction (JMP 0). If it
    // doesn't match, jump ahead to statement that returns 0 (ignore packet). Repeat for the other
    // three words of the IPv6 address, and if they all match, return PACKETLEN (accept packet).
    BPF_STMT(BPF_LD  | BPF_W   | BPF_ABS,  24),
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K,    htonl(ipv6[0]), 0, 7),
    BPF_STMT(BPF_LD  | BPF_W   | BPF_ABS,  28),
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K,    htonl(ipv6[1]), 0, 5),
    BPF_STMT(BPF_LD  | BPF_W   | BPF_ABS,  32),
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K,    htonl(ipv6[2]), 0, 3),
    BPF_STMT(BPF_LD  | BPF_W   | BPF_ABS,  36),
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K,    htonl(ipv6[3]), 0, 1),
    BPF_STMT(BPF_RET | BPF_K,              PACKETLEN),
    BPF_STMT(BPF_RET | BPF_K,              0),
  };
  // clang-format on
  struct sock_fprog filter = { sizeof(filter_code) / sizeof(filter_code[0]), filter_code };

  if (setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER, &filter, sizeof(filter))) {
    logmsg(ANDROID_LOG_FATAL, "attach packet filter failed: %s", strerror(errno));
    return 0;
  }

  struct sockaddr_ll sll = {
    .sll_family   = AF_PACKET,
    .sll_protocol = htons(ETH_P_IPV6),
    .sll_ifindex  = if_nametoindex(Global_Clatd_Config.native_ipv6_interface),
    .sll_pkttype  = PACKET_OTHERHOST,  // The 464xlat IPv6 address is not assigned to the kernel.
  };
  if (bind(sock, (struct sockaddr *)&sll, sizeof(sll))) {
    logmsg(ANDROID_LOG_FATAL, "binding packet socket: %s", strerror(errno));
    return 0;
  }

  return 1;
}

/* function: configure_tun_ip
 * configures the ipv4 and ipv6 addresses on the tunnel interface
 *   tunnel - tun device data
 *   mtu    - mtu of tun device
 */
void configure_tun_ip(const struct tun_data *tunnel, const char *v4_addr, int mtu) {
  if (!v4_addr || !inet_pton(AF_INET, v4_addr, &Global_Clatd_Config.ipv4_local_subnet.s_addr)) {
    logmsg(ANDROID_LOG_FATAL, "Invalid IPv4 address %s", v4_addr);
    exit(1);
  }

  char addrstr[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &Global_Clatd_Config.ipv4_local_subnet, addrstr, sizeof(addrstr));
  logmsg(ANDROID_LOG_INFO, "Using IPv4 address %s on %s", addrstr, tunnel->device4);

  // Configure the interface before bringing it up. As soon as we bring the interface up, the
  // framework will be notified and will assume the interface's configuration has been finalized.
  int status = add_address(tunnel->device4, AF_INET, &Global_Clatd_Config.ipv4_local_subnet, 32,
                           &Global_Clatd_Config.ipv4_local_subnet);
  if (status < 0) {
    logmsg(ANDROID_LOG_FATAL, "configure_tun_ip/if_address(4) failed: %s", strerror(-status));
    exit(1);
  }

  status = if_up(tunnel->device4, mtu);
  if (status < 0) {
    logmsg(ANDROID_LOG_FATAL, "configure_tun_ip/if_up(4) failed: %s", strerror(-status));
    exit(1);
  }
}

/* function: set_capability
 * set the permitted, effective and inheritable capabilities of the current
 * thread
 */
void set_capability(uint64_t target_cap) {
  struct __user_cap_header_struct header = {
    .version = _LINUX_CAPABILITY_VERSION_3,
    .pid     = 0  // 0 = change myself
  };
  struct __user_cap_data_struct cap[_LINUX_CAPABILITY_U32S_3] = {};

  cap[0].permitted = cap[0].effective = cap[0].inheritable = target_cap;
  cap[1].permitted = cap[1].effective = cap[1].inheritable = target_cap >> 32;

  if (capset(&header, cap) < 0) {
    logmsg(ANDROID_LOG_FATAL, "capset failed: %s", strerror(errno));
    exit(1);
  }
}

/* function: drop_root_but_keep_caps
 * drops root privs but keeps the needed capabilities
 */
void drop_root_but_keep_caps() {
  gid_t groups[] = { AID_INET, AID_VPN };
  if (setgroups(sizeof(groups) / sizeof(groups[0]), groups) < 0) {
    logmsg(ANDROID_LOG_FATAL, "setgroups failed: %s", strerror(errno));
    exit(1);
  }

  prctl(PR_SET_KEEPCAPS, 1);

  if (setresgid(AID_CLAT, AID_CLAT, AID_CLAT) < 0) {
    logmsg(ANDROID_LOG_FATAL, "setresgid failed: %s", strerror(errno));
    exit(1);
  }
  if (setresuid(AID_CLAT, AID_CLAT, AID_CLAT) < 0) {
    logmsg(ANDROID_LOG_FATAL, "setresuid failed: %s", strerror(errno));
    exit(1);
  }

  // keep CAP_NET_RAW capability to open raw socket, and CAP_IPC_LOCK for mmap
  // to lock memory.
  set_capability((1 << CAP_NET_ADMIN) |
                 (1 << CAP_NET_RAW) |
                 (1 << CAP_IPC_LOCK));
}

/* function: open_sockets
 * opens a packet socket to receive IPv6 packets and a raw socket to send them
 *   tunnel - tun device data
 *   mark - the socket mark to use for the sending raw socket
 */
void open_sockets(struct tun_data *tunnel, uint32_t mark) {
  int rawsock = socket(AF_INET6, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_RAW);
  if (rawsock < 0) {
    logmsg(ANDROID_LOG_FATAL, "raw socket failed: %s", strerror(errno));
    exit(1);
  }

  if (mark != MARK_UNSET && setsockopt(rawsock, SOL_SOCKET, SO_MARK, &mark, sizeof(mark)) < 0) {
    logmsg(ANDROID_LOG_ERROR, "could not set mark on raw socket: %s", strerror(errno));
  }

  tunnel->write_fd6 = rawsock;

  tunnel->read_fd6 = ring_create(tunnel);
  if (tunnel->read_fd6 < 0) {
    exit(1);
  }
}

int ipv6_address_changed(const char *interface) {
  union anyip *interface_ip;

  interface_ip = getinterface_ip(interface, AF_INET6);
  if (!interface_ip) {
    logmsg(ANDROID_LOG_ERROR, "Unable to find an IPv6 address on interface %s", interface);
    return 1;
  }

  if (!ipv6_prefix_equal(&interface_ip->ip6, &Global_Clatd_Config.ipv6_local_subnet)) {
    char oldstr[INET6_ADDRSTRLEN];
    char newstr[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &Global_Clatd_Config.ipv6_local_subnet, oldstr, sizeof(oldstr));
    inet_ntop(AF_INET6, &interface_ip->ip6, newstr, sizeof(newstr));
    logmsg(ANDROID_LOG_INFO, "IPv6 prefix on %s changed: %s -> %s", interface, oldstr, newstr);
    free(interface_ip);
    return 1;
  } else {
    free(interface_ip);
    return 0;
  }
}

/* function: configure_clat_ipv6_address
 * picks the clat IPv6 address and configures packet translation to use it.
 *   tunnel - tun device data
 *   interface - uplink interface name
 *   returns: 1 on success, 0 on failure
 */
int configure_clat_ipv6_address(const struct tun_data *tunnel, const char *interface,
                                const char *v6_addr) {
  if (!v6_addr || !inet_pton(AF_INET6, v6_addr, &Global_Clatd_Config.ipv6_local_subnet)) {
    logmsg(ANDROID_LOG_FATAL, "Invalid source address %s", v6_addr);
    return 0;
  }

  char addrstr[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6, &Global_Clatd_Config.ipv6_local_subnet, addrstr, sizeof(addrstr));
  logmsg(ANDROID_LOG_INFO, "Using IPv6 address %s on %s", addrstr, interface);

  // Start translating packets to the new prefix.
  add_anycast_address(tunnel->write_fd6, &Global_Clatd_Config.ipv6_local_subnet, interface);

  // Update our packet socket filter to reflect the new 464xlat IP address.
  if (!configure_packet_socket(tunnel->read_fd6)) {
    // Things aren't going to work. Bail out and hope we have better luck next time.
    // We don't log an error here because configure_packet_socket has already done so.
    return 0;
  }

  return 1;
}

int detect_mtu(const struct in6_addr *plat_subnet, uint32_t plat_suffix, uint32_t mark) {
  // Create an IPv6 UDP socket.
  int s = socket(AF_INET6, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (s < 0) {
    logmsg(ANDROID_LOG_FATAL, "socket(AF_INET6, SOCK_DGRAM, 0) failed");
    exit(1);
  }

  // Socket's mark affects routing decisions (network selection)
  if ((mark != MARK_UNSET) && setsockopt(s, SOL_SOCKET, SO_MARK, &mark, sizeof(mark))) {
    logmsg(ANDROID_LOG_FATAL, "setsockopt(SOL_SOCKET, SO_MARK) failed: %s", strerror(errno));
    exit(1);
  }

  // Try to connect udp socket to plat_subnet(96 bits):plat_suffix(32 bits)
  struct sockaddr_in6 dst = {
    .sin6_family = AF_INET6,
    .sin6_addr   = *plat_subnet,
  };
  dst.sin6_addr.s6_addr32[3] = plat_suffix;
  if (connect(s, (struct sockaddr *)&dst, sizeof(dst))) {
    logmsg(ANDROID_LOG_FATAL, "connect() failed: %s", strerror(errno));
    exit(1);
  }

  // Fetch the socket's IPv6 mtu - this is effectively fetching mtu from routing table
  int mtu;
  socklen_t sz_mtu = sizeof(mtu);
  if (getsockopt(s, SOL_IPV6, IPV6_MTU, &mtu, &sz_mtu)) {
    logmsg(ANDROID_LOG_FATAL, "getsockopt(SOL_IPV6, IPV6_MTU) failed: %s", strerror(errno));
    exit(1);
  }
  if (sz_mtu != sizeof(mtu)) {
    logmsg(ANDROID_LOG_FATAL, "getsockopt(SOL_IPV6, IPV6_MTU) returned unexpected size: %d",
           sz_mtu);
    exit(1);
  }
  close(s);

  return mtu;
}

/* function: configure_interface
 * reads the configuration and applies it to the interface
 *   uplink_interface - network interface to use to reach the ipv6 internet
 *   plat_prefix      - PLAT prefix to use
 *   v4_addr          - the v4 address to use on the tunnel interface
 *   v6_addr          - the v6 address to use on the native interface
 *   tunnel           - tun device data
 *   mark             - the socket mark to use for the sending raw socket
 */
void configure_interface(const char *uplink_interface, const char *plat_prefix, const char *v4_addr,
                         const char *v6_addr, struct tun_data *tunnel, uint32_t mark) {
  Global_Clatd_Config.native_ipv6_interface = uplink_interface;
  if (!plat_prefix || inet_pton(AF_INET6, plat_prefix, &Global_Clatd_Config.plat_subnet) <= 0) {
    logmsg(ANDROID_LOG_FATAL, "invalid IPv6 address specified for plat prefix: %s", plat_prefix);
    exit(1);
  }

  int mtu = detect_mtu(&Global_Clatd_Config.plat_subnet, htonl(0x08080808), mark);
  // clamp to minimum ipv6 mtu - this probably cannot ever trigger
  if (mtu < 1280) mtu = 1280;
  // clamp to buffer size
  if (mtu > MAXMTU) mtu = MAXMTU;
  // decrease by ipv6(40) + ipv6 fragmentation header(8) vs ipv4(20) overhead of 28 bytes
  mtu -= MTU_DELTA;
  logmsg(ANDROID_LOG_WARN, "ipv4 mtu is %d", mtu);

  configure_tun_ip(tunnel, v4_addr, mtu);

  if (!configure_clat_ipv6_address(tunnel, uplink_interface, v6_addr)) {
    exit(1);
  }
}

/* function: read_packet
 * reads a packet from the tunnel fd and translates it
 *   read_fd  - file descriptor to read original packet from
 *   write_fd - file descriptor to write translated packet to
 *   to_ipv6  - whether the packet is to be translated to ipv6 or ipv4
 */
void read_packet(int read_fd, int write_fd, int to_ipv6) {
  ssize_t readlen;
  uint8_t buf[PACKETLEN], *packet;

  readlen = read(read_fd, buf, PACKETLEN);

  if (readlen < 0) {
    if (errno != EAGAIN) {
      logmsg(ANDROID_LOG_WARN, "read_packet/read error: %s", strerror(errno));
    }
    return;
  } else if (readlen == 0) {
    logmsg(ANDROID_LOG_WARN, "read_packet/tun interface removed");
    running = 0;
    return;
  }

  struct tun_pi *tun_header = (struct tun_pi *)buf;
  if (readlen < (ssize_t)sizeof(*tun_header)) {
    logmsg(ANDROID_LOG_WARN, "read_packet/short read: got %ld bytes", readlen);
    return;
  }

  uint16_t proto = ntohs(tun_header->proto);
  if (proto != ETH_P_IP) {
    logmsg(ANDROID_LOG_WARN, "%s: unknown packet type = 0x%x", __func__, proto);
    return;
  }

  if (tun_header->flags != 0) {
    logmsg(ANDROID_LOG_WARN, "%s: unexpected flags = %d", __func__, tun_header->flags);
  }

  packet = (uint8_t *)(tun_header + 1);
  readlen -= sizeof(*tun_header);
  translate_packet(write_fd, to_ipv6, packet, readlen);
}

/* function: event_loop
 * reads packets from the tun network interface and passes them down the stack
 *   tunnel - tun device data
 */
void event_loop(struct tun_data *tunnel) {
  time_t last_interface_poll;
  struct pollfd wait_fd[] = {
    { tunnel->read_fd6, POLLIN, 0 },
    { tunnel->fd4, POLLIN, 0 },
  };

  // start the poll timer
  last_interface_poll = time(NULL);

  while (running) {
    if (poll(wait_fd, ARRAY_SIZE(wait_fd), NO_TRAFFIC_INTERFACE_POLL_FREQUENCY * 1000) == -1) {
      if (errno != EINTR) {
        logmsg(ANDROID_LOG_WARN, "event_loop/poll returned an error: %s", strerror(errno));
      }
    } else {
      if (wait_fd[0].revents & POLLIN) {
        ring_read(&tunnel->ring, tunnel->fd4, 0 /* to_ipv6 */);
      }
      // If any other bit is set, assume it's due to an error (i.e. POLLERR).
      if (wait_fd[0].revents & ~POLLIN) {
        // ring_read doesn't clear the error indication on the socket.
        recv(tunnel->read_fd6, NULL, 0, MSG_PEEK);
        logmsg(ANDROID_LOG_WARN, "event_loop: clearing error on read_fd6: %s", strerror(errno));
      }

      // Call read_packet if the socket has data to be read, but also if an
      // error is waiting. If we don't call read() after getting POLLERR, a
      // subsequent poll() will return immediately with POLLERR again,
      // causing this code to spin in a loop. Calling read() will clear the
      // socket error flag instead.
      if (wait_fd[1].revents) {
        read_packet(tunnel->fd4, tunnel->write_fd6, 1 /* to_ipv6 */);
      }
    }

    time_t now = time(NULL);
    if (now >= (last_interface_poll + INTERFACE_POLL_FREQUENCY)) {
      last_interface_poll = now;
      if (ipv6_address_changed(Global_Clatd_Config.native_ipv6_interface)) {
        break;
      }
    }
  }
}
