/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define LOG_TAG "resolv"

#include "resolv_cache.h"

#include <resolv.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <algorithm>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <errno.h>
#include <linux/if.h>
#include <net/if.h>
#include <netdb.h>

#include <aidl/android/net/IDnsResolver.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android-base/thread_annotations.h>
#include <android/multinetwork.h>  // ResNsendFlags

#include <server_configurable_flags/get_flags.h>

#include "DnsStats.h"
#include "res_comp.h"
#include "res_debug.h"
#include "resolv_private.h"
#include "util.h"

using aidl::android::net::IDnsResolver;
using android::base::StringAppendF;
using android::net::DnsQueryEvent;
using android::net::DnsStats;
using android::net::PROTO_DOT;
using android::net::PROTO_TCP;
using android::net::PROTO_UDP;
using android::netdutils::DumpWriter;
using android::netdutils::IPSockAddr;

/* This code implements a small and *simple* DNS resolver cache.
 *
 * It is only used to cache DNS answers for a time defined by the smallest TTL
 * among the answer records in order to reduce DNS traffic. It is not supposed
 * to be a full DNS cache, since we plan to implement that in the future in a
 * dedicated process running on the system.
 *
 * Note that its design is kept simple very intentionally, i.e.:
 *
 *  - it takes raw DNS query packet data as input, and returns raw DNS
 *    answer packet data as output
 *
 *    (this means that two similar queries that encode the DNS name
 *     differently will be treated distinctly).
 *
 *    the smallest TTL value among the answer records are used as the time
 *    to keep an answer in the cache.
 *
 *    this is bad, but we absolutely want to avoid parsing the answer packets
 *    (and should be solved by the later full DNS cache process).
 *
 *  - the implementation is just a (query-data) => (answer-data) hash table
 *    with a trivial least-recently-used expiration policy.
 *
 * Doing this keeps the code simple and avoids to deal with a lot of things
 * that a full DNS cache is expected to do.
 *
 * The API is also very simple:
 *
 *   - the client calls resolv_cache_lookup() before performing a query
 *
 *     If the function returns RESOLV_CACHE_FOUND, a copy of the answer data
 *     has been copied into the client-provided answer buffer.
 *
 *     If the function returns RESOLV_CACHE_NOTFOUND, the client should perform
 *     a request normally, *then* call resolv_cache_add() to add the received
 *     answer to the cache.
 *
 *     If the function returns RESOLV_CACHE_UNSUPPORTED, the client should
 *     perform a request normally, and *not* call resolv_cache_add()
 *
 *     Note that RESOLV_CACHE_UNSUPPORTED is also returned if the answer buffer
 *     is too short to accomodate the cached result.
 */

/* Default number of entries kept in the cache. This value has been
 * determined by browsing through various sites and counting the number
 * of corresponding requests. Keep in mind that our framework is currently
 * performing two requests per name lookup (one for IPv4, the other for IPv6)
 *
 *    www.google.com      4
 *    www.ysearch.com     6
 *    www.amazon.com      8
 *    www.nytimes.com     22
 *    www.espn.com        28
 *    www.msn.com         28
 *    www.lemonde.fr      35
 *
 * (determined in 2009-2-17 from Paris, France, results may vary depending
 *  on location)
 *
 * most high-level websites use lots of media/ad servers with different names
 * but these are generally reused when browsing through the site.
 *
 * As such, a value of 64 should be relatively comfortable at the moment.
 *
 * ******************************************
 * * NOTE - this has changed.
 * * 1) we've added IPv6 support so each dns query results in 2 responses
 * * 2) we've made this a system-wide cache, so the cost is less (it's not
 * *    duplicated in each process) and the need is greater (more processes
 * *    making different requests).
 * * Upping by 2x for IPv6
 * * Upping by another 5x for the centralized nature
 * *****************************************
 */
const int CONFIG_MAX_ENTRIES = 64 * 2 * 5;
constexpr int DNSEVENT_SUBSAMPLING_MAP_DEFAULT_KEY = -1;

static time_t _time_now(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}

/* reminder: the general format of a DNS packet is the following:
 *
 *    HEADER  (12 bytes)
 *    QUESTION  (variable)
 *    ANSWER (variable)
 *    AUTHORITY (variable)
 *    ADDITIONNAL (variable)
 *
 * the HEADER is made of:
 *
 *   ID     : 16 : 16-bit unique query identification field
 *
 *   QR     :  1 : set to 0 for queries, and 1 for responses
 *   Opcode :  4 : set to 0 for queries
 *   AA     :  1 : set to 0 for queries
 *   TC     :  1 : truncation flag, will be set to 0 in queries
 *   RD     :  1 : recursion desired
 *
 *   RA     :  1 : recursion available (0 in queries)
 *   Z      :  3 : three reserved zero bits
 *   RCODE  :  4 : response code (always 0=NOERROR in queries)
 *
 *   QDCount: 16 : question count
 *   ANCount: 16 : Answer count (0 in queries)
 *   NSCount: 16: Authority Record count (0 in queries)
 *   ARCount: 16: Additionnal Record count (0 in queries)
 *
 * the QUESTION is made of QDCount Question Record (QRs)
 * the ANSWER is made of ANCount RRs
 * the AUTHORITY is made of NSCount RRs
 * the ADDITIONNAL is made of ARCount RRs
 *
 * Each Question Record (QR) is made of:
 *
 *   QNAME   : variable : Query DNS NAME
 *   TYPE    : 16       : type of query (A=1, PTR=12, MX=15, AAAA=28, ALL=255)
 *   CLASS   : 16       : class of query (IN=1)
 *
 * Each Resource Record (RR) is made of:
 *
 *   NAME    : variable : DNS NAME
 *   TYPE    : 16       : type of query (A=1, PTR=12, MX=15, AAAA=28, ALL=255)
 *   CLASS   : 16       : class of query (IN=1)
 *   TTL     : 32       : seconds to cache this RR (0=none)
 *   RDLENGTH: 16       : size of RDDATA in bytes
 *   RDDATA  : variable : RR data (depends on TYPE)
 *
 * Each QNAME contains a domain name encoded as a sequence of 'labels'
 * terminated by a zero. Each label has the following format:
 *
 *    LEN  : 8     : lenght of label (MUST be < 64)
 *    NAME : 8*LEN : label length (must exclude dots)
 *
 * A value of 0 in the encoding is interpreted as the 'root' domain and
 * terminates the encoding. So 'www.android.com' will be encoded as:
 *
 *   <3>www<7>android<3>com<0>
 *
 * Where <n> represents the byte with value 'n'
 *
 * Each NAME reflects the QNAME of the question, but has a slightly more
 * complex encoding in order to provide message compression. This is achieved
 * by using a 2-byte pointer, with format:
 *
 *    TYPE   : 2  : 0b11 to indicate a pointer, 0b01 and 0b10 are reserved
 *    OFFSET : 14 : offset to another part of the DNS packet
 *
 * The offset is relative to the start of the DNS packet and must point
 * A pointer terminates the encoding.
 *
 * The NAME can be encoded in one of the following formats:
 *
 *   - a sequence of simple labels terminated by 0 (like QNAMEs)
 *   - a single pointer
 *   - a sequence of simple labels terminated by a pointer
 *
 * A pointer shall always point to either a pointer of a sequence of
 * labels (which can themselves be terminated by either a 0 or a pointer)
 *
 * The expanded length of a given domain name should not exceed 255 bytes.
 *
 * NOTE: we don't parse the answer packets, so don't need to deal with NAME
 *       records, only QNAMEs.
 */

#define DNS_HEADER_SIZE 12

#define DNS_TYPE_A "\00\01"     /* big-endian decimal 1 */
#define DNS_TYPE_PTR "\00\014"  /* big-endian decimal 12 */
#define DNS_TYPE_MX "\00\017"   /* big-endian decimal 15 */
#define DNS_TYPE_AAAA "\00\034" /* big-endian decimal 28 */
#define DNS_TYPE_ALL "\00\0377" /* big-endian decimal 255 */

#define DNS_CLASS_IN "\00\01" /* big-endian decimal 1 */

struct DnsPacket {
    const uint8_t* base;
    const uint8_t* end;
    const uint8_t* cursor;
};

static void _dnsPacket_init(DnsPacket* packet, const uint8_t* buff, int bufflen) {
    packet->base = buff;
    packet->end = buff + bufflen;
    packet->cursor = buff;
}

static void _dnsPacket_rewind(DnsPacket* packet) {
    packet->cursor = packet->base;
}

static void _dnsPacket_skip(DnsPacket* packet, int count) {
    const uint8_t* p = packet->cursor + count;

    if (p > packet->end) p = packet->end;

    packet->cursor = p;
}

