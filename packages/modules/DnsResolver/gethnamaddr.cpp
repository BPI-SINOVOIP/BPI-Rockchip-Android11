/*	$NetBSD: gethnamaddr.c,v 1.91 2014/06/19 15:08:18 christos Exp $	*/

/*
 * ++Copyright++ 1985, 1988, 1993
 * -
 * Copyright (c) 1985, 1988, 1993
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
 * -
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
 * -
 * --Copyright--
 */

#include "gethnamaddr.h"

#include <android-base/logging.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <functional>
#include <vector>

#include "hostent.h"
#include "netd_resolv/resolv.h"
#include "res_comp.h"
#include "res_debug.h"  // p_class(), p_type()
#include "res_init.h"
#include "resolv_cache.h"
#include "stats.pb.h"

using android::net::NetworkDnsEventReported;

// NetBSD uses _DIAGASSERT to null-check arguments and the like,
// but it's clear from the number of mistakes in their assertions
// that they don't actually test or ship with this.
#define _DIAGASSERT(e) /* nothing */

// TODO: unify macro ALIGNBYTES and ALIGN for all possible data type alignment of hostent
// buffer.
#define ALIGNBYTES (sizeof(uintptr_t) - 1)
#define ALIGN(p) (((uintptr_t)(p) + ALIGNBYTES) & ~ALIGNBYTES)

constexpr int MAXADDRS = 35;

typedef union {
    HEADER hdr;
    uint8_t buf[MAXPACKET];
} querybuf;

typedef union {
    int32_t al;
    char ac;
} align;

static void convert_v4v6_hostent(struct hostent* hp, char** bpp, char* ep,
                                 const std::function<void(struct hostent* hp)>& mapping_param,
                                 const std::function<void(char* src, char* dst)>& mapping_addr);
static void pad_v4v6_hostent(struct hostent* hp, char** bpp, char* ep);

static int dns_gethtbyaddr(const unsigned char* uaddr, int len, int af,
                           const android_net_context* netcontext, getnamaddr* info,
                           NetworkDnsEventReported* event);
static int dns_gethtbyname(ResState* res, const char* name, int af, getnamaddr* info);

#define BOUNDED_INCR(x)      \
    do {                     \
        BOUNDS_CHECK(cp, x); \
        cp += (x);           \
    } while (0)

#define BOUNDS_CHECK(ptr, count)                     \
    do {                                             \
        if (eom - (ptr) < (count)) goto no_recovery; \
    } while (0)

