/*	$NetBSD: res_send.c,v 1.9 2006/01/24 17:41:25 christos Exp $	*/

/*
 * Copyright (c) 1985, 1989, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
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

/*
 * Send query to name server and wait for reply.
 */

#define LOG_TAG "resolv"

#include <chrono>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <android-base/logging.h>
#include <android-base/result.h>
#include <android/multinetwork.h>  // ResNsendFlags

#include <netdutils/Slice.h>
#include <netdutils/Stopwatch.h>
#include "DnsTlsDispatcher.h"
#include "DnsTlsTransport.h"
#include "Experiments.h"
#include "PrivateDnsConfiguration.h"
#include "netd_resolv/resolv.h"
#include "private/android_filesystem_config.h"

#include "res_comp.h"
#include "res_debug.h"
#include "res_init.h"
#include "resolv_cache.h"
#include "stats.h"
#include "stats.pb.h"
#include "util.h"

using namespace std::chrono_literals;
// TODO: use the namespace something like android::netd_resolv for libnetd_resolv
using android::base::ErrnoError;
using android::base::Result;
using android::net::CacheStatus;
using android::net::DnsQueryEvent;
using android::net::DnsTlsDispatcher;
using android::net::DnsTlsTransport;
using android::net::gPrivateDnsConfiguration;
using android::net::IpVersion;
using android::net::IV_IPV4;
using android::net::IV_IPV6;
using android::net::IV_UNKNOWN;
using android::net::LinuxErrno;
using android::net::NetworkDnsEventReported;
using android::net::NS_T_INVALID;
using android::net::NsRcode;
using android::net::NsType;
using android::net::PrivateDnsMode;
using android::net::PrivateDnsModes;
using android::net::PrivateDnsStatus;
using android::net::PROTO_TCP;
using android::net::PROTO_UDP;
using android::netdutils::IPSockAddr;
using android::netdutils::Slice;
using android::netdutils::Stopwatch;

static DnsTlsDispatcher sDnsTlsDispatcher;

static int send_vc(res_state statp, res_params* params, const uint8_t* buf, int buflen,
                   uint8_t* ans, int anssiz, int* terrno, size_t ns, time_t* at, int* rcode,
                   int* delay);
static int send_dg(res_state statp, res_params* params, const uint8_t* buf, int buflen,
                   uint8_t* ans, int anssiz, int* terrno, size_t* ns, int* v_circuit,
                   int* gotsomewhere, time_t* at, int* rcode, int* delay);

static void dump_error(const char*, const struct sockaddr*, int);

static int sock_eq(struct sockaddr*, struct sockaddr*);
static int connect_with_timeout(int sock, const struct sockaddr* nsap, socklen_t salen,
                                const struct timespec timeout);
static int retrying_poll(const int sock, short events, const struct timespec* finish);
static int res_tls_send(res_state, const Slice query, const Slice answer, int* rcode,
                        bool* fallback);

NsType getQueryType(const uint8_t* msg, size_t msgLen) {
    ns_msg handle;
    ns_rr rr;
    if (ns_initparse((const uint8_t*)msg, msgLen, &handle) < 0 ||
        ns_parserr(&handle, ns_s_qd, 0, &rr) < 0) {
        return NS_T_INVALID;
    }
    return static_cast<NsType>(ns_rr_type(rr));
}

IpVersion ipFamilyToIPVersion(const int ipFamily) {
    switch (ipFamily) {
        case AF_INET:
            return IV_IPV4;
        case AF_INET6:
            return IV_IPV6;
        default:
            return IV_UNKNOWN;
    }
}

// BEGIN: Code copied from ISC eventlib
// TODO: move away from this code
#define BILLION 1000000000

static struct timespec evConsTime(time_t sec, long nsec) {
    struct timespec x;

    x.tv_sec = sec;
    x.tv_nsec = nsec;
    return (x);
}

static struct timespec evAddTime(struct timespec addend1, struct timespec addend2) {
    struct timespec x;

    x.tv_sec = addend1.tv_sec + addend2.tv_sec;
    x.tv_nsec = addend1.tv_nsec + addend2.tv_nsec;
    if (x.tv_nsec >= BILLION) {
        x.tv_sec++;
        x.tv_nsec -= BILLION;
    }
    return (x);
}

static struct timespec evSubTime(struct timespec minuend, struct timespec subtrahend) {
    struct timespec x;

    x.tv_sec = minuend.tv_sec - subtrahend.tv_sec;
    if (minuend.tv_nsec >= subtrahend.tv_nsec)
        x.tv_nsec = minuend.tv_nsec - subtrahend.tv_nsec;
    else {
        x.tv_nsec = BILLION - subtrahend.tv_nsec + minuend.tv_nsec;
        x.tv_sec--;
    }
    return (x);
}

static int evCmpTime(struct timespec a, struct timespec b) {
#define SGN(x) ((x) < 0 ? (-1) : (x) > 0 ? (1) : (0));
    time_t s = a.tv_sec - b.tv_sec;
    long n;

    if (s != 0) return SGN(s);

    n = a.tv_nsec - b.tv_nsec;
    return SGN(n);
}

static struct timespec evNowTime(void) {
    struct timespec tsnow;
    clock_gettime(CLOCK_REALTIME, &tsnow);
    return tsnow;
}

// END: Code copied from ISC eventlib

/* BIONIC-BEGIN: implement source port randomization */
static int random_bind(int s, int family) {
    sockaddr_union u;
    int j;
    socklen_t slen;

    /* clear all, this also sets the IP4/6 address to 'any' */
    memset(&u, 0, sizeof u);

    switch (family) {
        case AF_INET:
            u.sin.sin_family = family;
            slen = sizeof u.sin;
            break;
        case AF_INET6:
            u.sin6.sin6_family = family;
            slen = sizeof u.sin6;
            break;
        default:
            errno = EPROTO;
            return -1;
    }

    /* first try to bind to a random source port a few times */
    for (j = 0; j < 10; j++) {
        /* find a random port between 1025 .. 65534 */
        int port = 1025 + (arc4random_uniform(65535 - 1025));
        if (family == AF_INET)
            u.sin.sin_port = htons(port);
        else
            u.sin6.sin6_port = htons(port);

        if (!bind(s, &u.sa, slen)) return 0;
    }

    // nothing after 10 attempts, our network table is probably busy
    // let the system decide which port is best
    if (family == AF_INET)
        u.sin.sin_port = 0;
    else
        u.sin6.sin6_port = 0;

    return bind(s, &u.sa, slen);
}
/* BIONIC-END */