static int _dnsPacket_readInt16(DnsPacket* packet) {
    const uint8_t* p = packet->cursor;

    if (p + 2 > packet->end) return -1;

    packet->cursor = p + 2;
    return (p[0] << 8) | p[1];
}

/** QUERY CHECKING **/

/* check bytes in a dns packet. returns 1 on success, 0 on failure.
 * the cursor is only advanced in the case of success
 */
static int _dnsPacket_checkBytes(DnsPacket* packet, int numBytes, const void* bytes) {
    const uint8_t* p = packet->cursor;

    if (p + numBytes > packet->end) return 0;

    if (memcmp(p, bytes, numBytes) != 0) return 0;

    packet->cursor = p + numBytes;
    return 1;
}

/* parse and skip a given QNAME stored in a query packet,
 * from the current cursor position. returns 1 on success,
 * or 0 for malformed data.
 */
static int _dnsPacket_checkQName(DnsPacket* packet) {
    const uint8_t* p = packet->cursor;
    const uint8_t* end = packet->end;

    for (;;) {
        int c;

        if (p >= end) break;

        c = *p++;

        if (c == 0) {
            packet->cursor = p;
            return 1;
        }

        /* we don't expect label compression in QNAMEs */
        if (c >= 64) break;

        p += c;
        /* we rely on the bound check at the start
         * of the loop here */
    }
    /* malformed data */
    LOG(INFO) << __func__ << ": malformed QNAME";
    return 0;
}

/* parse and skip a given QR stored in a packet.
 * returns 1 on success, and 0 on failure
 */
static int _dnsPacket_checkQR(DnsPacket* packet) {
    if (!_dnsPacket_checkQName(packet)) return 0;

    /* TYPE must be one of the things we support */
    if (!_dnsPacket_checkBytes(packet, 2, DNS_TYPE_A) &&
        !_dnsPacket_checkBytes(packet, 2, DNS_TYPE_PTR) &&
        !_dnsPacket_checkBytes(packet, 2, DNS_TYPE_MX) &&
        !_dnsPacket_checkBytes(packet, 2, DNS_TYPE_AAAA) &&
        !_dnsPacket_checkBytes(packet, 2, DNS_TYPE_ALL)) {
        LOG(INFO) << __func__ << ": unsupported TYPE";
        return 0;
    }
    /* CLASS must be IN */
    if (!_dnsPacket_checkBytes(packet, 2, DNS_CLASS_IN)) {
        LOG(INFO) << __func__ << ": unsupported CLASS";
        return 0;
    }

    return 1;
}

/* check the header of a DNS Query packet, return 1 if it is one
 * type of query we can cache, or 0 otherwise
 */
static int _dnsPacket_checkQuery(DnsPacket* packet) {
    const uint8_t* p = packet->base;
    int qdCount, anCount, dnCount, arCount;

    if (p + DNS_HEADER_SIZE > packet->end) {
        LOG(INFO) << __func__ << ": query packet too small";
        return 0;
    }

    /* QR must be set to 0, opcode must be 0 and AA must be 0 */
    /* RA, Z, and RCODE must be 0 */
    if ((p[2] & 0xFC) != 0 || (p[3] & 0xCF) != 0) {
        LOG(INFO) << __func__ << ": query packet flags unsupported";
        return 0;
    }

    /* Note that we ignore the TC, RD, CD, and AD bits here for the
     * following reasons:
     *
     * - there is no point for a query packet sent to a server
     *   to have the TC bit set, but the implementation might
     *   set the bit in the query buffer for its own needs
     *   between a resolv_cache_lookup and a resolv_cache_add.
     *   We should not freak out if this is the case.
     *
     * - we consider that the result from a query might depend on
     *   the RD, AD, and CD bits, so these bits
     *   should be used to differentiate cached result.
     *
     *   this implies that these bits are checked when hashing or
     *   comparing query packets, but not TC
     */

    /* ANCOUNT, DNCOUNT and ARCOUNT must be 0 */
    qdCount = (p[4] << 8) | p[5];
    anCount = (p[6] << 8) | p[7];
    dnCount = (p[8] << 8) | p[9];
    arCount = (p[10] << 8) | p[11];

    if (anCount != 0 || dnCount != 0 || arCount > 1) {
        LOG(INFO) << __func__ << ": query packet contains non-query records";
        return 0;
    }

    if (qdCount == 0) {
        LOG(INFO) << __func__ << ": query packet doesn't contain query record";
        return 0;
    }

    /* Check QDCOUNT QRs */
    packet->cursor = p + DNS_HEADER_SIZE;

    for (; qdCount > 0; qdCount--)
        if (!_dnsPacket_checkQR(packet)) return 0;

    return 1;
}

/** QUERY HASHING SUPPORT
 **
 ** THE FOLLOWING CODE ASSUMES THAT THE INPUT PACKET HAS ALREADY
 ** BEEN SUCCESFULLY CHECKED.
 **/

/* use 32-bit FNV hash function */
#define FNV_MULT 16777619U
#define FNV_BASIS 2166136261U

static unsigned _dnsPacket_hashBytes(DnsPacket* packet, int numBytes, unsigned hash) {
    const uint8_t* p = packet->cursor;
    const uint8_t* end = packet->end;

    while (numBytes > 0 && p < end) {
        hash = hash * FNV_MULT ^ *p++;
        numBytes--;
    }
    packet->cursor = p;
    return hash;
}

static unsigned _dnsPacket_hashQName(DnsPacket* packet, unsigned hash) {
    const uint8_t* p = packet->cursor;
    const uint8_t* end = packet->end;

    for (;;) {
        int c;

        if (p >= end) { /* should not happen */
            LOG(INFO) << __func__ << ": INTERNAL_ERROR: read-overflow";
            break;
        }

        c = *p++;

        if (c == 0) break;

        if (c >= 64) {
            LOG(INFO) << __func__ << ": INTERNAL_ERROR: malformed domain";
            break;
        }
        if (p + c >= end) {
            LOG(INFO) << __func__ << ": INTERNAL_ERROR: simple label read-overflow";
            break;
        }
        while (c > 0) {
            hash = hash * FNV_MULT ^ *p++;
            c -= 1;
        }
    }
    packet->cursor = p;
    return hash;
}

static unsigned _dnsPacket_hashQR(DnsPacket* packet, unsigned hash) {
    hash = _dnsPacket_hashQName(packet, hash);
    hash = _dnsPacket_hashBytes(packet, 4, hash); /* TYPE and CLASS */
    return hash;
}

static unsigned _dnsPacket_hashRR(DnsPacket* packet, unsigned hash) {
    int rdlength;
    hash = _dnsPacket_hashQR(packet, hash);
    hash = _dnsPacket_hashBytes(packet, 4, hash); /* TTL */
    rdlength = _dnsPacket_readInt16(packet);
    hash = _dnsPacket_hashBytes(packet, rdlength, hash); /* RDATA */
    return hash;
}

static unsigned _dnsPacket_hashQuery(DnsPacket* packet) {
    unsigned hash = FNV_BASIS;
    int count, arcount;
    _dnsPacket_rewind(packet);

    /* ignore the ID */
    _dnsPacket_skip(packet, 2);

    /* we ignore the TC bit for reasons explained in
     * _dnsPacket_checkQuery().
     *
     * however we hash the RD bit to differentiate
     * between answers for recursive and non-recursive
     * queries.
     */
    hash = hash * FNV_MULT ^ (packet->base[2] & 1);

    /* mark the first header byte as processed */
    _dnsPacket_skip(packet, 1);

    /* process the second header byte */
    hash = _dnsPacket_hashBytes(packet, 1, hash);

    /* read QDCOUNT */
    count = _dnsPacket_readInt16(packet);

    /* assume: ANcount and NScount are 0 */
    _dnsPacket_skip(packet, 4);

    /* read ARCOUNT */
    arcount = _dnsPacket_readInt16(packet);

    /* hash QDCOUNT QRs */
    for (; count > 0; count--) hash = _dnsPacket_hashQR(packet, hash);

    /* hash ARCOUNT RRs */
    for (; arcount > 0; arcount--) hash = _dnsPacket_hashRR(packet, hash);

    return hash;
}

/** QUERY COMPARISON
 **
 ** THE FOLLOWING CODE ASSUMES THAT THE INPUT PACKETS HAVE ALREADY
 ** BEEN SUCCESSFULLY CHECKED.
 **/