static struct hostent* getanswer(const querybuf* answer, int anslen, const char* qname, int qtype,
                                 struct hostent* hent, char* buf, size_t buflen, int* he) {
    const HEADER* hp;
    const uint8_t* cp;
    int n;
    size_t qlen;
    const uint8_t *eom, *erdata;
    char *bp, **hap, *ep;
    int ancount, qdcount;
    int haveanswer, had_error;
    int toobig = 0;
    char tbuf[MAXDNAME];
    char* addr_ptrs[MAXADDRS];
    const char* tname;
    std::vector<char*> aliases;

    _DIAGASSERT(answer != NULL);
    _DIAGASSERT(qname != NULL);

    tname = qname;
    hent->h_name = NULL;
    eom = answer->buf + anslen;

    bool (*name_ok)(const char* dn);
    switch (qtype) {
        case T_A:
        case T_AAAA:
            name_ok = res_hnok;
            break;
        case T_PTR:
            name_ok = res_dnok;
            break;
        default:
            *he = NO_RECOVERY;
            return NULL; /* XXX should be abort(); */
    }

    /*
     * find first satisfactory answer
     */
    hp = &answer->hdr;
    ancount = ntohs(hp->ancount);
    qdcount = ntohs(hp->qdcount);
    bp = buf;
    ep = buf + buflen;
    cp = answer->buf;
    BOUNDED_INCR(HFIXEDSZ);
    if (qdcount != 1) goto no_recovery;

    n = dn_expand(answer->buf, eom, cp, bp, (int) (ep - bp));
    if ((n < 0) || !name_ok(bp)) goto no_recovery;

    BOUNDED_INCR(n + QFIXEDSZ);
    if (qtype == T_A || qtype == T_AAAA) {
        /* res_send() has already verified that the query name is the
         * same as the one we sent; this just gets the expanded name
         * (i.e., with the succeeding search-domain tacked on).
         */
        n = (int) strlen(bp) + 1; /* for the \0 */
        if (n >= MAXHOSTNAMELEN) goto no_recovery;
        hent->h_name = bp;
        bp += n;
        /* The qname can be abbreviated, but h_name is now absolute. */
        qname = hent->h_name;
    }
    hent->h_addr_list = hap = addr_ptrs;
    *hap = NULL;
    haveanswer = 0;
    had_error = 0;
    while (ancount-- > 0 && cp < eom && !had_error) {
        n = dn_expand(answer->buf, eom, cp, bp, (int) (ep - bp));
        if ((n < 0) || !name_ok(bp)) {
            had_error++;
            continue;
        }
        cp += n; /* name */
        BOUNDS_CHECK(cp, 3 * INT16SZ + INT32SZ);
        int type = ntohs(*reinterpret_cast<const uint16_t*>(cp));
        cp += INT16SZ; /* type */
        int cl = ntohs(*reinterpret_cast<const uint16_t*>(cp));
        cp += INT16SZ + INT32SZ; /* class, TTL */
        n = ntohs(*reinterpret_cast<const uint16_t*>(cp));
        cp += INT16SZ; /* len */
        BOUNDS_CHECK(cp, n);
        erdata = cp + n;
        if (cl != C_IN) {
            /* XXX - debug? syslog? */
            cp += n;
            continue; /* XXX - had_error++ ? */
        }
        if ((qtype == T_A || qtype == T_AAAA) && type == T_CNAME) {
            n = dn_expand(answer->buf, eom, cp, tbuf, (int) sizeof tbuf);
            if ((n < 0) || !name_ok(tbuf)) {
                had_error++;
                continue;
            }
            cp += n;
            if (cp != erdata) goto no_recovery;
            /* Store alias. */
            aliases.push_back(bp);
            n = (int) strlen(bp) + 1; /* for the \0 */
            if (n >= MAXHOSTNAMELEN) {
                had_error++;
                continue;
            }
            bp += n;
            /* Get canonical name. */
            n = (int) strlen(tbuf) + 1; /* for the \0 */
            if (n > ep - bp || n >= MAXHOSTNAMELEN) {
                had_error++;
                continue;
            }
            strlcpy(bp, tbuf, (size_t)(ep - bp));
            hent->h_name = bp;
            bp += n;
            continue;
        }
        if (qtype == T_PTR && type == T_CNAME) {
            n = dn_expand(answer->buf, eom, cp, tbuf, (int) sizeof tbuf);
            if (n < 0 || !res_dnok(tbuf)) {
                had_error++;
                continue;
            }
            cp += n;
            if (cp != erdata) goto no_recovery;
            /* Get canonical name. */
            n = (int) strlen(tbuf) + 1; /* for the \0 */
            if (n > ep - bp || n >= MAXHOSTNAMELEN) {
                had_error++;
                continue;
            }
            strlcpy(bp, tbuf, (size_t)(ep - bp));
            tname = bp;
            bp += n;
            continue;
        }
        if (type != qtype) {
            if (type != T_KEY && type != T_SIG)
                LOG(DEBUG) << __func__ << ": asked for \"" << qname << " " << p_class(C_IN) << " "
                           << p_type(qtype) << "\", got type \"" << p_type(type) << "\"";
            cp += n;
            continue; /* XXX - had_error++ ? */
        }
        switch (type) {
            case T_PTR:
                if (strcasecmp(tname, bp) != 0) {
                    LOG(DEBUG) << __func__ << ": asked for \"" << qname << "\", got \"" << bp
                               << "\"";
                    cp += n;
                    continue; /* XXX - had_error++ ? */
                }
                n = dn_expand(answer->buf, eom, cp, bp, (int) (ep - bp));
                if ((n < 0) || !res_hnok(bp)) {
                    had_error++;
                    break;
                }
                cp += n;
                if (cp != erdata) goto no_recovery;
                if (!haveanswer)
                    hent->h_name = bp;
                else
                    aliases.push_back(bp);
                if (n != -1) {
                    n = (int) strlen(bp) + 1; /* for the \0 */
                    if (n >= MAXHOSTNAMELEN) {
                        had_error++;
                        break;
                    }
                    bp += n;
                }
                break;
            case T_A:
            case T_AAAA:
                if (strcasecmp(hent->h_name, bp) != 0) {
                    LOG(DEBUG) << __func__ << ": asked for \"" << hent->h_name << "\", got \"" << bp
                               << "\"";
                    cp += n;
                    continue; /* XXX - had_error++ ? */
                }
                if (n != hent->h_length) {
                    cp += n;
                    continue;
                }
                if (type == T_AAAA) {
                    struct in6_addr in6;
                    memcpy(&in6, cp, NS_IN6ADDRSZ);
                    if (IN6_IS_ADDR_V4MAPPED(&in6)) {
                        cp += n;
                        continue;
                    }
                }
                if (!haveanswer) {
                    int nn;

                    hent->h_name = bp;
                    nn = (int) strlen(bp) + 1; /* for the \0 */
                    bp += nn;
                }

                bp += sizeof(align) - (size_t)((uintptr_t)bp % sizeof(align));

                if (bp + n >= ep) {
                    LOG(DEBUG) << __func__ << ": size (" << n << ") too big";
                    had_error++;
                    continue;
                }
                if (hap >= &addr_ptrs[MAXADDRS - 1]) {
                    if (!toobig++) {
                        LOG(DEBUG) << __func__ << ": Too many addresses (" << MAXADDRS << ")";
                    }
                    cp += n;
                    continue;
                }
                (void) memcpy(*hap++ = bp, cp, (size_t) n);
                bp += n;
                cp += n;
                if (cp != erdata) goto no_recovery;
                break;
            default:
                abort();
        }
        if (!had_error) haveanswer++;
    }
    if (haveanswer) {
        *hap = NULL;
        if (!hent->h_name) {
            n = (int) strlen(qname) + 1; /* for the \0 */
            if (n > ep - bp || n >= MAXHOSTNAMELEN) goto no_recovery;
            strlcpy(bp, qname, (size_t)(ep - bp));
            hent->h_name = bp;
            bp += n;
        }
        if (hent->h_addrtype == AF_INET) pad_v4v6_hostent(hent, &bp, ep);
        goto success;
    }
no_recovery:
    *he = NO_RECOVERY;
    return NULL;
success:
    bp = (char*) ALIGN(bp);
    aliases.push_back(nullptr);
    qlen = aliases.size() * sizeof(*hent->h_aliases);
    if ((size_t)(ep - bp) < qlen) goto nospc;
    hent->h_aliases = (char**) bp;
    memcpy(bp, aliases.data(), qlen);

    bp += qlen;
    n = (int) (hap - addr_ptrs);
    qlen = (n + 1) * sizeof(*hent->h_addr_list);
    if ((size_t)(ep - bp) < qlen) goto nospc;
    hent->h_addr_list = (char**) bp;
    memcpy(bp, addr_ptrs, qlen);
    *he = NETDB_SUCCESS;
    return hent;
nospc:
    errno = ENOSPC;
    *he = NETDB_INTERNAL;
    return NULL;
}