// Disables all nameservers other than selectedServer
static void res_set_usable_server(int selectedServer, int nscount, bool usable_servers[]) {
    int usableIndex = 0;
    for (int ns = 0; ns < nscount; ns++) {
        if (usable_servers[ns]) ++usableIndex;
        if (usableIndex != selectedServer) usable_servers[ns] = false;
    }
}

// Looks up the nameserver address in res.nsaddrs[], returns the ns number if found, otherwise -1.
static int res_ourserver_p(res_state statp, const sockaddr* sa) {
    const sockaddr_in *inp, *srv;
    const sockaddr_in6 *in6p, *srv6;
    int ns = 0;
    switch (sa->sa_family) {
        case AF_INET:
            inp = (const struct sockaddr_in*) (const void*) sa;

            for (const IPSockAddr& ipsa : statp->nsaddrs) {
                sockaddr_storage ss = ipsa;
                srv = reinterpret_cast<sockaddr_in*>(&ss);
                if (srv->sin_family == inp->sin_family && srv->sin_port == inp->sin_port &&
                    (srv->sin_addr.s_addr == INADDR_ANY ||
                     srv->sin_addr.s_addr == inp->sin_addr.s_addr))
                    return ns;
                ++ns;
            }
            break;
        case AF_INET6:
            in6p = (const struct sockaddr_in6*) (const void*) sa;
            for (const IPSockAddr& ipsa : statp->nsaddrs) {
                sockaddr_storage ss = ipsa;
                srv6 = reinterpret_cast<sockaddr_in6*>(&ss);
                if (srv6->sin6_family == in6p->sin6_family && srv6->sin6_port == in6p->sin6_port &&
#ifdef HAVE_SIN6_SCOPE_ID
                    (srv6->sin6_scope_id == 0 || srv6->sin6_scope_id == in6p->sin6_scope_id) &&
#endif
                    (IN6_IS_ADDR_UNSPECIFIED(&srv6->sin6_addr) ||
                     IN6_ARE_ADDR_EQUAL(&srv6->sin6_addr, &in6p->sin6_addr)))
                    return ns;
                ++ns;
            }
            break;
        default:
            break;
    }
    return -1;
}

/* int
 * res_nameinquery(name, type, cl, buf, eom)
 *	look for (name, type, cl) in the query section of packet (buf, eom)
 * requires:
 *	buf + HFIXEDSZ <= eom
 * returns:
 *	-1 : format error
 *	0  : not found
 *	>0 : found
 * author:
 *	paul vixie, 29may94
 */
int res_nameinquery(const char* name, int type, int cl, const uint8_t* buf, const uint8_t* eom) {
    const uint8_t* cp = buf + HFIXEDSZ;
    int qdcount = ntohs(((const HEADER*) (const void*) buf)->qdcount);

    while (qdcount-- > 0) {
        char tname[MAXDNAME + 1];
        int n = dn_expand(buf, eom, cp, tname, sizeof tname);
        if (n < 0) return (-1);
        cp += n;
        if (cp + 2 * INT16SZ > eom) return (-1);
        int ttype = ntohs(*reinterpret_cast<const uint16_t*>(cp));
        cp += INT16SZ;
        int tclass = ntohs(*reinterpret_cast<const uint16_t*>(cp));
        cp += INT16SZ;
        if (ttype == type && tclass == cl && ns_samename(tname, name) == 1) return (1);
    }
    return (0);
}

/* int
 * res_queriesmatch(buf1, eom1, buf2, eom2)
 *	is there a 1:1 mapping of (name,type,class)
 *	in (buf1,eom1) and (buf2,eom2)?
 * returns:
 *	-1 : format error
 *	0  : not a 1:1 mapping
 *	>0 : is a 1:1 mapping
 * author:
 *	paul vixie, 29may94
 */
int res_queriesmatch(const uint8_t* buf1, const uint8_t* eom1, const uint8_t* buf2,
                     const uint8_t* eom2) {
    const uint8_t* cp = buf1 + HFIXEDSZ;
    int qdcount = ntohs(((const HEADER*) (const void*) buf1)->qdcount);

    if (buf1 + HFIXEDSZ > eom1 || buf2 + HFIXEDSZ > eom2) return (-1);

    /*
     * Only header section present in replies to
     * dynamic update packets.
     */
    if ((((const HEADER*) (const void*) buf1)->opcode == ns_o_update) &&
        (((const HEADER*) (const void*) buf2)->opcode == ns_o_update))
        return (1);

    if (qdcount != ntohs(((const HEADER*) (const void*) buf2)->qdcount)) return (0);
    while (qdcount-- > 0) {
        char tname[MAXDNAME + 1];
        int n = dn_expand(buf1, eom1, cp, tname, sizeof tname);
        if (n < 0) return (-1);
        cp += n;
        if (cp + 2 * INT16SZ > eom1) return (-1);
        int ttype = ntohs(*reinterpret_cast<const uint16_t*>(cp));
        cp += INT16SZ;
        int tclass = ntohs(*reinterpret_cast<const uint16_t*>(cp));
        cp += INT16SZ;
        if (!res_nameinquery(tname, ttype, tclass, buf2, eom2)) return (0);
    }
    return (1);
}

static DnsQueryEvent* addDnsQueryEvent(NetworkDnsEventReported* event) {
    return event->mutable_dns_query_events()->add_dns_query_event();
}