static int _dnsPacket_isEqualDomainName(DnsPacket* pack1, DnsPacket* pack2) {
    const uint8_t* p1 = pack1->cursor;
    const uint8_t* end1 = pack1->end;
    const uint8_t* p2 = pack2->cursor;
    const uint8_t* end2 = pack2->end;

    for (;;) {
        int c1, c2;

        if (p1 >= end1 || p2 >= end2) {
            LOG(INFO) << __func__ << ": INTERNAL_ERROR: read-overflow";
            break;
        }
        c1 = *p1++;
        c2 = *p2++;
        if (c1 != c2) break;

        if (c1 == 0) {
            pack1->cursor = p1;
            pack2->cursor = p2;
            return 1;
        }
        if (c1 >= 64) {
            LOG(INFO) << __func__ << ": INTERNAL_ERROR: malformed domain";
            break;
        }
        if ((p1 + c1 > end1) || (p2 + c1 > end2)) {
            LOG(INFO) << __func__ << ": INTERNAL_ERROR: simple label read-overflow";
            break;
        }
        if (memcmp(p1, p2, c1) != 0) break;
        p1 += c1;
        p2 += c1;
        /* we rely on the bound checks at the start of the loop */
    }
    /* not the same, or one is malformed */
    LOG(INFO) << __func__ << ": different DN";
    return 0;
}

static int _dnsPacket_isEqualBytes(DnsPacket* pack1, DnsPacket* pack2, int numBytes) {
    const uint8_t* p1 = pack1->cursor;
    const uint8_t* p2 = pack2->cursor;

    if (p1 + numBytes > pack1->end || p2 + numBytes > pack2->end) return 0;

    if (memcmp(p1, p2, numBytes) != 0) return 0;

    pack1->cursor += numBytes;
    pack2->cursor += numBytes;
    return 1;
}

static int _dnsPacket_isEqualQR(DnsPacket* pack1, DnsPacket* pack2) {
    /* compare domain name encoding + TYPE + CLASS */
    if (!_dnsPacket_isEqualDomainName(pack1, pack2) ||
        !_dnsPacket_isEqualBytes(pack1, pack2, 2 + 2))
        return 0;

    return 1;
}

static int _dnsPacket_isEqualRR(DnsPacket* pack1, DnsPacket* pack2) {
    int rdlength1, rdlength2;
    /* compare query + TTL */
    if (!_dnsPacket_isEqualQR(pack1, pack2) || !_dnsPacket_isEqualBytes(pack1, pack2, 4)) return 0;

    /* compare RDATA */
    rdlength1 = _dnsPacket_readInt16(pack1);
    rdlength2 = _dnsPacket_readInt16(pack2);
    if (rdlength1 != rdlength2 || !_dnsPacket_isEqualBytes(pack1, pack2, rdlength1)) return 0;

    return 1;
}

static int _dnsPacket_isEqualQuery(DnsPacket* pack1, DnsPacket* pack2) {
    int count1, count2, arcount1, arcount2;

    /* compare the headers, ignore most fields */
    _dnsPacket_rewind(pack1);
    _dnsPacket_rewind(pack2);

    /* compare RD, ignore TC, see comment in _dnsPacket_checkQuery */
    if ((pack1->base[2] & 1) != (pack2->base[2] & 1)) {
        LOG(INFO) << __func__ << ": different RD";
        return 0;
    }

    if (pack1->base[3] != pack2->base[3]) {
        LOG(INFO) << __func__ << ": different CD or AD";
        return 0;
    }

    /* mark ID and header bytes as compared */
    _dnsPacket_skip(pack1, 4);
    _dnsPacket_skip(pack2, 4);

    /* compare QDCOUNT */
    count1 = _dnsPacket_readInt16(pack1);
    count2 = _dnsPacket_readInt16(pack2);
    if (count1 != count2 || count1 < 0) {
        LOG(INFO) << __func__ << ": different QDCOUNT";
        return 0;
    }

    /* assume: ANcount and NScount are 0 */
    _dnsPacket_skip(pack1, 4);
    _dnsPacket_skip(pack2, 4);

    /* compare ARCOUNT */
    arcount1 = _dnsPacket_readInt16(pack1);
    arcount2 = _dnsPacket_readInt16(pack2);
    if (arcount1 != arcount2 || arcount1 < 0) {
        LOG(INFO) << __func__ << ": different ARCOUNT";
        return 0;
    }

    /* compare the QDCOUNT QRs */
    for (; count1 > 0; count1--) {
        if (!_dnsPacket_isEqualQR(pack1, pack2)) {
            LOG(INFO) << __func__ << ": different QR";
            return 0;
        }
    }

    /* compare the ARCOUNT RRs */
    for (; arcount1 > 0; arcount1--) {
        if (!_dnsPacket_isEqualRR(pack1, pack2)) {
            LOG(INFO) << __func__ << ": different additional RR";
            return 0;
        }
    }
    return 1;
}

/* cache entry. for simplicity, 'hash' and 'hlink' are inlined in this
 * structure though they are conceptually part of the hash table.
 *
 * similarly, mru_next and mru_prev are part of the global MRU list
 */
struct Entry {
    unsigned int hash;   /* hash value */
    struct Entry* hlink; /* next in collision chain */
    struct Entry* mru_prev;
    struct Entry* mru_next;

    const uint8_t* query;
    int querylen;
    const uint8_t* answer;
    int answerlen;
    time_t expires; /* time_t when the entry isn't valid any more */
    int id;         /* for debugging purpose */
};

/*
 * Find the TTL for a negative DNS result.  This is defined as the minimum
 * of the SOA records TTL and the MINIMUM-TTL field (RFC-2308).
 *
 * Return 0 if not found.
 */
static uint32_t answer_getNegativeTTL(ns_msg handle) {
    int n, nscount;
    uint32_t result = 0;
    ns_rr rr;

    nscount = ns_msg_count(handle, ns_s_ns);
    for (n = 0; n < nscount; n++) {
        if ((ns_parserr(&handle, ns_s_ns, n, &rr) == 0) && (ns_rr_type(rr) == ns_t_soa)) {
            const uint8_t* rdata = ns_rr_rdata(rr);          // find the data
            const uint8_t* edata = rdata + ns_rr_rdlen(rr);  // add the len to find the end
            int len;
            uint32_t ttl, rec_result = rr.ttl;

            // find the MINIMUM-TTL field from the blob of binary data for this record
            // skip the server name
            len = dn_skipname(rdata, edata);
            if (len == -1) continue;  // error skipping
            rdata += len;

            // skip the admin name
            len = dn_skipname(rdata, edata);
            if (len == -1) continue;  // error skipping
            rdata += len;

            if (edata - rdata != 5 * NS_INT32SZ) continue;
            // skip: serial number + refresh interval + retry interval + expiry
            rdata += NS_INT32SZ * 4;
            // finally read the MINIMUM TTL
            ttl = ntohl(*reinterpret_cast<const uint32_t*>(rdata));
            if (ttl < rec_result) {
                rec_result = ttl;
            }
            // Now that the record is read successfully, apply the new min TTL
            if (n == 0 || rec_result < result) {
                result = rec_result;
            }
        }
    }
    return result;
}

/*
 * Parse the answer records and find the appropriate
 * smallest TTL among the records.  This might be from
 * the answer records if found or from the SOA record
 * if it's a negative result.
 *
 * The returned TTL is the number of seconds to
 * keep the answer in the cache.
 *
 * In case of parse error zero (0) is returned which
 * indicates that the answer shall not be cached.
 */
static uint32_t answer_getTTL(const void* answer, int answerlen) {
    ns_msg handle;
    int ancount, n;
    uint32_t result, ttl;
    ns_rr rr;

    result = 0;
    if (ns_initparse((const uint8_t*) answer, answerlen, &handle) >= 0) {
        // get number of answer records
        ancount = ns_msg_count(handle, ns_s_an);

        if (ancount == 0) {
            // a response with no answers?  Cache this negative result.
            result = answer_getNegativeTTL(handle);
        } else {
            for (n = 0; n < ancount; n++) {
                if (ns_parserr(&handle, ns_s_an, n, &rr) == 0) {
                    ttl = rr.ttl;
                    if (n == 0 || ttl < result) {
                        result = ttl;
                    }
                } else {
                    PLOG(INFO) << __func__ << ": ns_parserr failed ancount no = " << n;
                }
            }
        }
    } else {
        PLOG(INFO) << __func__ << ": ns_initparse failed";
    }

    LOG(INFO) << __func__ << ": TTL = " << result;
    return result;
}

static void entry_free(Entry* e) {
    /* everything is allocated in a single memory block */
    if (e) {
        free(e);
    }
}

static void entry_mru_remove(Entry* e) {
    e->mru_prev->mru_next = e->mru_next;
    e->mru_next->mru_prev = e->mru_prev;
}

static void entry_mru_add(Entry* e, Entry* list) {
    Entry* first = list->mru_next;

    e->mru_next = first;
    e->mru_prev = list;

    list->mru_next = e;
    first->mru_prev = e;
}

/* compute the hash of a given entry, this is a hash of most
 * data in the query (key) */