int resolv_gethostbyname(const char* name, int af, hostent* hp, char* buf, size_t buflen,
                         const android_net_context* netcontext, hostent** result,
                         NetworkDnsEventReported* event) {
    getnamaddr info;

    ResState res;
    res_init(&res, netcontext, event);

    size_t size;
    switch (af) {
        case AF_INET:
            size = NS_INADDRSZ;
            break;
        case AF_INET6:
            size = NS_IN6ADDRSZ;
            break;
        default:
            return EAI_FAMILY;
    }
    if (buflen < size) goto nospc;

    hp->h_addrtype = af;
    hp->h_length = (int) size;

    /*
     * disallow names consisting only of digits/dots, unless
     * they end in a dot.
     */
    if (isdigit((uint8_t)name[0])) {
        for (const char* cp = name;; ++cp) {
            if (!*cp) {
                if (*--cp == '.') break;
                /*
                 * All-numeric, no dot at the end.
                 * Fake up a hostent as if we'd actually
                 * done a lookup.
                 */
                goto fake;
            }
            if (!isdigit((uint8_t)*cp) && *cp != '.') break;
        }
    }
    if ((isxdigit((uint8_t)name[0]) && strchr(name, ':') != NULL) || name[0] == ':') {
        for (const char* cp = name;; ++cp) {
            if (!*cp) {
                if (*--cp == '.') break;
                /*
                 * All-IPv6-legal, no dot at the end.
                 * Fake up a hostent as if we'd actually
                 * done a lookup.
                 */
                goto fake;
            }
            if (!isxdigit((uint8_t)*cp) && *cp != ':' && *cp != '.') break;
        }
    }

    info.hp = hp;
    info.buf = buf;
    info.buflen = buflen;
    if (_hf_gethtbyname2(name, af, &info)) {
        int error = dns_gethtbyname(&res, name, af, &info);
        if (error != 0) return error;
    }
    *result = hp;
    return 0;
nospc:
    return EAI_MEMORY;
fake:
    HENT_ARRAY(hp->h_addr_list, 1, buf, buflen);
    HENT_ARRAY(hp->h_aliases, 0, buf, buflen);

    hp->h_aliases[0] = NULL;
    if (size > buflen) goto nospc;

    if (inet_pton(af, name, buf) <= 0) {
        return EAI_NODATA;
    }
    hp->h_addr_list[0] = buf;
    hp->h_addr_list[1] = NULL;
    buf += size;
    buflen -= size;
    HENT_SCOPY(hp->h_name, name, buf, buflen);
    *result = hp;
    return 0;
}