static bool isNetworkRestricted(int terrno) {
    // It's possible that system was in some network restricted mode, which blocked
    // the operation of sending packet and resulted in EPERM errno.
    // It would be no reason to keep retrying on that case.
    // TODO: Check the system status to know if network restricted mode is
    // enabled.
    return (terrno == EPERM);
}

int res_nsend(res_state statp, const uint8_t* buf, int buflen, uint8_t* ans, int anssiz, int* rcode,
              uint32_t flags, std::chrono::milliseconds sleepTimeMs) {
    LOG(DEBUG) << __func__;

    // Should not happen
    if (anssiz < HFIXEDSZ) {
        // TODO: Remove errno once callers stop using it
        errno = EINVAL;
        return -EINVAL;
    }
    res_pquery(buf, buflen);

    int anslen = 0;
    Stopwatch cacheStopwatch;
    ResolvCacheStatus cache_status =
            resolv_cache_lookup(statp->netid, buf, buflen, ans, anssiz, &anslen, flags);
    const int32_t cacheLatencyUs = saturate_cast<int32_t>(cacheStopwatch.timeTakenUs());
    if (cache_status == RESOLV_CACHE_FOUND) {
        HEADER* hp = (HEADER*)(void*)ans;
        *rcode = hp->rcode;
        DnsQueryEvent* dnsQueryEvent = addDnsQueryEvent(statp->event);
        dnsQueryEvent->set_latency_micros(cacheLatencyUs);
        dnsQueryEvent->set_cache_hit(static_cast<CacheStatus>(cache_status));
        dnsQueryEvent->set_type(getQueryType(buf, buflen));
        return anslen;
    } else if (cache_status != RESOLV_CACHE_UNSUPPORTED) {
        // had a cache miss for a known network, so populate the thread private
        // data so the normal resolve path can do its thing
        resolv_populate_res_for_net(statp);
    }
    if (statp->nameserverCount() == 0) {
        // We have no nameservers configured, so there's no point trying.
        // Tell the cache the query failed, or any retries and anyone else asking the same
        // question will block for PENDING_REQUEST_TIMEOUT seconds instead of failing fast.
        _resolv_cache_query_failed(statp->netid, buf, buflen, flags);

        // TODO: Remove errno once callers stop using it
        errno = ESRCH;
        return -ESRCH;
    }

    // If parallel_lookup is enabled, it might be required to wait some time to avoid
    // gateways drop packets if queries are sent too close together
    if (sleepTimeMs != 0ms) {
        std::this_thread::sleep_for(sleepTimeMs);
    }
    // DoT
    if (!(statp->netcontext_flags & NET_CONTEXT_FLAG_USE_LOCAL_NAMESERVERS)) {
        bool fallback = false;
        int resplen = res_tls_send(statp, Slice(const_cast<uint8_t*>(buf), buflen),
                                   Slice(ans, anssiz), rcode, &fallback);
        if (resplen > 0) {
            LOG(DEBUG) << __func__ << ": got answer from DoT";
            res_pquery(ans, resplen);
            if (cache_status == RESOLV_CACHE_NOTFOUND) {
                resolv_cache_add(statp->netid, buf, buflen, ans, resplen);
            }
            return resplen;
        }
        if (!fallback) {
            _resolv_cache_query_failed(statp->netid, buf, buflen, flags);
            return -ETIMEDOUT;
        }
    }

    res_stats stats[MAXNS]{};
    res_params params;
    int revision_id = resolv_cache_get_resolver_stats(statp->netid, &params, stats, statp->nsaddrs);
    if (revision_id < 0) {
        // TODO: Remove errno once callers stop using it
        errno = ESRCH;
        return -ESRCH;
    }
    bool usable_servers[MAXNS];
    int usableServersCount = android_net_res_stats_get_usable_servers(
            &params, stats, statp->nameserverCount(), usable_servers);

    if ((flags & ANDROID_RESOLV_NO_RETRY) && usableServersCount > 1) {
        auto hp = reinterpret_cast<const HEADER*>(buf);

        // Select a random server based on the query id
        int selectedServer = (hp->id % usableServersCount) + 1;
        res_set_usable_server(selectedServer, statp->nameserverCount(), usable_servers);
    }

    // Send request, RETRY times, or until successful.
    int retryTimes = (flags & ANDROID_RESOLV_NO_RETRY) ? 1 : params.retry_count;
    int useTcp = buflen > PACKETSZ;
    int gotsomewhere = 0;
    // Use an impossible error code as default value
    int terrno = ETIME;

    for (int attempt = 0; attempt < retryTimes; ++attempt) {
        for (size_t ns = 0; ns < statp->nsaddrs.size(); ++ns) {
            if (!usable_servers[ns]) continue;

            *rcode = RCODE_INTERNAL_ERROR;

            // Get server addr
            const IPSockAddr& serverSockAddr = statp->nsaddrs[ns];
            LOG(DEBUG) << __func__ << ": Querying server (# " << ns + 1
                       << ") address = " << serverSockAddr.toString();

            ::android::net::Protocol query_proto = useTcp ? PROTO_TCP : PROTO_UDP;
            time_t query_time = 0;
            int delay = 0;
            bool fallbackTCP = false;
            const bool shouldRecordStats = (attempt == 0);
            int resplen;
            Stopwatch queryStopwatch;
            int retry_count_for_event = 0;
            size_t actualNs = ns;
            // Use an impossible error code as default value
            terrno = ETIME;
            if (useTcp) {
                // TCP; at most one attempt per server.
                attempt = retryTimes;
                resplen = send_vc(statp, &params, buf, buflen, ans, anssiz, &terrno, ns,
                                  &query_time, rcode, &delay);

                if (buflen <= PACKETSZ && resplen <= 0 &&
                    statp->tc_mode == aidl::android::net::IDnsResolver::TC_MODE_UDP_TCP) {
                    // reset to UDP for next query on next DNS server if resolver is currently doing
                    // TCP fallback retry and current server does not support TCP connectin
                    useTcp = false;
                }
                LOG(INFO) << __func__ << ": used send_vc " << resplen << " terrno: " << terrno;
            } else {
                // UDP
                resplen = send_dg(statp, &params, buf, buflen, ans, anssiz, &terrno, &actualNs,
                                  &useTcp, &gotsomewhere, &query_time, rcode, &delay);
                fallbackTCP = useTcp ? true : false;
                retry_count_for_event = attempt;
                LOG(INFO) << __func__ << ": used send_dg " << resplen << " terrno: " << terrno;
            }

            const IPSockAddr& receivedServerAddr = statp->nsaddrs[actualNs];
            DnsQueryEvent* dnsQueryEvent = addDnsQueryEvent(statp->event);
            dnsQueryEvent->set_cache_hit(static_cast<CacheStatus>(cache_status));
            // When |retryTimes| > 1, we cannot actually know the correct latency value if we
            // received the answer from the previous server. So temporarily set the latency as -1 if
            // that condition happened.
            // TODO: make the latency value accurate.
            dnsQueryEvent->set_latency_micros(
                    (actualNs == ns) ? saturate_cast<int32_t>(queryStopwatch.timeTakenUs()) : -1);
            dnsQueryEvent->set_dns_server_index(actualNs);
            dnsQueryEvent->set_ip_version(ipFamilyToIPVersion(receivedServerAddr.family()));
            dnsQueryEvent->set_retry_times(retry_count_for_event);
            dnsQueryEvent->set_rcode(static_cast<NsRcode>(*rcode));
            dnsQueryEvent->set_protocol(query_proto);
            dnsQueryEvent->set_type(getQueryType(buf, buflen));
            dnsQueryEvent->set_linux_errno(static_cast<LinuxErrno>(terrno));

            // Only record stats the first time we try a query. This ensures that
            // queries that deterministically fail (e.g., a name that always returns
            // SERVFAIL or times out) do not unduly affect the stats.
            if (shouldRecordStats) {
                // (b/151166599): This is a workaround to prevent that DnsResolver calculates the
                // reliability of DNS servers from being broken when network restricted mode is
                // enabled.
                // TODO: Introduce the new server selection instead of skipping stats recording.
                if (!isNetworkRestricted(terrno)) {
                    res_sample sample;
                    res_stats_set_sample(&sample, query_time, *rcode, delay);
                    // KeepListening UDP mechanism is incompatible with usable_servers of legacy
                    // stats, so keep the old logic for now.
                    // TODO: Replace usable_servers of legacy stats with new one.
                    resolv_cache_add_resolver_stats_sample(
                            statp->netid, revision_id, serverSockAddr, sample, params.max_samples);
                }
                resolv_stats_add(statp->netid, receivedServerAddr, dnsQueryEvent);
            }

            if (resplen == 0) continue;
            if (fallbackTCP) {
                ns--;
                continue;
            }
            if (resplen < 0) {
                _resolv_cache_query_failed(statp->netid, buf, buflen, flags);
                statp->closeSockets();
                return -terrno;
            };

            LOG(DEBUG) << __func__ << ": got answer:";
            res_pquery(ans, (resplen > anssiz) ? anssiz : resplen);

            if (cache_status == RESOLV_CACHE_NOTFOUND) {
                resolv_cache_add(statp->netid, buf, buflen, ans, resplen);
            }
            statp->closeSockets();
            return (resplen);
        }  // for each ns
    }  // for each retry
    statp->closeSockets();
    terrno = useTcp ? terrno : gotsomewhere ? ETIMEDOUT : ECONNREFUSED;
    // TODO: Remove errno once callers stop using it
    errno = useTcp ? terrno
                   : gotsomewhere ? ETIMEDOUT /* no answer obtained */
                                  : ECONNREFUSED /* no nameservers found */;

    _resolv_cache_query_failed(statp->netid, buf, buflen, flags);
    return -terrno;
}