static unsigned entry_hash(const Entry* e) {
    DnsPacket pack[1];

    _dnsPacket_init(pack, e->query, e->querylen);
    return _dnsPacket_hashQuery(pack);
}

/* initialize an Entry as a search key, this also checks the input query packet
 * returns 1 on success, or 0 in case of unsupported/malformed data */
static int entry_init_key(Entry* e, const void* query, int querylen) {
    DnsPacket pack[1];

    memset(e, 0, sizeof(*e));

    e->query = (const uint8_t*) query;
    e->querylen = querylen;
    e->hash = entry_hash(e);

    _dnsPacket_init(pack, e->query, e->querylen);

    return _dnsPacket_checkQuery(pack);
}

/* allocate a new entry as a cache node */
static Entry* entry_alloc(const Entry* init, const void* answer, int answerlen) {
    Entry* e;
    int size;

    size = sizeof(*e) + init->querylen + answerlen;
    e = (Entry*) calloc(size, 1);
    if (e == NULL) return e;

    e->hash = init->hash;
    e->query = (const uint8_t*) (e + 1);
    e->querylen = init->querylen;

    memcpy((char*) e->query, init->query, e->querylen);

    e->answer = e->query + e->querylen;
    e->answerlen = answerlen;

    memcpy((char*) e->answer, answer, e->answerlen);

    return e;
}

static int entry_equals(const Entry* e1, const Entry* e2) {
    DnsPacket pack1[1], pack2[1];

    if (e1->querylen != e2->querylen) {
        return 0;
    }
    _dnsPacket_init(pack1, e1->query, e1->querylen);
    _dnsPacket_init(pack2, e2->query, e2->querylen);

    return _dnsPacket_isEqualQuery(pack1, pack2);
}

/* We use a simple hash table with external collision lists
 * for simplicity, the hash-table fields 'hash' and 'hlink' are
 * inlined in the Entry structure.
 */

/* Maximum time for a thread to wait for an pending request */
constexpr int PENDING_REQUEST_TIMEOUT = 20;

// lock protecting everything in NetConfig.
static std::mutex cache_mutex;
static std::condition_variable cv;

namespace {

// Map format: ReturnCode:rate_denom
// if the ReturnCode is not associated with any rate_denom, use default
// Sampling rate varies by return code; events to log are chosen randomly, with a
// probability proportional to the sampling rate.
constexpr const char DEFAULT_SUBSAMPLING_MAP[] = "default:1 0:100 7:10";

std::unordered_map<int, uint32_t> resolv_get_dns_event_subsampling_map() {
    using android::base::ParseInt;
    using android::base::ParseUint;
    using android::base::Split;
    using server_configurable_flags::GetServerConfigurableFlag;
    std::unordered_map<int, uint32_t> sampling_rate_map{};
    std::vector<std::string> subsampling_vector =
            Split(GetServerConfigurableFlag("netd_native", "dns_event_subsample_map",
                                            DEFAULT_SUBSAMPLING_MAP),
                  " ");
    for (const auto& pair : subsampling_vector) {
        std::vector<std::string> rate_denom = Split(pair, ":");
        int return_code;
        uint32_t denom;
        if (rate_denom.size() != 2) {
            LOG(ERROR) << __func__ << ": invalid subsampling_pair = " << pair;
            continue;
        }
        if (rate_denom[0] == "default") {
            return_code = DNSEVENT_SUBSAMPLING_MAP_DEFAULT_KEY;
        } else if (!ParseInt(rate_denom[0], &return_code)) {
            LOG(ERROR) << __func__ << ": parse subsampling_pair failed = " << pair;
            continue;
        }
        if (!ParseUint(rate_denom[1], &denom)) {
            LOG(ERROR) << __func__ << ": parse subsampling_pair failed = " << pair;
            continue;
        }
        sampling_rate_map[return_code] = denom;
    }
    return sampling_rate_map;
}

}  // namespace

// Note that Cache is not thread-safe per se, access to its members must be protected
// by an external mutex.
//
// TODO: move all cache manipulation code here and make data members private.
struct Cache {
    Cache() {
        entries.resize(CONFIG_MAX_ENTRIES);
        mru_list.mru_prev = mru_list.mru_next = &mru_list;
    }
    ~Cache() { flush(); }

    void flush() {
        for (int nn = 0; nn < CONFIG_MAX_ENTRIES; nn++) {
            Entry** pnode = (Entry**)&entries[nn];

            while (*pnode) {
                Entry* node = *pnode;
                *pnode = node->hlink;
                entry_free(node);
            }
        }

        flushPendingRequests();

        mru_list.mru_next = mru_list.mru_prev = &mru_list;
        num_entries = 0;
        last_id = 0;

        LOG(INFO) << "DNS cache flushed";
    }

    void flushPendingRequests() {
        pending_req_info* ri = pending_requests.next;
        while (ri) {
            pending_req_info* tmp = ri;
            ri = ri->next;
            free(tmp);
        }

        pending_requests.next = nullptr;
        cv.notify_all();
    }

    int num_entries = 0;

    // TODO: convert to std::list
    Entry mru_list;
    int last_id = 0;
    std::vector<Entry> entries;

    // TODO: convert to std::vector
    struct pending_req_info {
        unsigned int hash;
        struct pending_req_info* next;
    } pending_requests{};
};

struct NetConfig {
    explicit NetConfig(unsigned netId) : netid(netId) {
        cache = std::make_unique<Cache>();
        dns_event_subsampling_map = resolv_get_dns_event_subsampling_map();
    }
    int nameserverCount() { return nameserverSockAddrs.size(); }

    const unsigned netid;
    std::unique_ptr<Cache> cache;
    std::vector<std::string> nameservers;
    std::vector<IPSockAddr> nameserverSockAddrs;
    int revision_id = 0;  // # times the nameservers have been replaced
    res_params params{};
    res_stats nsstats[MAXNS]{};
    std::vector<std::string> search_domains;
    int wait_for_pending_req_timeout_count = 0;
    // Map format: ReturnCode:rate_denom
    std::unordered_map<int, uint32_t> dns_event_subsampling_map;
    DnsStats dnsStats;
    // Customized hostname/address table will be stored in customizedTable.
    // If resolverParams.hosts is empty, the existing customized table will be erased.
    HostMapping customizedTable = {};
    int tc_mode = aidl::android::net::IDnsResolver::TC_MODE_DEFAULT;
    bool enforceDnsUid = false;
    std::vector<int32_t> transportTypes;
};

/* gets cache associated with a network, or NULL if none exists */
static Cache* find_named_cache_locked(unsigned netid) REQUIRES(cache_mutex);

// Return true - if there is a pending request in |cache| matching |key|.
// Return false - if no pending request is found matching the key. Optionally
//                link a new one if parameter append_if_not_found is true.
static bool cache_has_pending_request_locked(Cache* cache, const Entry* key,
                                             bool append_if_not_found) {
    if (!cache || !key) return false;

    Cache::pending_req_info* ri = cache->pending_requests.next;
    Cache::pending_req_info* prev = &cache->pending_requests;
    while (ri) {
        if (ri->hash == key->hash) {
            return true;
        }
        prev = ri;
        ri = ri->next;
    }

    if (append_if_not_found) {
        ri = (Cache::pending_req_info*)calloc(1, sizeof(Cache::pending_req_info));
        if (ri) {
            ri->hash = key->hash;
            prev->next = ri;
        }
    }
    return false;
}

// Notify all threads that the cache entry |key| has become available
static void cache_notify_waiting_tid_locked(struct Cache* cache, const Entry* key) {
    if (!cache || !key) return;

    Cache::pending_req_info* ri = cache->pending_requests.next;
    Cache::pending_req_info* prev = &cache->pending_requests;
    while (ri) {
        if (ri->hash == key->hash) {
            // remove item from list and destroy
            prev->next = ri->next;
            free(ri);
            cv.notify_all();
            return;
        }
        prev = ri;
        ri = ri->next;
    }
}

void _resolv_cache_query_failed(unsigned netid, const void* query, int querylen, uint32_t flags) {
    // We should not notify with these flags.
    if (flags & (ANDROID_RESOLV_NO_CACHE_STORE | ANDROID_RESOLV_NO_CACHE_LOOKUP)) {
        return;
    }
    Entry key[1];

    if (!entry_init_key(key, query, querylen)) return;

    std::lock_guard guard(cache_mutex);

    Cache* cache = find_named_cache_locked(netid);

    if (cache) {
        cache_notify_waiting_tid_locked(cache, key);
    }
}

static void cache_dump_mru_locked(Cache* cache) {
    std::string buf;

    StringAppendF(&buf, "MRU LIST (%2d): ", cache->num_entries);
    for (Entry* e = cache->mru_list.mru_next; e != &cache->mru_list; e = e->mru_next) {
        StringAppendF(&buf, " %d", e->id);
    }

    LOG(INFO) << __func__ << ": " << buf;
}