int resolv_gethostbyaddr(const void* addr, socklen_t len, int af, hostent* hp, char* buf,
                         size_t buflen, const struct android_net_context* netcontext,
                         hostent** result, NetworkDnsEventReported* event) {
    const uint8_t* uaddr = (const uint8_t*)addr;
    socklen_t size;
    struct getnamaddr info;

    _DIAGASSERT(addr != NULL);

    if (af == AF_INET6 && len == NS_IN6ADDRSZ &&
        (IN6_IS_ADDR_LINKLOCAL((const struct in6_addr*) addr) ||
         IN6_IS_ADDR_SITELOCAL((const struct in6_addr*) addr))) {
        return EAI_NODATA;
    }
    if (af == AF_INET6 && len == NS_IN6ADDRSZ &&
        (IN6_IS_ADDR_V4MAPPED((const struct in6_addr*) addr) ||
         IN6_IS_ADDR_V4COMPAT((const struct in6_addr*) addr))) {
        /* Unmap. */
        uaddr += NS_IN6ADDRSZ - NS_INADDRSZ;
        addr = uaddr;
        af = AF_INET;
        len = NS_INADDRSZ;
    }
    switch (af) {
        case AF_INET:
            size = NS_INADDRSZ;
            break;
        case AF_INET6:
            size = NS_IN6ADDRSZ;
            break;
        default:
            return EAI_FAMILY;
    }
    if (size != len) {
        // TODO: Consider converting to a private extended EAI_* error code.
        // Currently, the EAI_* value has no corresponding error code for invalid argument socket
        // length. In order to not rely on errno, convert the original error code pair, EAI_SYSTEM
        // and EINVAL, to EAI_FAIL.
        return EAI_FAIL;
    }
    info.hp = hp;
    info.buf = buf;
    info.buflen = buflen;
    if (_hf_gethtbyaddr(uaddr, len, af, &info)) {
        int error = dns_gethtbyaddr(uaddr, len, af, netcontext, &info, event);
        if (error != 0) return error;
    }
    *result = hp;
    return 0;
}