static struct timespec get_timeout(res_state statp, const res_params* params, const int ns) {
    int msec;
    // Legacy algorithm which scales the timeout by nameserver number.
    // For instance, with 4 nameservers: 5s, 2.5s, 5s, 10s
    // This has no effect with 1 or 2 nameservers
    msec = params->base_timeout_msec << ns;
    if (ns > 0) {
        msec /= statp->nameserverCount();
    }
    // For safety, don't allow OEMs and experiments to configure a timeout shorter than 1s.
    if (msec < 1000) {
        msec = 1000;  // Use at least 1000ms
    }
    LOG(INFO) << __func__ << ": using timeout of " << msec << " msec";

    struct timespec result;
    result.tv_sec = msec / 1000;
    result.tv_nsec = (msec % 1000) * 1000000;
    return result;
}

static int send_vc(res_state statp, res_params* params, const uint8_t* buf, int buflen,
                   uint8_t* ans, int anssiz, int* terrno, size_t ns, time_t* at, int* rcode,
                   int* delay) {
    *at = time(NULL);
    *delay = 0;
    const HEADER* hp = (const HEADER*) (const void*) buf;
    HEADER* anhp = (HEADER*) (void*) ans;
    struct sockaddr* nsap;
    int nsaplen;
    int truncating, connreset, n;
    uint8_t* cp;

    LOG(INFO) << __func__ << ": using send_vc";

    // It should never happen, but just in case.
    if (ns >= statp->nsaddrs.size()) {
        LOG(ERROR) << __func__ << ": Out-of-bound indexing: " << ns;
        *terrno = EINVAL;
        return -1;
    }

    sockaddr_storage ss = statp->nsaddrs[ns];
    nsap = reinterpret_cast<sockaddr*>(&ss);
    nsaplen = sockaddrSize(nsap);

    connreset = 0;
same_ns:
    truncating = 0;

    struct timespec start_time = evNowTime();

    /* Are we still talking to whom we want to talk to? */
    if (statp->tcp_nssock >= 0 && (statp->_flags & RES_F_VC) != 0) {
        struct sockaddr_storage peer;
        socklen_t size = sizeof peer;
        unsigned old_mark;
        socklen_t mark_size = sizeof(old_mark);
        if (getpeername(statp->tcp_nssock, (struct sockaddr*)(void*)&peer, &size) < 0 ||
            !sock_eq((struct sockaddr*)(void*)&peer, nsap) ||
            getsockopt(statp->tcp_nssock, SOL_SOCKET, SO_MARK, &old_mark, &mark_size) < 0 ||
            old_mark != statp->_mark) {
            statp->closeSockets();
        }
    }

    if (statp->tcp_nssock < 0 || (statp->_flags & RES_F_VC) == 0) {
        if (statp->tcp_nssock >= 0) statp->closeSockets();

        statp->tcp_nssock.reset(socket(nsap->sa_family, SOCK_STREAM | SOCK_CLOEXEC, 0));
        if (statp->tcp_nssock < 0) {
            *terrno = errno;
            PLOG(DEBUG) << __func__ << ": socket(vc): ";
            switch (errno) {
                case EPROTONOSUPPORT:
                case EPFNOSUPPORT:
                case EAFNOSUPPORT:
                    return 0;
                default:
                    return -1;
            }
        }
        const uid_t uid = statp->enforce_dns_uid ? AID_DNS : statp->uid;
        resolv_tag_socket(statp->tcp_nssock, uid, statp->pid);
        if (statp->_mark != MARK_UNSET) {
            if (setsockopt(statp->tcp_nssock, SOL_SOCKET, SO_MARK, &statp->_mark,
                           sizeof(statp->_mark)) < 0) {
                *terrno = errno;
                PLOG(DEBUG) << __func__ << ": setsockopt: ";
                return -1;
            }
        }
        errno = 0;
        if (random_bind(statp->tcp_nssock, nsap->sa_family) < 0) {
            *terrno = errno;
            dump_error("bind/vc", nsap, nsaplen);
            statp->closeSockets();
            return (0);
        }
        if (connect_with_timeout(statp->tcp_nssock, nsap, (socklen_t)nsaplen,
                                 get_timeout(statp, params, ns)) < 0) {
            *terrno = errno;
            dump_error("connect/vc", nsap, nsaplen);
            statp->closeSockets();
            /*
             * The way connect_with_timeout() is implemented prevents us from reliably
             * determining whether this was really a timeout or e.g. ECONNREFUSED. Since
             * currently both cases are handled in the same way, there is no need to
             * change this (yet). If we ever need to reliably distinguish between these
             * cases, both connect_with_timeout() and retrying_poll() need to be
             * modified, though.
             */
            *rcode = RCODE_TIMEOUT;
            return (0);
        }
        statp->_flags |= RES_F_VC;
    }

    /*
     * Send length & message
     */
    uint16_t len = htons(static_cast<uint16_t>(buflen));
    const iovec iov[] = {
            {.iov_base = &len, .iov_len = INT16SZ},
            {.iov_base = const_cast<uint8_t*>(buf), .iov_len = static_cast<size_t>(buflen)},
    };
    if (writev(statp->tcp_nssock, iov, 2) != (INT16SZ + buflen)) {
        *terrno = errno;
        PLOG(DEBUG) << __func__ << ": write failed: ";
        statp->closeSockets();
        return (0);
    }
    /*
     * Receive length & response
     */
read_len:
    cp = ans;
    len = INT16SZ;
    while ((n = read(statp->tcp_nssock, (char*)cp, (size_t)len)) > 0) {
        cp += n;
        if ((len -= n) == 0) break;
    }
    if (n <= 0) {
        *terrno = errno;
        PLOG(DEBUG) << __func__ << ": read failed: ";
        statp->closeSockets();
        /*
         * A long running process might get its TCP
         * connection reset if the remote server was
         * restarted.  Requery the server instead of
         * trying a new one.  When there is only one
         * server, this means that a query might work
         * instead of failing.  We only allow one reset
         * per query to prevent looping.
         */
        if (*terrno == ECONNRESET && !connreset) {
            connreset = 1;
            goto same_ns;
        }
        return (0);
    }
    uint16_t resplen = ntohs(*reinterpret_cast<const uint16_t*>(ans));
    if (resplen > anssiz) {
        LOG(DEBUG) << __func__ << ": response truncated";
        truncating = 1;
        len = anssiz;
    } else
        len = resplen;
    if (len < HFIXEDSZ) {
        /*
         * Undersized message.
         */
        LOG(DEBUG) << __func__ << ": undersized: " << len;
        *terrno = EMSGSIZE;
        statp->closeSockets();
        return (0);
    }
    cp = ans;
    while (len != 0 && (n = read(statp->tcp_nssock, (char*)cp, (size_t)len)) > 0) {
        cp += n;
        len -= n;
    }
    if (n <= 0) {
        *terrno = errno;
        PLOG(DEBUG) << __func__ << ": read(vc): ";
        statp->closeSockets();
        return (0);
    }

    if (truncating) {
        /*
         * Flush rest of answer so connection stays in synch.
         */
        anhp->tc = 1;
        len = resplen - anssiz;
        while (len != 0) {
            char junk[PACKETSZ];

            n = read(statp->tcp_nssock, junk, (len > sizeof junk) ? sizeof junk : len);
            if (n > 0)
                len -= n;
            else
                break;
        }
        LOG(WARNING) << __func__ << ": resplen " << resplen << " exceeds buf size " << anssiz;
        // return size should never exceed container size
        resplen = anssiz;
    }
    /*
     * If the calling application has bailed out of
     * a previous call and failed to arrange to have
     * the circuit closed or the server has got
     * itself confused, then drop the packet and
     * wait for the correct one.
     */
    if (hp->id != anhp->id) {
        LOG(DEBUG) << __func__ << ": ld answer (unexpected):";
        res_pquery(ans, resplen);
        goto read_len;
    }

    /*
     * All is well, or the error is fatal.  Signal that the
     * next nameserver ought not be tried.
     */
    if (resplen > 0) {
        struct timespec done = evNowTime();
        *delay = res_stats_calculate_rtt(&done, &start_time);
        *rcode = anhp->rcode;
    }
    *terrno = 0;
    return (resplen);
}