/* This function tries to find a key within the hash table
 * In case of success, it will return a *pointer* to the hashed key.
 * In case of failure, it will return a *pointer* to NULL
 *
 * So, the caller must check '*result' to check for success/failure.
 *
 * The main idea is that the result can later be used directly in
 * calls to resolv_cache_add or _resolv_cache_remove as the 'lookup'
 * parameter. This makes the code simpler and avoids re-searching
 * for the key position in the htable.
 *
 * The result of a lookup_p is only valid until you alter the hash
 * table.
 */
static Entry** _cache_lookup_p(Cache* cache, Entry* key) {
    int index = key->hash % CONFIG_MAX_ENTRIES;
    Entry** pnode = (Entry**) &cache->entries[index];

    while (*pnode != NULL) {
        Entry* node = *pnode;

        if (node == NULL) break;

        if (node->hash == key->hash && entry_equals(node, key)) break;

        pnode = &node->hlink;
    }
    return pnode;
}

/* Add a new entry to the hash table. 'lookup' must be the
 * result of an immediate previous failed _lookup_p() call
 * (i.e. with *lookup == NULL), and 'e' is the pointer to the
 * newly created entry
 */
static void _cache_add_p(Cache* cache, Entry** lookup, Entry* e) {
    *lookup = e;
    e->id = ++cache->last_id;
    entry_mru_add(e, &cache->mru_list);
    cache->num_entries += 1;

    LOG(INFO) << __func__ << ": entry " << e->id << " added (count=" << cache->num_entries << ")";
}

/* Remove an existing entry from the hash table,
 * 'lookup' must be the result of an immediate previous
 * and succesful _lookup_p() call.
 */
static void _cache_remove_p(Cache* cache, Entry** lookup) {
    Entry* e = *lookup;

    LOG(INFO) << __func__ << ": entry " << e->id << " removed (count=" << cache->num_entries - 1
              << ")";

    entry_mru_remove(e);
    *lookup = e->hlink;
    entry_free(e);
    cache->num_entries -= 1;
}

/* Remove the oldest entry from the hash table.
 */
static void _cache_remove_oldest(Cache* cache) {
    Entry* oldest = cache->mru_list.mru_prev;
    Entry** lookup = _cache_lookup_p(cache, oldest);

    if (*lookup == NULL) { /* should not happen */
        LOG(INFO) << __func__ << ": OLDEST NOT IN HTABLE ?";
        return;
    }
    LOG(INFO) << __func__ << ": Cache full - removing oldest";
    res_pquery(oldest->query, oldest->querylen);
    _cache_remove_p(cache, lookup);
}

/* Remove all expired entries from the hash table.
 */
static void _cache_remove_expired(Cache* cache) {
    Entry* e;
    time_t now = _time_now();

    for (e = cache->mru_list.mru_next; e != &cache->mru_list;) {
        // Entry is old, remove
        if (now >= e->expires) {
            Entry** lookup = _cache_lookup_p(cache, e);
            if (*lookup == NULL) { /* should not happen */
                LOG(INFO) << __func__ << ": ENTRY NOT IN HTABLE ?";
                return;
            }
            e = e->mru_next;
            _cache_remove_p(cache, lookup);
        } else {
            e = e->mru_next;
        }
    }
}

// Get a NetConfig associated with a network, or nullptr if not found.
static NetConfig* find_netconfig_locked(unsigned netid) REQUIRES(cache_mutex);

ResolvCacheStatus resolv_cache_lookup(unsigned netid, const void* query, int querylen, void* answer,
                                      int answersize, int* answerlen, uint32_t flags) {
    // Skip cache lookup, return RESOLV_CACHE_NOTFOUND directly so that it is
    // possible to cache the answer of this query.
    // If ANDROID_RESOLV_NO_CACHE_STORE is set, return RESOLV_CACHE_SKIP to skip possible cache
    // storing.
    // (b/150371903): ANDROID_RESOLV_NO_CACHE_STORE should imply ANDROID_RESOLV_NO_CACHE_LOOKUP
    // to avoid side channel attack.
    if (flags & (ANDROID_RESOLV_NO_CACHE_LOOKUP | ANDROID_RESOLV_NO_CACHE_STORE)) {
        return flags & ANDROID_RESOLV_NO_CACHE_STORE ? RESOLV_CACHE_SKIP : RESOLV_CACHE_NOTFOUND;
    }
    Entry key;
    Entry** lookup;
    Entry* e;
    time_t now;

    LOG(INFO) << __func__ << ": lookup";

    /* we don't cache malformed queries */
    if (!entry_init_key(&key, query, querylen)) {
        LOG(INFO) << __func__ << ": unsupported query";
        return RESOLV_CACHE_UNSUPPORTED;
    }
    /* lookup cache */
    std::unique_lock lock(cache_mutex);
    android::base::ScopedLockAssertion assume_lock(cache_mutex);
    Cache* cache = find_named_cache_locked(netid);
    if (cache == nullptr) {
        return RESOLV_CACHE_UNSUPPORTED;
    }

    /* see the description of _lookup_p to understand this.
     * the function always return a non-NULL pointer.
     */
    lookup = _cache_lookup_p(cache, &key);
    e = *lookup;

    if (e == NULL) {
        LOG(INFO) << __func__ << ": NOT IN CACHE";

        if (!cache_has_pending_request_locked(cache, &key, true)) {
            return RESOLV_CACHE_NOTFOUND;

        } else {
            LOG(INFO) << __func__ << ": Waiting for previous request";
            // wait until (1) timeout OR
            //            (2) cv is notified AND no pending request matching the |key|
            // (cv notifier should delete pending request before sending notification.)
            bool ret = cv.wait_for(lock, std::chrono::seconds(PENDING_REQUEST_TIMEOUT),
                                   [netid, &cache, &key]() REQUIRES(cache_mutex) {
                                       // Must update cache as it could have been deleted
                                       cache = find_named_cache_locked(netid);
                                       return !cache_has_pending_request_locked(cache, &key, false);
                                   });
            if (!cache) {
                return RESOLV_CACHE_NOTFOUND;
            }
            if (ret == false) {
                NetConfig* info = find_netconfig_locked(netid);
                if (info != NULL) {
                    info->wait_for_pending_req_timeout_count++;
                }
            }
            lookup = _cache_lookup_p(cache, &key);
            e = *lookup;
            if (e == NULL) {
                return RESOLV_CACHE_NOTFOUND;
            }
        }
    }

    now = _time_now();

    /* remove stale entries here */
    if (now >= e->expires) {
        LOG(INFO) << __func__ << ": NOT IN CACHE (STALE ENTRY " << *lookup << "DISCARDED)";
        res_pquery(e->query, e->querylen);
        _cache_remove_p(cache, lookup);
        return RESOLV_CACHE_NOTFOUND;
    }

    *answerlen = e->answerlen;
    if (e->answerlen > answersize) {
        /* NOTE: we return UNSUPPORTED if the answer buffer is too short */
        LOG(INFO) << __func__ << ": ANSWER TOO LONG";
        return RESOLV_CACHE_UNSUPPORTED;
    }

    memcpy(answer, e->answer, e->answerlen);

    /* bump up this entry to the top of the MRU list */
    if (e != cache->mru_list.mru_next) {
        entry_mru_remove(e);
        entry_mru_add(e, &cache->mru_list);
    }

    LOG(INFO) << __func__ << ": FOUND IN CACHE entry=" << e;
    return RESOLV_CACHE_FOUND;
}

int resolv_cache_add(unsigned netid, const void* query, int querylen, const void* answer,
                     int answerlen) {
    Entry key[1];
    Entry* e;
    Entry** lookup;
    uint32_t ttl;
    Cache* cache = NULL;

    /* don't assume that the query has already been cached
     */
    if (!entry_init_key(key, query, querylen)) {
        LOG(INFO) << __func__ << ": passed invalid query?";
        return -EINVAL;
    }

    std::lock_guard guard(cache_mutex);

    cache = find_named_cache_locked(netid);
    if (cache == nullptr) {
        return -ENONET;
    }

    lookup = _cache_lookup_p(cache, key);
    e = *lookup;

    // Should only happen on ANDROID_RESOLV_NO_CACHE_LOOKUP
    if (e != NULL) {
        LOG(INFO) << __func__ << ": ALREADY IN CACHE (" << e << ") ? IGNORING ADD";
        cache_notify_waiting_tid_locked(cache, key);
        return -EEXIST;
    }

    if (cache->num_entries >= CONFIG_MAX_ENTRIES) {
        _cache_remove_expired(cache);
        if (cache->num_entries >= CONFIG_MAX_ENTRIES) {
            _cache_remove_oldest(cache);
        }
        // TODO: It looks useless, remove below code after having test to prove it.
        lookup = _cache_lookup_p(cache, key);
        e = *lookup;
        if (e != NULL) {
            LOG(INFO) << __func__ << ": ALREADY IN CACHE (" << e << ") ? IGNORING ADD";
            cache_notify_waiting_tid_locked(cache, key);
            return -EEXIST;
        }
    }

    ttl = answer_getTTL(answer, answerlen);
    if (ttl > 0) {
        e = entry_alloc(key, answer, answerlen);
        if (e != NULL) {
            e->expires = ttl + _time_now();
            _cache_add_p(cache, lookup, e);
        }
    }

    cache_dump_mru_locked(cache);
    cache_notify_waiting_tid_locked(cache, key);

    return 0;
}