// TODO: Consider leaving function without returning error code as _gethtent() does because
// the error code of the caller does not currently return to netd.
struct hostent* netbsd_gethostent_r(FILE* hf, struct hostent* hent, char* buf, size_t buflen,
                                    int* he) {
    char *name;
    char* cp;
    int af, len;
    size_t anum;
    struct in6_addr host_addr;
    std::vector<char*> aliases;

    if (hf == NULL) {
        *he = NETDB_INTERNAL;
        errno = EINVAL;
        return NULL;
    }
    char* p = NULL;

    // Allocate a new space to read file lines like upstream does.
    const size_t line_buf_size = MAXPACKET;
    if ((p = (char*) malloc(line_buf_size)) == NULL) {
        goto nospc;
    }
    for (;;) {
        if (!fgets(p, line_buf_size, hf)) {
            free(p);
            *he = HOST_NOT_FOUND;
            return NULL;
        }
        if (*p == '#') {
            continue;
        }
        if (!(cp = strpbrk(p, "#\n"))) {
            continue;
        }
        *cp = '\0';
        if (!(cp = strpbrk(p, " \t"))) continue;
        *cp++ = '\0';
        if (inet_pton(AF_INET6, p, &host_addr) > 0) {
            af = AF_INET6;
            len = NS_IN6ADDRSZ;
        } else {
            if (inet_pton(AF_INET, p, &host_addr) <= 0) continue;
            af = AF_INET;
            len = NS_INADDRSZ;
        }

        /* if this is not something we're looking for, skip it. */
        if (hent->h_addrtype != 0 && hent->h_addrtype != af) continue;
        if (hent->h_length != 0 && hent->h_length != len) continue;

        while (*cp == ' ' || *cp == '\t') cp++;
        if ((cp = strpbrk(name = cp, " \t")) != NULL) *cp++ = '\0';
        while (cp && *cp) {
            if (*cp == ' ' || *cp == '\t') {
                cp++;
                continue;
            }
            aliases.push_back(cp);
            if ((cp = strpbrk(cp, " \t")) != NULL) *cp++ = '\0';
        }
        break;
    }
    hent->h_length = len;
    hent->h_addrtype = af;
    HENT_ARRAY(hent->h_addr_list, 1, buf, buflen);
    anum = aliases.size();
    HENT_ARRAY(hent->h_aliases, anum, buf, buflen);
    HENT_COPY(hent->h_addr_list[0], &host_addr, hent->h_length, buf, buflen);
    hent->h_addr_list[1] = NULL;

    /* Reserve space for mapping IPv4 address to IPv6 address in place */
    if (hent->h_addrtype == AF_INET) {
        HENT_COPY(buf, NAT64_PAD, sizeof(NAT64_PAD), buf, buflen);
    }

    HENT_SCOPY(hent->h_name, name, buf, buflen);
    for (size_t i = 0; i < anum; i++) HENT_SCOPY(hent->h_aliases[i], aliases[i], buf, buflen);
    hent->h_aliases[anum] = NULL;
    *he = NETDB_SUCCESS;
    free(p);
    return hent;
nospc:
    free(p);
    errno = ENOSPC;
    *he = NETDB_INTERNAL;
    return NULL;
}