/* return -1 on error (errno set), 0 on success */
static int connect_with_timeout(int sock, const sockaddr* nsap, socklen_t salen,
                                const timespec timeout) {
    int res, origflags;

    origflags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, origflags | O_NONBLOCK);

    res = connect(sock, nsap, salen);
    if (res < 0 && errno != EINPROGRESS) {
        res = -1;
        goto done;
    }
    if (res != 0) {
        timespec now = evNowTime();
        timespec finish = evAddTime(now, timeout);
        LOG(INFO) << __func__ << ": " << sock << " send_vc";
        res = retrying_poll(sock, POLLIN | POLLOUT, &finish);
        if (res <= 0) {
            res = -1;
        }
    }
done:
    fcntl(sock, F_SETFL, origflags);
    LOG(INFO) << __func__ << ": " << sock << " connect_with_const timeout returning " << res;
    return res;
}

static int retrying_poll(const int sock, const short events, const struct timespec* finish) {
    struct timespec now, timeout;

retry:
    LOG(INFO) << __func__ << ": " << sock << " retrying_poll";

    now = evNowTime();
    if (evCmpTime(*finish, now) > 0)
        timeout = evSubTime(*finish, now);
    else
        timeout = evConsTime(0L, 0L);
    struct pollfd fds = {.fd = sock, .events = events};
    int n = ppoll(&fds, 1, &timeout, /*__mask=*/NULL);
    if (n == 0) {
        LOG(INFO) << __func__ << ": " << sock << " retrying_poll timeout";
        errno = ETIMEDOUT;
        return 0;
    }
    if (n < 0) {
        if (errno == EINTR) goto retry;
        PLOG(INFO) << __func__ << ": " << sock << " retrying_poll failed";
        return n;
    }
    if (fds.revents & (POLLIN | POLLOUT | POLLERR)) {
        int error;
        socklen_t len = sizeof(error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error) {
            errno = error;
            PLOG(INFO) << __func__ << ": " << sock << " retrying_poll getsockopt failed";
            return -1;
        }
    }
    LOG(INFO) << __func__ << ": " << sock << " retrying_poll returning " << n;
    return n;
}