bool resolv_gethostbyaddr_from_cache(unsigned netid, char domain_name[], size_t domain_name_size,
                                     const char* ip_address, int af) {
    if (domain_name_size > NS_MAXDNAME) {
        LOG(WARNING) << __func__ << ": invalid domain_name_size " << domain_name_size;
        return false;
    } else if (ip_address == nullptr || ip_address[0] == '\0') {
        LOG(WARNING) << __func__ << ": invalid ip_address";
        return false;
    } else if (af != AF_INET && af != AF_INET6) {
        LOG(WARNING) << __func__ << ": unsupported AF";
        return false;
    }

    Cache* cache = nullptr;
    Entry* node = nullptr;

    ns_rr rr;
    ns_msg handle;
    ns_rr rr_query;

    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;
    char* addr_buf = nullptr;

    std::lock_guard guard(cache_mutex);

    cache = find_named_cache_locked(netid);
    if (cache == nullptr) {
        return false;
    }

    for (node = cache->mru_list.mru_next; node != nullptr && node != &cache->mru_list;
         node = node->mru_next) {
        if (node->answer == nullptr) {
            continue;
        }

        memset(&handle, 0, sizeof(handle));

        if (ns_initparse(node->answer, node->answerlen, &handle) < 0) {
            continue;
        }

        for (int n = 0; n < ns_msg_count(handle, ns_s_an); n++) {
            memset(&rr, 0, sizeof(rr));

            if (ns_parserr(&handle, ns_s_an, n, &rr)) {
                continue;
            }

            if (ns_rr_type(rr) == ns_t_a && af == AF_INET) {
                addr_buf = (char*)&(sa.sin_addr);
            } else if (ns_rr_type(rr) == ns_t_aaaa && af == AF_INET6) {
                addr_buf = (char*)&(sa6.sin6_addr);
            } else {
                continue;
            }

            if (inet_pton(af, ip_address, addr_buf) != 1) {
                LOG(WARNING) << __func__ << ": inet_pton() fail";
                return false;
            }

            if (memcmp(ns_rr_rdata(rr), addr_buf, ns_rr_rdlen(rr)) == 0) {
                int query_count = ns_msg_count(handle, ns_s_qd);
                for (int i = 0; i < query_count; i++) {
                    memset(&rr_query, 0, sizeof(rr_query));
                    if (ns_parserr(&handle, ns_s_qd, i, &rr_query)) {
                        continue;
                    }
                    strlcpy(domain_name, ns_rr_name(rr_query), domain_name_size);
                    if (domain_name[0] != '\0') {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

static std::unordered_map<unsigned, std::unique_ptr<NetConfig>> sNetConfigMap
        GUARDED_BY(cache_mutex);

// Clears nameservers set for |netconfig| and clears the stats
static void free_nameservers_locked(NetConfig* netconfig);
// Order-insensitive comparison for the two set of servers.
static bool resolv_is_nameservers_equal(const std::vector<std::string>& oldServers,
                                        const std::vector<std::string>& newServers);
// clears the stats samples contained withing the given netconfig.
static void res_cache_clear_stats_locked(NetConfig* netconfig);

// public API for netd to query if name server is set on specific netid
bool resolv_has_nameservers(unsigned netid) {
    std::lock_guard guard(cache_mutex);
    NetConfig* info = find_netconfig_locked(netid);
    return (info != nullptr) && (info->nameserverCount() > 0);
}

int resolv_create_cache_for_net(unsigned netid) {
    std::lock_guard guard(cache_mutex);
    if (sNetConfigMap.find(netid) != sNetConfigMap.end()) {
        LOG(ERROR) << __func__ << ": Cache is already created, netId: " << netid;
        return -EEXIST;
    }

    sNetConfigMap[netid] = std::make_unique<NetConfig>(netid);
    return 0;
}

void resolv_delete_cache_for_net(unsigned netid) {
    std::lock_guard guard(cache_mutex);
    sNetConfigMap.erase(netid);
}

int resolv_flush_cache_for_net(unsigned netid) {
    std::lock_guard guard(cache_mutex);

    NetConfig* netconfig = find_netconfig_locked(netid);
    if (netconfig == nullptr) {
        return -ENONET;
    }
    netconfig->cache->flush();

    // Also clear the NS statistics.
    res_cache_clear_stats_locked(netconfig);
    return 0;
}

std::vector<unsigned> resolv_list_caches() {
    std::lock_guard guard(cache_mutex);
    std::vector<unsigned> result;
    result.reserve(sNetConfigMap.size());
    for (const auto& [netId, _] : sNetConfigMap) {
        result.push_back(netId);
    }
    return result;
}

static Cache* find_named_cache_locked(unsigned netid) {
    NetConfig* info = find_netconfig_locked(netid);
    if (info != nullptr) return info->cache.get();
    return nullptr;
}

static NetConfig* find_netconfig_locked(unsigned netid) {
    if (auto it = sNetConfigMap.find(netid); it != sNetConfigMap.end()) {
        return it->second.get();
    }
    return nullptr;
}

static void resolv_set_experiment_params(res_params* params) {
    if (params->retry_count == 0) {
        params->retry_count = getExperimentFlagInt("retry_count", RES_DFLRETRY);
    }

    if (params->base_timeout_msec == 0) {
        params->base_timeout_msec =
                getExperimentFlagInt("retransmission_time_interval", RES_TIMEOUT);
    }
}

android::net::NetworkType resolv_get_network_types_for_net(unsigned netid) {
    std::lock_guard guard(cache_mutex);
    NetConfig* netconfig = find_netconfig_locked(netid);
    if (netconfig == nullptr) return android::net::NT_UNKNOWN;
    return convert_network_type(netconfig->transportTypes);
}

namespace {

// Returns valid domains without duplicates which are limited to max size |MAXDNSRCH|.
std::vector<std::string> filter_domains(const std::vector<std::string>& domains) {
    std::set<std::string> tmp_set;
    std::vector<std::string> res;

    std::copy_if(domains.begin(), domains.end(), std::back_inserter(res),
                 [&tmp_set](const std::string& str) {
                     return !(str.size() > MAXDNSRCHPATH - 1) && (tmp_set.insert(str).second);
                 });
    if (res.size() > MAXDNSRCH) {
        LOG(WARNING) << __func__ << ": valid domains=" << res.size()
                     << ", but MAXDNSRCH=" << MAXDNSRCH;
        res.resize(MAXDNSRCH);
    }
    return res;
}

std::vector<std::string> filter_nameservers(const std::vector<std::string>& servers) {
    std::vector<std::string> res = servers;
    if (res.size() > MAXNS) {
        LOG(WARNING) << __func__ << ": too many servers: " << res.size();
        res.resize(MAXNS);
    }
    return res;
}

bool isValidServer(const std::string& server) {
    const addrinfo hints = {
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_DGRAM,
    };
    addrinfo* result = nullptr;
    if (int err = getaddrinfo_numeric(server.c_str(), "53", hints, &result); err != 0) {
        LOG(WARNING) << __func__ << ": getaddrinfo_numeric(" << server
                     << ") = " << gai_strerror(err);
        return false;
    }
    freeaddrinfo(result);
    return true;
}

}  // namespace

std::vector<std::string> getCustomizedTableByName(const size_t netid, const char* hostname) {
    std::lock_guard guard(cache_mutex);
    NetConfig* netconfig = find_netconfig_locked(netid);

    std::vector<std::string> result;
    if (netconfig != nullptr) {
        const auto& hosts = netconfig->customizedTable.equal_range(hostname);
        for (auto i = hosts.first; i != hosts.second; ++i) {
            result.push_back(i->second);
        }
    }
    return result;
}

int resolv_set_nameservers(unsigned netid, const std::vector<std::string>& servers,
                           const std::vector<std::string>& domains, const res_params& params,
                           const aidl::android::net::ResolverOptionsParcel& resolverOptions,
                           const std::vector<int32_t>& transportTypes) {
    std::vector<std::string> nameservers = filter_nameservers(servers);
    const int numservers = static_cast<int>(nameservers.size());

    LOG(INFO) << __func__ << ": netId = " << netid << ", numservers = " << numservers;

    // Parse the addresses before actually locking or changing any state, in case there is an error.
    // As a side effect this also reduces the time the lock is kept.
    std::vector<IPSockAddr> ipSockAddrs;
    ipSockAddrs.reserve(nameservers.size());
    for (const auto& server : nameservers) {
        if (!isValidServer(server)) return -EINVAL;
        ipSockAddrs.push_back(IPSockAddr::toIPSockAddr(server, 53));
    }

    std::lock_guard guard(cache_mutex);
    NetConfig* netconfig = find_netconfig_locked(netid);

    if (netconfig == nullptr) return -ENONET;

    uint8_t old_max_samples = netconfig->params.max_samples;
    netconfig->params = params;
    resolv_set_experiment_params(&netconfig->params);
    if (!resolv_is_nameservers_equal(netconfig->nameservers, nameservers)) {
        // free current before adding new
        free_nameservers_locked(netconfig);
        netconfig->nameservers = std::move(nameservers);
        for (int i = 0; i < numservers; i++) {
            LOG(INFO) << __func__ << ": netid = " << netid
                      << ", addr = " << netconfig->nameservers[i];
        }
        netconfig->nameserverSockAddrs = std::move(ipSockAddrs);
    } else {
        if (netconfig->params.max_samples != old_max_samples) {
            // If the maximum number of samples changes, the overhead of keeping the most recent
            // samples around is not considered worth the effort, so they are cleared instead.
            // All other parameters do not affect shared state: Changing these parameters does
            // not invalidate the samples, as they only affect aggregation and the conditions
            // under which servers are considered usable.
            res_cache_clear_stats_locked(netconfig);
        }
    }

    // Always update the search paths. Cache-flushing however is not necessary,
    // since the stored cache entries do contain the domain, not just the host name.
    netconfig->search_domains = filter_domains(domains);

    // Setup stats for cleartext dns servers.
    if (!netconfig->dnsStats.setServers(netconfig->nameserverSockAddrs, PROTO_TCP) ||
        !netconfig->dnsStats.setServers(netconfig->nameserverSockAddrs, PROTO_UDP)) {
        LOG(WARNING) << __func__ << ": netid = " << netid << ", failed to set dns stats";
        return -EINVAL;
    }
    netconfig->customizedTable.clear();
    for (const auto& host : resolverOptions.hosts) {
        if (!host.hostName.empty() && !host.ipAddr.empty())
            netconfig->customizedTable.emplace(host.hostName, host.ipAddr);
    }

    if (resolverOptions.tcMode < aidl::android::net::IDnsResolver::TC_MODE_DEFAULT ||
        resolverOptions.tcMode > aidl::android::net::IDnsResolver::TC_MODE_UDP_TCP) {
        LOG(WARNING) << __func__ << ": netid = " << netid
                     << ", invalid TC mode: " << resolverOptions.tcMode;
        return -EINVAL;
    }
    netconfig->tc_mode = resolverOptions.tcMode;
    netconfig->enforceDnsUid = resolverOptions.enforceDnsUid;

    netconfig->transportTypes = transportTypes;

    return 0;
}

static bool resolv_is_nameservers_equal(const std::vector<std::string>& oldServers,
                                        const std::vector<std::string>& newServers) {
    const std::set<std::string> olds(oldServers.begin(), oldServers.end());
    const std::set<std::string> news(newServers.begin(), newServers.end());

    // TODO: this is incorrect if the list of current or previous nameservers
    // contains duplicates. This does not really matter because the framework
    // filters out duplicates, but we should probably fix it. It's also
    // insensitive to the order of the nameservers; we should probably fix that
    // too.
    return olds == news;
}

static void free_nameservers_locked(NetConfig* netconfig) {
    netconfig->nameservers.clear();
    netconfig->nameserverSockAddrs.clear();
    res_cache_clear_stats_locked(netconfig);
}

void resolv_populate_res_for_net(ResState* statp) {
    if (statp == nullptr) {
        return;
    }
    LOG(INFO) << __func__ << ": netid=" << statp->netid;

    std::lock_guard guard(cache_mutex);
    NetConfig* info = find_netconfig_locked(statp->netid);
    if (info == nullptr) return;

    statp->nsaddrs = info->nameserverSockAddrs;
    statp->search_domains = info->search_domains;
    statp->tc_mode = info->tc_mode;
    statp->enforce_dns_uid = info->enforceDnsUid;
}

/* Resolver reachability statistics. */

static void res_cache_add_stats_sample_locked(res_stats* stats, const res_sample& sample,
                                              int max_samples) {
    // Note: This function expects max_samples > 0, otherwise a (harmless) modification of the
    // allocated but supposedly unused memory for samples[0] will happen
    LOG(INFO) << __func__ << ": adding sample to stats, next = " << unsigned(stats->sample_next)
              << ", count = " << unsigned(stats->sample_count);
    stats->samples[stats->sample_next] = sample;
    if (stats->sample_count < max_samples) {
        ++stats->sample_count;
    }
    if (++stats->sample_next >= max_samples) {
        stats->sample_next = 0;
    }
}

static void res_cache_clear_stats_locked(NetConfig* netconfig) {
    for (int i = 0; i < MAXNS; ++i) {
        netconfig->nsstats[i].sample_count = 0;
        netconfig->nsstats[i].sample_next = 0;
    }

    // Increment the revision id to ensure that sample state is not written back if the
    // servers change; in theory it would suffice to do so only if the servers or
    // max_samples actually change, in practice the overhead of checking is higher than the
    // cost, and overflows are unlikely.
    ++netconfig->revision_id;
}

int android_net_res_stats_get_info_for_net(unsigned netid, int* nscount,
                                           struct sockaddr_storage servers[MAXNS], int* dcount,
                                           char domains[MAXDNSRCH][MAXDNSRCHPATH],
                                           res_params* params, struct res_stats stats[MAXNS],
                                           int* wait_for_pending_req_timeout_count) {
    std::lock_guard guard(cache_mutex);
    NetConfig* info = find_netconfig_locked(netid);
    if (!info) return -1;

    const int num = info->nameserverCount();
    if (num > MAXNS) {
        LOG(INFO) << __func__ << ": nscount " << num << " > MAXNS " << MAXNS;
        errno = EFAULT;
        return -1;
    }

    for (int i = 0; i < num; i++) {
        servers[i] = info->nameserverSockAddrs[i];
        stats[i] = info->nsstats[i];
    }

    for (size_t i = 0; i < info->search_domains.size(); i++) {
        strlcpy(domains[i], info->search_domains[i].c_str(), MAXDNSRCHPATH);
    }

    *nscount = num;
    *dcount = static_cast<int>(info->search_domains.size());
    *params = info->params;
    *wait_for_pending_req_timeout_count = info->wait_for_pending_req_timeout_count;

    return info->revision_id;
}

std::vector<std::string> resolv_cache_dump_subsampling_map(unsigned netid) {
    using android::base::StringPrintf;
    std::lock_guard guard(cache_mutex);
    NetConfig* netconfig = find_netconfig_locked(netid);
    if (netconfig == nullptr) return {};
    std::vector<std::string> result;
    for (const auto& pair : netconfig->dns_event_subsampling_map) {
        result.push_back(StringPrintf("%s:%d",
                                      (pair.first == DNSEVENT_SUBSAMPLING_MAP_DEFAULT_KEY)
                                              ? "default"
                                              : std::to_string(pair.first).c_str(),
                                      pair.second));
    }
    return result;
}

// Decides whether an event should be sampled using a random number generator and
// a sampling factor derived from the netid and the return code.
//
// Returns the subsampling rate if the event should be sampled, or 0 if it should be discarded.
uint32_t resolv_cache_get_subsampling_denom(unsigned netid, int return_code) {
    std::lock_guard guard(cache_mutex);
    NetConfig* netconfig = find_netconfig_locked(netid);
    if (netconfig == nullptr) return 0;  // Don't log anything at all.
    const auto& subsampling_map = netconfig->dns_event_subsampling_map;
    auto search_returnCode = subsampling_map.find(return_code);
    uint32_t denom;
    if (search_returnCode != subsampling_map.end()) {
        denom = search_returnCode->second;
    } else {
        auto search_default = subsampling_map.find(DNSEVENT_SUBSAMPLING_MAP_DEFAULT_KEY);
        denom = (search_default == subsampling_map.end()) ? 0 : search_default->second;
    }
    return denom;
}

int resolv_cache_get_resolver_stats(unsigned netid, res_params* params, res_stats stats[MAXNS],
                                    const std::vector<IPSockAddr>& serverSockAddrs) {
    std::lock_guard guard(cache_mutex);
    NetConfig* info = find_netconfig_locked(netid);
    if (!info) return -1;

    for (size_t i = 0; i < serverSockAddrs.size(); i++) {
        for (size_t j = 0; j < info->nameserverSockAddrs.size(); j++) {
            // Should never happen. Just in case because of the fix-sized array |stats|.
            if (j >= MAXNS) {
                LOG(WARNING) << __func__ << ": unexpected size " << j;
                return -1;
            }

            // It's possible that the server is not found, e.g. when a new list of nameservers
            // is updated to the NetConfig just after this look up thread being populated.
            // Keep the server valid as-is (by means of keeping stats[i] unset), but we should
            // think about if there's a better way.
            if (info->nameserverSockAddrs[j] == serverSockAddrs[i]) {
                stats[i] = info->nsstats[j];
                break;
            }
        }
    }

    *params = info->params;
    return info->revision_id;
}

void resolv_cache_add_resolver_stats_sample(unsigned netid, int revision_id,
                                            const IPSockAddr& serverSockAddr,
                                            const res_sample& sample, int max_samples) {
    if (max_samples <= 0) return;

    std::lock_guard guard(cache_mutex);
    NetConfig* info = find_netconfig_locked(netid);

    if (info && info->revision_id == revision_id) {
        const int serverNum = std::min(MAXNS, static_cast<int>(info->nameserverSockAddrs.size()));
        for (int ns = 0; ns < serverNum; ns++) {
            if (serverSockAddr == info->nameserverSockAddrs[ns]) {
                res_cache_add_stats_sample_locked(&info->nsstats[ns], sample, max_samples);
                return;
            }
        }
    }
}

bool has_named_cache(unsigned netid) {
    std::lock_guard guard(cache_mutex);
    return find_named_cache_locked(netid) != nullptr;
}

int resolv_cache_get_expiration(unsigned netid, const std::vector<char>& query,
                                time_t* expiration) {
    Entry key;
    *expiration = -1;

    // A malformed query is not allowed.
    if (!entry_init_key(&key, query.data(), query.size())) {
        LOG(WARNING) << __func__ << ": unsupported query";
        return -EINVAL;
    }

    // lookup cache.
    Cache* cache;
    std::lock_guard guard(cache_mutex);
    if (cache = find_named_cache_locked(netid); cache == nullptr) {
        LOG(WARNING) << __func__ << ": cache not created in the network " << netid;
        return -ENONET;
    }
    Entry** lookup = _cache_lookup_p(cache, &key);
    Entry* e = *lookup;
    if (e == NULL) {
        LOG(WARNING) << __func__ << ": not in cache";
        return -ENODATA;
    }

    if (_time_now() >= e->expires) {
        LOG(WARNING) << __func__ << ": entry expired";
        return -ENODATA;
    }

    *expiration = e->expires;
    return 0;
}

int resolv_stats_set_servers_for_dot(unsigned netid, const std::vector<std::string>& servers) {
    std::lock_guard guard(cache_mutex);
    const auto info = find_netconfig_locked(netid);

    if (info == nullptr) return -ENONET;

    std::vector<IPSockAddr> serverSockAddrs;
    serverSockAddrs.reserve(servers.size());
    for (const auto& server : servers) {
        serverSockAddrs.push_back(IPSockAddr::toIPSockAddr(server, 853));
    }

    if (!info->dnsStats.setServers(serverSockAddrs, android::net::PROTO_DOT)) {
        LOG(WARNING) << __func__ << ": netid = " << netid << ", failed to set dns stats";
        return -EINVAL;
    }

    return 0;
}

bool resolv_stats_add(unsigned netid, const android::netdutils::IPSockAddr& server,
                      const DnsQueryEvent* record) {
    if (record == nullptr) return false;

    std::lock_guard guard(cache_mutex);
    if (const auto info = find_netconfig_locked(netid); info != nullptr) {
        return info->dnsStats.addStats(server, *record);
    }
    return false;
}

static const char* tc_mode_to_str(const int mode) {
    switch (mode) {
        case aidl::android::net::IDnsResolver::TC_MODE_DEFAULT:
            return "default";
        case aidl::android::net::IDnsResolver::TC_MODE_UDP_TCP:
            return "UDP_TCP";
        default:
            return "unknown";
    }
}

static android::net::NetworkType to_stats_network_type(int32_t mainType, bool withVpn) {
    switch (mainType) {
        case IDnsResolver::TRANSPORT_CELLULAR:
            return withVpn ? android::net::NT_CELLULAR_VPN : android::net::NT_CELLULAR;
        case IDnsResolver::TRANSPORT_WIFI:
            return withVpn ? android::net::NT_WIFI_VPN : android::net::NT_WIFI;
        case IDnsResolver::TRANSPORT_BLUETOOTH:
            return withVpn ? android::net::NT_BLUETOOTH_VPN : android::net::NT_BLUETOOTH;
        case IDnsResolver::TRANSPORT_ETHERNET:
            return withVpn ? android::net::NT_ETHERNET_VPN : android::net::NT_ETHERNET;
        case IDnsResolver::TRANSPORT_VPN:
            return withVpn ? android::net::NT_UNKNOWN : android::net::NT_VPN;
        case IDnsResolver::TRANSPORT_WIFI_AWARE:
            return withVpn ? android::net::NT_UNKNOWN : android::net::NT_WIFI_AWARE;
        case IDnsResolver::TRANSPORT_LOWPAN:
            return withVpn ? android::net::NT_UNKNOWN : android::net::NT_LOWPAN;
        default:
            return android::net::NT_UNKNOWN;
    }
}

android::net::NetworkType convert_network_type(const std::vector<int32_t>& transportTypes) {
    // The valid transportTypes size is 1 to 3.
    if (transportTypes.size() > 3 || transportTypes.size() == 0) return android::net::NT_UNKNOWN;
    // TransportTypes size == 1, map the type to stats network type directly.
    if (transportTypes.size() == 1) return to_stats_network_type(transportTypes[0], false);
    // TransportTypes size == 3, only cellular + wifi + vpn is valid.
    if (transportTypes.size() == 3) {
        std::vector<int32_t> sortedTransTypes = transportTypes;
        std::sort(sortedTransTypes.begin(), sortedTransTypes.end());
        if (sortedTransTypes != std::vector<int32_t>{IDnsResolver::TRANSPORT_CELLULAR,
                                                     IDnsResolver::TRANSPORT_WIFI,
                                                     IDnsResolver::TRANSPORT_VPN}) {
            return android::net::NT_UNKNOWN;
        }
        return android::net::NT_WIFI_CELLULAR_VPN;
    }
    // TransportTypes size == 2, it shoud be 1 main type + vpn type.
    // Otherwise, consider it as UNKNOWN.
    bool hasVpn = false;
    int32_t mainType = IDnsResolver::TRANSPORT_UNKNOWN;
    for (const auto& transportType : transportTypes) {
        if (transportType == IDnsResolver::TRANSPORT_VPN) {
            hasVpn = true;
            continue;
        }
        mainType = transportType;
    }
    return hasVpn ? to_stats_network_type(mainType, true) : android::net::NT_UNKNOWN;
}

static const char* transport_type_to_str(const std::vector<int32_t>& transportTypes) {
    switch (convert_network_type(transportTypes)) {
        case android::net::NT_CELLULAR:
            return "CELLULAR";
        case android::net::NT_WIFI:
            return "WIFI";
        case android::net::NT_BLUETOOTH:
            return "BLUETOOTH";
        case android::net::NT_ETHERNET:
            return "ETHERNET";
        case android::net::NT_VPN:
            return "VPN";
        case android::net::NT_WIFI_AWARE:
            return "WIFI_AWARE";
        case android::net::NT_LOWPAN:
            return "LOWPAN";
        case android::net::NT_CELLULAR_VPN:
            return "CELLULAR_VPN";
        case android::net::NT_WIFI_VPN:
            return "WIFI_VPN";
        case android::net::NT_BLUETOOTH_VPN:
            return "BLUETOOTH_VPN";
        case android::net::NT_ETHERNET_VPN:
            return "ETHERNET_VPN";
        case android::net::NT_WIFI_CELLULAR_VPN:
            return "WIFI_CELLULAR_VPN";
        default:
            return "UNKNOWN";
    }
}

void resolv_netconfig_dump(DumpWriter& dw, unsigned netid) {
    std::lock_guard guard(cache_mutex);
    if (const auto info = find_netconfig_locked(netid); info != nullptr) {
        info->dnsStats.dump(dw);
        // TODO: dump info->hosts
        dw.println("TC mode: %s", tc_mode_to_str(info->tc_mode));
        dw.println("TransportType: %s", transport_type_to_str(info->transportTypes));
    }
}