static void convert_v4v6_hostent(struct hostent* hp, char** bpp, char* ep,
                                 const std::function<void(struct hostent* hp)>& map_param,
                                 const std::function<void(char* src, char* dst)>& map_addr) {
    _DIAGASSERT(hp != NULL);
    _DIAGASSERT(bpp != NULL);
    _DIAGASSERT(ep != NULL);

    if (hp->h_addrtype != AF_INET || hp->h_length != NS_INADDRSZ) return;
    map_param(hp);
    for (char** ap = hp->h_addr_list; *ap; ap++) {
        int i = (int)(sizeof(align) - (size_t)((uintptr_t)*bpp % sizeof(align)));

        if (ep - *bpp < (i + NS_IN6ADDRSZ)) {
            /* Out of memory.  Truncate address list here.  XXX */
            *ap = NULL;
            return;
        }
        *bpp += i;
        map_addr(*ap, *bpp);
        *ap = *bpp;
        *bpp += NS_IN6ADDRSZ;
    }
}

/* Reserve space for mapping IPv4 address to IPv6 address in place */
static void pad_v4v6_hostent(struct hostent* hp, char** bpp, char* ep) {
    convert_v4v6_hostent(hp, bpp, ep,
                         [](struct hostent* hp) {
                             (void) hp; /* unused */
                         },
                         [](char* src, char* dst) {
                             memcpy(dst, src, NS_INADDRSZ);
                             memcpy(dst + NS_INADDRSZ, NAT64_PAD, sizeof(NAT64_PAD));
                         });
}

static int dns_gethtbyname(ResState* res, const char* name, int addr_type, getnamaddr* info) {
    int n, type;
    info->hp->h_addrtype = addr_type;

    switch (info->hp->h_addrtype) {
        case AF_INET:
            info->hp->h_length = NS_INADDRSZ;
            type = T_A;
            break;
        case AF_INET6:
            info->hp->h_length = NS_IN6ADDRSZ;
            type = T_AAAA;
            break;
        default:
            return EAI_FAMILY;
    }
    auto buf = std::make_unique<querybuf>();

    int he;
    n = res_nsearch(res, name, C_IN, type, buf->buf, (int)sizeof(buf->buf), &he);
    if (n < 0) {
        LOG(DEBUG) << __func__ << ": res_nsearch failed (" << n << ")";
        // Return h_errno (he) to catch more detailed errors rather than EAI_NODATA.
        // Note that res_nsearch() doesn't set the pair NETDB_INTERNAL and errno.
        // See also herrnoToAiErrno().
        return herrnoToAiErrno(he);
    }
    hostent* hp = getanswer(buf.get(), n, name, type, info->hp, info->buf, info->buflen, &he);
    if (hp == NULL) return herrnoToAiErrno(he);

    return 0;
}