static std::vector<pollfd> extractUdpFdset(res_state statp, const short events = POLLIN) {
    std::vector<pollfd> fdset(statp->nsaddrs.size());
    for (size_t i = 0; i < statp->nsaddrs.size(); ++i) {
        fdset[i] = {.fd = statp->nssocks[i], .events = events};
    }
    return fdset;
}

static Result<std::vector<int>> udpRetryingPoll(res_state statp, const timespec* finish) {
    for (;;) {
        LOG(DEBUG) << __func__ << ": poll";
        timespec start_time = evNowTime();
        timespec timeout = (evCmpTime(*finish, start_time) > 0) ? evSubTime(*finish, start_time)
                                                                : evConsTime(0L, 0L);
        std::vector<pollfd> fdset = extractUdpFdset(statp);
        const int n = ppoll(fdset.data(), fdset.size(), &timeout, /*__mask=*/nullptr);
        if (n <= 0) {
            if (errno == EINTR && n < 0) continue;
            if (n == 0) errno = ETIMEDOUT;
            PLOG(INFO) << __func__ << ": failed";
            return ErrnoError();
        }
        std::vector<int> fdsToRead;
        for (const auto& pollfd : fdset) {
            if (pollfd.revents & (POLLIN | POLLERR)) {
                fdsToRead.push_back(pollfd.fd);
            }
        }
        LOG(DEBUG) << __func__ << ": "
                   << " returning fd size: " << fdsToRead.size();
        return fdsToRead;
    }
}

static Result<std::vector<int>> udpRetryingPollWrapper(res_state statp, int ns,
                                                       const timespec* finish) {
    const bool keepListeningUdp =
            android::net::Experiments::getInstance()->getFlag("keep_listening_udp", 0);
    if (keepListeningUdp) return udpRetryingPoll(statp, finish);

    if (int n = retrying_poll(statp->nssocks[ns], POLLIN, finish); n <= 0) {
        return ErrnoError();
    }
    return std::vector<int>{statp->nssocks[ns]};
}

bool ignoreInvalidAnswer(res_state statp, const sockaddr_storage& from, const uint8_t* buf,
                         int buflen, uint8_t* ans, int anssiz, int* receivedFromNs) {
    const HEADER* hp = (const HEADER*)(const void*)buf;
    HEADER* anhp = (HEADER*)(void*)ans;
    if (hp->id != anhp->id) {
        // response from old query, ignore it.
        LOG(DEBUG) << __func__ << ": old answer:";
        return true;
    }
    if (*receivedFromNs = res_ourserver_p(statp, (sockaddr*)(void*)&from); *receivedFromNs < 0) {
        // response from wrong server? ignore it.
        LOG(DEBUG) << __func__ << ": not our server:";
        return true;
    }
    if (!res_queriesmatch(buf, buf + buflen, ans, ans + anssiz)) {
        // response contains wrong query? ignore it.
        LOG(DEBUG) << __func__ << ": wrong query name:";
        return true;
    }
    return false;
}

