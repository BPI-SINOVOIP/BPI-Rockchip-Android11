/*	$NetBSD: resolv.h,v 1.31 2005/12/26 19:01:47 perry Exp $	*/

/*
 * Copyright (c) 1983, 1987, 1989
 *    The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#pragma once

#include <android-base/logging.h>
#include <android-base/unique_fd.h>
#include <net/if.h>
#include <time.h>
#include <string>
#include <vector>

#include "DnsResolver.h"
#include "netd_resolv/resolv.h"
#include "params.h"
#include "stats.pb.h"

// Linux defines MAXHOSTNAMELEN as 64, while the domain name limit in
// RFC 1034 and RFC 1035 is 255 octets.
#ifdef MAXHOSTNAMELEN
#undef MAXHOSTNAMELEN
#endif
#define MAXHOSTNAMELEN 256

/*
 * Global defines and variables for resolver stub.
 */
#define RES_TIMEOUT 5000 /* min. milliseconds between retries */
#define RES_DFLRETRY 2    /* Default #/tries. */

// Flags for res_state->_flags
#define RES_F_VC 0x00000001        // socket is TCP
#define RES_F_EDNS0ERR 0x00000004  // EDNS0 caused errors

// Holds either a sockaddr_in or a sockaddr_in6.
union sockaddr_union {
    struct sockaddr sa;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
};
constexpr int MAXPACKET = 8 * 1024;

struct ResState {
    void closeSockets() {
        tcp_nssock.reset();
        _flags &= ~RES_F_VC;

        for (auto& sock : nssocks) {
            sock.reset();
        }
    }

    int nameserverCount() { return nsaddrs.size(); }

    // clang-format off
    unsigned netid;                             // NetId: cache key and socket mark
    uid_t uid;                                  // uid of the app that sent the DNS lookup
    pid_t pid;                                  // pid of the app that sent the DNS lookup
    uint16_t id;                                // current message id
    std::vector<std::string> search_domains{};  // domains to search
    std::vector<android::netdutils::IPSockAddr> nsaddrs;
    android::base::unique_fd nssocks[MAXNS];    // UDP sockets to nameservers
    unsigned ndots : 4;                         // threshold for initial abs. query
    unsigned _mark;                             // If non-0 SET_MARK to _mark on all request sockets
    android::base::unique_fd tcp_nssock;        // TCP socket (but why not one per nameserver?)
    uint32_t _flags = 0;                        // See RES_F_* defines below
    android::net::NetworkDnsEventReported* event;
    uint32_t netcontext_flags;
    int tc_mode = 0;
    bool enforce_dns_uid = false;
    // clang-format on
};

// TODO: remove these legacy aliases
typedef ResState* res_state;

/* End of stats related definitions */

/*
 * Error code extending h_errno codes defined in bionic/libc/include/netdb.h.
 *
 * This error code, including legacy h_errno, is returned from res_nquery(), res_nsearch(),
 * res_nquerydomain(), res_queryN(), res_searchN() and res_querydomainN() for DNS metrics.
 *
 * TODO: Consider mapping legacy and extended h_errno into a unified resolver error code mapping.
 */
#define NETD_RESOLV_H_ERRNO_EXT_TIMEOUT RCODE_TIMEOUT

extern const char* const _res_opcodes[];

int res_nameinquery(const char*, int, int, const uint8_t*, const uint8_t*);
int res_queriesmatch(const uint8_t*, const uint8_t*, const uint8_t*, const uint8_t*);

int res_nquery(res_state, const char*, int, int, uint8_t*, int, int*);
int res_nsearch(res_state, const char*, int, int, uint8_t*, int, int*);
int res_nquerydomain(res_state, const char*, const char*, int, int, uint8_t*, int, int*);
int res_nmkquery(int op, const char* qname, int cl, int type, const uint8_t* data, int datalen,
                 uint8_t* buf, int buflen, int netcontext_flags);
int res_nsend(res_state statp, const uint8_t* buf, int buflen, uint8_t* ans, int anssiz, int* rcode,
              uint32_t flags, std::chrono::milliseconds sleepTimeMs = {});
int res_nopt(res_state, int, uint8_t*, int, int);

int getaddrinfo_numeric(const char* hostname, const char* servname, addrinfo hints,
                        addrinfo** result);

// Helper function for converting h_errno to the error codes visible to netd
int herrnoToAiErrno(int herrno);

// switch resolver log severity
android::base::LogSeverity logSeverityStrToEnum(const std::string& logSeverityStr);

template <typename Dest>
Dest saturate_cast(int64_t x) {
    using DestLimits = std::numeric_limits<Dest>;
    if (x > DestLimits::max()) return DestLimits::max();
    if (x < DestLimits::min()) return DestLimits::min();
    return static_cast<Dest>(x);
}

android::net::NsType getQueryType(const uint8_t* msg, size_t msgLen);

android::net::IpVersion ipFamilyToIPVersion(int ipFamily);

inline void resolv_tag_socket(int sock, uid_t uid, pid_t pid) {
    if (android::net::gResNetdCallbacks.tagSocket != nullptr) {
        if (int err = android::net::gResNetdCallbacks.tagSocket(sock, TAG_SYSTEM_DNS, uid, pid)) {
            LOG(WARNING) << "Failed to tag socket: " << strerror(-err);
        }
    }

    if (fchown(sock, uid, -1) == -1) {
        LOG(WARNING) << "Failed to chown socket: " << strerror(errno);
    }
}

inline std::string addrToString(const sockaddr_storage* addr) {
    char out[INET6_ADDRSTRLEN] = {0};
    getnameinfo((const sockaddr*)addr, sizeof(sockaddr_storage), out, INET6_ADDRSTRLEN, nullptr, 0,
                NI_NUMERICHOST);
    return std::string(out);
}