static int dns_gethtbyaddr(const unsigned char* uaddr, int len, int af,
                           const android_net_context* netcontext, getnamaddr* info,
                           NetworkDnsEventReported* event) {
    char qbuf[MAXDNAME + 1], *qp, *ep;
    int n;
    int advance;

    info->hp->h_length = len;
    info->hp->h_addrtype = af;

    switch (info->hp->h_addrtype) {
        case AF_INET:
            (void) snprintf(qbuf, sizeof(qbuf), "%u.%u.%u.%u.in-addr.arpa", (uaddr[3] & 0xff),
                            (uaddr[2] & 0xff), (uaddr[1] & 0xff), (uaddr[0] & 0xff));
            break;

        case AF_INET6:
            qp = qbuf;
            ep = qbuf + sizeof(qbuf) - 1;
            for (n = NS_IN6ADDRSZ - 1; n >= 0; n--) {
                advance = snprintf(qp, (size_t)(ep - qp), "%x.%x.", uaddr[n] & 0xf,
                                   ((unsigned int) uaddr[n] >> 4) & 0xf);
                if (advance > 0 && qp + advance < ep)
                    qp += advance;
                else {
                    // TODO: Consider converting to a private extended EAI_* error code.
                    // Currently, the EAI_* value has no corresponding error code for an internal
                    // out of buffer space. In order to not rely on errno, convert the original
                    // error code EAI_SYSTEM to EAI_MEMORY.
                    return EAI_MEMORY;
                }
            }
            if (strlcat(qbuf, "ip6.arpa", sizeof(qbuf)) >= sizeof(qbuf)) {
                // TODO: Consider converting to a private extended EAI_* error code.
                // Currently, the EAI_* value has no corresponding error code for an internal
                // out of buffer space. In order to not rely on errno, convert the original
                // error code EAI_SYSTEM to EAI_MEMORY.
                return EAI_MEMORY;
            }
            break;
        default:
            return EAI_FAMILY;
    }

    auto buf = std::make_unique<querybuf>();

    ResState res;
    res_init(&res, netcontext, event);
    int he;
    n = res_nquery(&res, qbuf, C_IN, T_PTR, buf->buf, (int)sizeof(buf->buf), &he);
    if (n < 0) {
        LOG(DEBUG) << __func__ << ": res_nquery failed (" << n << ")";
        // Note that res_nquery() doesn't set the pair NETDB_INTERNAL and errno.
        // Return h_errno (he) to catch more detailed errors rather than EAI_NODATA.
        // See also herrnoToAiErrno().
        return herrnoToAiErrno(he);
    }
    hostent* hp = getanswer(buf.get(), n, qbuf, T_PTR, info->hp, info->buf, info->buflen, &he);
    if (hp == NULL) return herrnoToAiErrno(he);

    char* bf = (char*) (hp->h_addr_list + 2);
    size_t blen = (size_t)(bf - info->buf);
    if (blen + info->hp->h_length > info->buflen) goto nospc;
    hp->h_addr_list[0] = bf;
    hp->h_addr_list[1] = NULL;
    memcpy(bf, uaddr, (size_t) info->hp->h_length);

    /* Reserve enough space for mapping IPv4 address to IPv6 address in place */
    if (info->hp->h_addrtype == AF_INET) {
        if (blen + NS_IN6ADDRSZ > info->buflen) goto nospc;
        // Pad zero to the unused address space
        memcpy(bf + NS_INADDRSZ, NAT64_PAD, sizeof(NAT64_PAD));
    }

    return 0;

nospc:
    return EAI_MEMORY;
}

int herrnoToAiErrno(int he) {
    switch (he) {
        // extended h_errno
        case NETD_RESOLV_H_ERRNO_EXT_TIMEOUT:
            return NETD_RESOLV_TIMEOUT;
        // legacy h_errno
        case NETDB_SUCCESS:
            return 0;
        case HOST_NOT_FOUND:  // TODO: Perhaps convert HOST_NOT_FOUND to EAI_NONAME instead
        case NO_DATA:         // NO_ADDRESS
            return EAI_NODATA;
        case TRY_AGAIN:
            return EAI_AGAIN;
        case NETDB_INTERNAL:
            // TODO: Remove ENOSPC and call abort() immediately whenever any allocation fails.
            if (errno == ENOSPC) return EAI_MEMORY;
            // Theoretically, this should not happen. Leave this here just in case.
            // Currently, getanswer() of {gethnamaddr, getaddrinfo}.cpp, res_nsearch() and
            // res_searchN() use this function to convert error code. Only getanswer()
            // of gethnamaddr.cpp may return the error code pair, herrno NETDB_INTERNAL and
            // errno ENOSPC, which has already converted to EAI_MEMORY. The remaining functions
            // don't set the pair herrno and errno.
            return EAI_SYSTEM;  // see errno for detail
        case NO_RECOVERY:
        default:
            return EAI_FAIL;  // TODO: Perhaps convert default to EAI_MAX (unknown error) instead
    }
}