static int send_dg(res_state statp, res_params* params, const uint8_t* buf, int buflen,
                   uint8_t* ans, int anssiz, int* terrno, size_t* ns, int* v_circuit,
                   int* gotsomewhere, time_t* at, int* rcode, int* delay) {
    // It should never happen, but just in case.
    if (*ns >= statp->nsaddrs.size()) {
        LOG(ERROR) << __func__ << ": Out-of-bound indexing: " << ns;
        *terrno = EINVAL;
        return -1;
    }

    *at = time(nullptr);
    *delay = 0;
    const sockaddr_storage ss = statp->nsaddrs[*ns];
    const sockaddr* nsap = reinterpret_cast<const sockaddr*>(&ss);
    const int nsaplen = sockaddrSize(nsap);

    if (statp->nssocks[*ns] == -1) {
        statp->nssocks[*ns].reset(socket(nsap->sa_family, SOCK_DGRAM | SOCK_CLOEXEC, 0));
        if (statp->nssocks[*ns] < 0) {
            *terrno = errno;
            PLOG(DEBUG) << __func__ << ": socket(dg): ";
            switch (errno) {
                case EPROTONOSUPPORT:
                case EPFNOSUPPORT:
                case EAFNOSUPPORT:
                    return (0);
                default:
                    return (-1);
            }
        }

        const uid_t uid = statp->enforce_dns_uid ? AID_DNS : statp->uid;
        resolv_tag_socket(statp->nssocks[*ns], uid, statp->pid);
        if (statp->_mark != MARK_UNSET) {
            if (setsockopt(statp->nssocks[*ns], SOL_SOCKET, SO_MARK, &(statp->_mark),
                           sizeof(statp->_mark)) < 0) {
                *terrno = errno;
                statp->closeSockets();
                return -1;
            }
        }
        // Use a "connected" datagram socket to receive an ECONNREFUSED error
        // on the next socket operation when the server responds with an
        // ICMP port-unreachable error. This way we can detect the absence of
        // a nameserver without timing out.
        if (random_bind(statp->nssocks[*ns], nsap->sa_family) < 0) {
            *terrno = errno;
            dump_error("bind(dg)", nsap, nsaplen);
            statp->closeSockets();
            return (0);
        }
        if (connect(statp->nssocks[*ns], nsap, (socklen_t)nsaplen) < 0) {
            *terrno = errno;
            dump_error("connect(dg)", nsap, nsaplen);
            statp->closeSockets();
            return (0);
        }
        LOG(DEBUG) << __func__ << ": new DG socket";
    }
    if (send(statp->nssocks[*ns], (const char*)buf, (size_t)buflen, 0) != buflen) {
        *terrno = errno;
        PLOG(DEBUG) << __func__ << ": send: ";
        statp->closeSockets();
        return 0;
    }

    timespec timeout = get_timeout(statp, params, *ns);
    timespec start_time = evNowTime();
    timespec finish = evAddTime(start_time, timeout);
    for (;;) {
        // Wait for reply.
        auto result = udpRetryingPollWrapper(statp, *ns, &finish);

        if (!result.has_value()) {
            const bool isTimeout = (result.error().code() == ETIMEDOUT);
            *rcode = (isTimeout) ? RCODE_TIMEOUT : *rcode;
            *terrno = (isTimeout) ? ETIMEDOUT : errno;
            *gotsomewhere = (isTimeout) ? 1 : *gotsomewhere;
            // Leave the UDP sockets open on timeout so we can keep listening for
            // a late response from this server while retrying on the next server.
            if (!isTimeout) statp->closeSockets();
            LOG(DEBUG) << __func__ << ": " << (isTimeout) ? "timeout" : "poll";
            return 0;
        }
        bool needRetry = false;
        for (int fd : result.value()) {
            needRetry = false;
            sockaddr_storage from;
            socklen_t fromlen = sizeof(from);
            int resplen =
                    recvfrom(fd, (char*)ans, (size_t)anssiz, 0, (sockaddr*)(void*)&from, &fromlen);
            if (resplen <= 0) {
                *terrno = errno;
                PLOG(DEBUG) << __func__ << ": recvfrom: ";
                continue;
            }
            *gotsomewhere = 1;
            if (resplen < HFIXEDSZ) {
                // Undersized message.
                LOG(DEBUG) << __func__ << ": undersized: " << resplen;
                *terrno = EMSGSIZE;
                continue;
            }

            int receivedFromNs = *ns;
            if (needRetry =
                        ignoreInvalidAnswer(statp, from, buf, buflen, ans, anssiz, &receivedFromNs);
                needRetry) {
                res_pquery(ans, (resplen > anssiz) ? anssiz : resplen);
                continue;
            }

            HEADER* anhp = (HEADER*)(void*)ans;
            if (anhp->rcode == FORMERR && (statp->netcontext_flags & NET_CONTEXT_FLAG_USE_EDNS)) {
                //  Do not retry if the server do not understand EDNS0.
                //  The case has to be captured here, as FORMERR packet do not
                //  carry query section, hence res_queriesmatch() returns 0.
                LOG(DEBUG) << __func__ << ": server rejected query with EDNS0:";
                res_pquery(ans, (resplen > anssiz) ? anssiz : resplen);
                // record the error
                statp->_flags |= RES_F_EDNS0ERR;
                *terrno = EREMOTEIO;
                continue;
            }

            timespec done = evNowTime();
            *delay = res_stats_calculate_rtt(&done, &start_time);
            if (anhp->rcode == SERVFAIL || anhp->rcode == NOTIMP || anhp->rcode == REFUSED) {
                LOG(DEBUG) << __func__ << ": server rejected query:";
                res_pquery(ans, (resplen > anssiz) ? anssiz : resplen);
                *rcode = anhp->rcode;
                continue;
            }
            if (anhp->tc) {
                // To get the rest of answer,
                // use TCP with same server.
                LOG(DEBUG) << __func__ << ": truncated answer";
                *terrno = E2BIG;
                *v_circuit = 1;
                return 1;
            }
            // All is well, or the error is fatal. Signal that the
            // next nameserver ought not be tried.

            *rcode = anhp->rcode;
            *ns = receivedFromNs;
            *terrno = 0;
            return resplen;
        }
        if (!needRetry) return 0;
    }
}

static void dump_error(const char* str, const struct sockaddr* address, int alen) {
    char hbuf[NI_MAXHOST];
    char sbuf[NI_MAXSERV];
    constexpr int niflags = NI_NUMERICHOST | NI_NUMERICSERV;
    const int err = errno;

    if (!WOULD_LOG(DEBUG)) return;

    if (getnameinfo(address, (socklen_t)alen, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), niflags)) {
        strncpy(hbuf, "?", sizeof(hbuf) - 1);
        hbuf[sizeof(hbuf) - 1] = '\0';
        strncpy(sbuf, "?", sizeof(sbuf) - 1);
        sbuf[sizeof(sbuf) - 1] = '\0';
    }
    errno = err;
    PLOG(DEBUG) << __func__ << ": " << str << " ([" << hbuf << "]." << sbuf << "): ";
}

static int sock_eq(struct sockaddr* a, struct sockaddr* b) {
    struct sockaddr_in *a4, *b4;
    struct sockaddr_in6 *a6, *b6;

    if (a->sa_family != b->sa_family) return 0;
    switch (a->sa_family) {
        case AF_INET:
            a4 = (struct sockaddr_in*) (void*) a;
            b4 = (struct sockaddr_in*) (void*) b;
            return a4->sin_port == b4->sin_port && a4->sin_addr.s_addr == b4->sin_addr.s_addr;
        case AF_INET6:
            a6 = (struct sockaddr_in6*) (void*) a;
            b6 = (struct sockaddr_in6*) (void*) b;
            return a6->sin6_port == b6->sin6_port &&
#ifdef HAVE_SIN6_SCOPE_ID
                   a6->sin6_scope_id == b6->sin6_scope_id &&
#endif
                   IN6_ARE_ADDR_EQUAL(&a6->sin6_addr, &b6->sin6_addr);
        default:
            return 0;
    }
}

PrivateDnsModes convertEnumType(PrivateDnsMode privateDnsmode) {
    switch (privateDnsmode) {
        case PrivateDnsMode::OFF:
            return PrivateDnsModes::PDM_OFF;
        case PrivateDnsMode::OPPORTUNISTIC:
            return PrivateDnsModes::PDM_OPPORTUNISTIC;
        case PrivateDnsMode::STRICT:
            return PrivateDnsModes::PDM_STRICT;
        default:
            return PrivateDnsModes::PDM_UNKNOWN;
    }
}

static int res_tls_send(res_state statp, const Slice query, const Slice answer, int* rcode,
                        bool* fallback) {
    int resplen = 0;
    const unsigned netId = statp->netid;

    PrivateDnsStatus privateDnsStatus = gPrivateDnsConfiguration.getStatus(netId);
    statp->event->set_private_dns_modes(convertEnumType(privateDnsStatus.mode));

    if (privateDnsStatus.mode == PrivateDnsMode::OFF) {
        *fallback = true;
        return -1;
    }

    if (privateDnsStatus.validatedServers().empty()) {
        if (privateDnsStatus.mode == PrivateDnsMode::OPPORTUNISTIC) {
            *fallback = true;
            return -1;
        } else {
            // Sleep and iterate some small number of times checking for the
            // arrival of resolved and validated server IP addresses, instead
            // of returning an immediate error.
            // This is needed because as soon as a network becomes the default network, apps will
            // send DNS queries on that network. If no servers have yet validated, and we do not
            // block those queries, they would immediately fail, causing application-visible errors.
            // Note that this can happen even before the network validates, since an unvalidated
            // network can become the default network if no validated networks are available.
            //
            // TODO: see if there is a better way to address this problem, such as buffering the
            // queries in a queue or only blocking queries for the first few seconds after a default
            // network change.
            for (int i = 0; i < 42; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                // Calling getStatus() to merely check if there's any validated server seems
                // wasteful. Consider adding a new method in PrivateDnsConfiguration for speed ups.
                if (!gPrivateDnsConfiguration.getStatus(netId).validatedServers().empty()) {
                    privateDnsStatus = gPrivateDnsConfiguration.getStatus(netId);
                    break;
                }
            }
            if (privateDnsStatus.validatedServers().empty()) {
                return -1;
            }
        }
    }

    LOG(INFO) << __func__ << ": performing query over TLS";

    const auto response = sDnsTlsDispatcher.query(privateDnsStatus.validatedServers(), statp, query,
                                                  answer, &resplen);

    LOG(INFO) << __func__ << ": TLS query result: " << static_cast<int>(response);

    if (privateDnsStatus.mode == PrivateDnsMode::OPPORTUNISTIC) {
        // In opportunistic mode, handle falling back to cleartext in some
        // cases (DNS shouldn't fail if a validated opportunistic mode server
        // becomes unreachable for some reason).
        switch (response) {
            case DnsTlsTransport::Response::success:
                *rcode = reinterpret_cast<HEADER*>(answer.base())->rcode;
                return resplen;
            case DnsTlsTransport::Response::network_error:
                // No need to set the error timeout here since it will fallback to UDP.
            case DnsTlsTransport::Response::internal_error:
                // Note: this will cause cleartext queries to be emitted, with
                // all of the EDNS0 goodness enabled. Fingers crossed.  :-/
                *fallback = true;
                [[fallthrough]];
            default:
                return -1;
        }
    } else {
        // Strict mode
        switch (response) {
            case DnsTlsTransport::Response::success:
                *rcode = reinterpret_cast<HEADER*>(answer.base())->rcode;
                return resplen;
            case DnsTlsTransport::Response::network_error:
                // This case happens when the query stored in DnsTlsTransport is expired since
                // either 1) the query has been tried for 3 times but no response or 2) fail to
                // establish the connection with the server.
                *rcode = RCODE_TIMEOUT;
                [[fallthrough]];
            default:
                return -1;
        }
    }
}

int resolv_res_nsend(const android_net_context* netContext, const uint8_t* msg, int msgLen,
                     uint8_t* ans, int ansLen, int* rcode, uint32_t flags,
                     NetworkDnsEventReported* event) {
    assert(event != nullptr);
    ResState res;
    res_init(&res, netContext, event);
    resolv_populate_res_for_net(&res);
    *rcode = NOERROR;
    return res_nsend(&res, msg, msgLen, ans, ansLen, rcode, flags);
}
