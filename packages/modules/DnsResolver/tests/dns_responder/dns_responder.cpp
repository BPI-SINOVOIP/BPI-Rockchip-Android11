/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "dns_responder.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <set>
#include <vector>

#define LOG_TAG "DNSResponder"
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <netdutils/InternetAddresses.h>
#include <netdutils/Slice.h>
#include <netdutils/SocketOption.h>

using android::netdutils::enableSockopt;
using android::netdutils::ScopedAddrinfo;
using android::netdutils::Slice;

namespace test {

std::string errno2str() {
    char error_msg[512] = {0};
    // It actually calls __gnu_strerror_r() which returns the type |char*| rather than |int|.
    // PLOG is an option though it requires lots of changes from ALOGx() to LOG(x).
    return strerror_r(errno, error_msg, sizeof(error_msg));
}

std::string str2hex(const char* buffer, size_t len) {
    std::string str(len * 2, '\0');
    for (size_t i = 0; i < len; ++i) {
        static const char* hex = "0123456789ABCDEF";
        uint8_t c = buffer[i];
        str[i * 2] = hex[c >> 4];
        str[i * 2 + 1] = hex[c & 0x0F];
    }
    return str;
}

std::string addr2str(const sockaddr* sa, socklen_t sa_len) {
    char host_str[NI_MAXHOST] = {0};
    int rv = getnameinfo(sa, sa_len, host_str, sizeof(host_str), nullptr, 0, NI_NUMERICHOST);
    if (rv == 0) return std::string(host_str);
    return std::string();
}

/* DNS struct helpers */

const char* dnstype2str(unsigned dnstype) {
    static std::unordered_map<unsigned, const char*> kTypeStrs = {
            {ns_type::ns_t_a, "A"},
            {ns_type::ns_t_ns, "NS"},
            {ns_type::ns_t_md, "MD"},
            {ns_type::ns_t_mf, "MF"},
            {ns_type::ns_t_cname, "CNAME"},
            {ns_type::ns_t_soa, "SOA"},
            {ns_type::ns_t_mb, "MB"},
            {ns_type::ns_t_mb, "MG"},
            {ns_type::ns_t_mr, "MR"},
            {ns_type::ns_t_null, "NULL"},
            {ns_type::ns_t_wks, "WKS"},
            {ns_type::ns_t_ptr, "PTR"},
            {ns_type::ns_t_hinfo, "HINFO"},
            {ns_type::ns_t_minfo, "MINFO"},
            {ns_type::ns_t_mx, "MX"},
            {ns_type::ns_t_txt, "TXT"},
            {ns_type::ns_t_rp, "RP"},
            {ns_type::ns_t_afsdb, "AFSDB"},
            {ns_type::ns_t_x25, "X25"},
            {ns_type::ns_t_isdn, "ISDN"},
            {ns_type::ns_t_rt, "RT"},
            {ns_type::ns_t_nsap, "NSAP"},
            {ns_type::ns_t_nsap_ptr, "NSAP-PTR"},
            {ns_type::ns_t_sig, "SIG"},
            {ns_type::ns_t_key, "KEY"},
            {ns_type::ns_t_px, "PX"},
            {ns_type::ns_t_gpos, "GPOS"},
            {ns_type::ns_t_aaaa, "AAAA"},
            {ns_type::ns_t_loc, "LOC"},
            {ns_type::ns_t_nxt, "NXT"},
            {ns_type::ns_t_eid, "EID"},
            {ns_type::ns_t_nimloc, "NIMLOC"},
            {ns_type::ns_t_srv, "SRV"},
            {ns_type::ns_t_naptr, "NAPTR"},
            {ns_type::ns_t_kx, "KX"},
            {ns_type::ns_t_cert, "CERT"},
            {ns_type::ns_t_a6, "A6"},
            {ns_type::ns_t_dname, "DNAME"},
            {ns_type::ns_t_sink, "SINK"},
            {ns_type::ns_t_opt, "OPT"},
            {ns_type::ns_t_apl, "APL"},
            {ns_type::ns_t_tkey, "TKEY"},
            {ns_type::ns_t_tsig, "TSIG"},
            {ns_type::ns_t_ixfr, "IXFR"},
            {ns_type::ns_t_axfr, "AXFR"},
            {ns_type::ns_t_mailb, "MAILB"},
            {ns_type::ns_t_maila, "MAILA"},
            {ns_type::ns_t_any, "ANY"},
            {ns_type::ns_t_zxfr, "ZXFR"},
    };
    auto it = kTypeStrs.find(dnstype);
    static const char* kUnknownStr{"UNKNOWN"};
    if (it == kTypeStrs.end()) return kUnknownStr;
    return it->second;
}

const char* dnsclass2str(unsigned dnsclass) {
    static std::unordered_map<unsigned, const char*> kClassStrs = {
            {ns_class::ns_c_in, "Internet"},    {2, "CSNet"},
            {ns_class::ns_c_chaos, "ChaosNet"}, {ns_class::ns_c_hs, "Hesiod"},
            {ns_class::ns_c_none, "none"},      {ns_class::ns_c_any, "any"}};
    auto it = kClassStrs.find(dnsclass);
    static const char* kUnknownStr{"UNKNOWN"};
    if (it == kClassStrs.end()) return kUnknownStr;
    return it->second;
}

const char* dnsproto2str(int protocol) {
    switch (protocol) {
        case IPPROTO_TCP:
            return "TCP";
        case IPPROTO_UDP:
            return "UDP";
        default:
            return "UNKNOWN";
    }
}

const char* DNSName::read(const char* buffer, const char* buffer_end) {
    const char* cur = buffer;
    bool last = false;
    do {
        cur = parseField(cur, buffer_end, &last);
        if (cur == nullptr) {
            LOG(ERROR) << "parsing failed at line " << __LINE__;
            return nullptr;
        }
    } while (!last);
    return cur;
}

char* DNSName::write(char* buffer, const char* buffer_end) const {
    char* buffer_cur = buffer;
    for (size_t pos = 0; pos < name.size();) {
        size_t dot_pos = name.find('.', pos);
        if (dot_pos == std::string::npos) {
            // Sanity check, should never happen unless parseField is broken.
            LOG(ERROR) << "logic error: all names are expected to end with a '.'";
            return nullptr;
        }
        const size_t len = dot_pos - pos;
        if (len >= 256) {
            LOG(ERROR) << "name component '" << name.substr(pos, dot_pos - pos) << "' is " << len
                       << " long, but max is 255";
            return nullptr;
        }
        if (buffer_cur + sizeof(uint8_t) + len > buffer_end) {
            LOG(ERROR) << "buffer overflow at line " << __LINE__;
            return nullptr;
        }
        *buffer_cur++ = len;
        buffer_cur = std::copy(std::next(name.begin(), pos), std::next(name.begin(), dot_pos),
                               buffer_cur);
        pos = dot_pos + 1;
    }
    // Write final zero.
    *buffer_cur++ = 0;
    return buffer_cur;
}

const char* DNSName::parseField(const char* buffer, const char* buffer_end, bool* last) {
    if (buffer + sizeof(uint8_t) > buffer_end) {
        LOG(ERROR) << "parsing failed at line " << __LINE__;
        return nullptr;
    }
    unsigned field_type = *buffer >> 6;
    unsigned ofs = *buffer & 0x3F;
    const char* cur = buffer + sizeof(uint8_t);
    if (field_type == 0) {
        // length + name component
        if (ofs == 0) {
            *last = true;
            return cur;
        }
        if (cur + ofs > buffer_end) {
            LOG(ERROR) << "parsing failed at line " << __LINE__;
            return nullptr;
        }
        name.append(cur, ofs);
        name.push_back('.');
        return cur + ofs;
    } else if (field_type == 3) {
        LOG(ERROR) << "name compression not implemented";
        return nullptr;
    }
    LOG(ERROR) << "invalid name field type";
    return nullptr;
}

const char* DNSQuestion::read(const char* buffer, const char* buffer_end) {
    const char* cur = qname.read(buffer, buffer_end);
    if (cur == nullptr) {
        LOG(ERROR) << "parsing failed at line " << __LINE__;
        return nullptr;
    }
    if (cur + 2 * sizeof(uint16_t) > buffer_end) {
        LOG(ERROR) << "parsing failed at line " << __LINE__;
        return nullptr;
    }
    qtype = ntohs(*reinterpret_cast<const uint16_t*>(cur));
    qclass = ntohs(*reinterpret_cast<const uint16_t*>(cur + sizeof(uint16_t)));
    return cur + 2 * sizeof(uint16_t);
}

char* DNSQuestion::write(char* buffer, const char* buffer_end) const {
    char* buffer_cur = qname.write(buffer, buffer_end);
    if (buffer_cur == nullptr) return nullptr;
    if (buffer_cur + 2 * sizeof(uint16_t) > buffer_end) {
        LOG(ERROR) << "buffer overflow on line " << __LINE__;
        return nullptr;
    }
    *reinterpret_cast<uint16_t*>(buffer_cur) = htons(qtype);
    *reinterpret_cast<uint16_t*>(buffer_cur + sizeof(uint16_t)) = htons(qclass);
    return buffer_cur + 2 * sizeof(uint16_t);
}

std::string DNSQuestion::toString() const {
    char buffer[16384];
    int len = snprintf(buffer, sizeof(buffer), "Q<%s,%s,%s>", qname.name.c_str(),
                       dnstype2str(qtype), dnsclass2str(qclass));
    return std::string(buffer, len);
}

const char* DNSRecord::read(const char* buffer, const char* buffer_end) {
    const char* cur = name.read(buffer, buffer_end);
    if (cur == nullptr) {
        LOG(ERROR) << "parsing failed at line " << __LINE__;
        return nullptr;
    }
    unsigned rdlen = 0;
    cur = readIntFields(cur, buffer_end, &rdlen);
    if (cur == nullptr) {
        LOG(ERROR) << "parsing failed at line " << __LINE__;
        return nullptr;
    }
    if (cur + rdlen > buffer_end) {
        LOG(ERROR) << "parsing failed at line " << __LINE__;
        return nullptr;
    }
    rdata.assign(cur, cur + rdlen);
    return cur + rdlen;
}

char* DNSRecord::write(char* buffer, const char* buffer_end) const {
    char* buffer_cur = name.write(buffer, buffer_end);
    if (buffer_cur == nullptr) return nullptr;
    buffer_cur = writeIntFields(rdata.size(), buffer_cur, buffer_end);
    if (buffer_cur == nullptr) return nullptr;
    if (buffer_cur + rdata.size() > buffer_end) {
        LOG(ERROR) << "buffer overflow on line " << __LINE__;
        return nullptr;
    }
    return std::copy(rdata.begin(), rdata.end(), buffer_cur);
}

std::string DNSRecord::toString() const {
    char buffer[16384];
    int len = snprintf(buffer, sizeof(buffer), "R<%s,%s,%s>", name.name.c_str(), dnstype2str(rtype),
                       dnsclass2str(rclass));
    return std::string(buffer, len);
}

const char* DNSRecord::readIntFields(const char* buffer, const char* buffer_end, unsigned* rdlen) {
    if (buffer + sizeof(IntFields) > buffer_end) {
        LOG(ERROR) << "parsing failed at line " << __LINE__;
        return nullptr;
    }
    const auto& intfields = *reinterpret_cast<const IntFields*>(buffer);
    rtype = ntohs(intfields.rtype);
    rclass = ntohs(intfields.rclass);
    ttl = ntohl(intfields.ttl);
    *rdlen = ntohs(intfields.rdlen);
    return buffer + sizeof(IntFields);
}

char* DNSRecord::writeIntFields(unsigned rdlen, char* buffer, const char* buffer_end) const {
    if (buffer + sizeof(IntFields) > buffer_end) {
        LOG(ERROR) << "buffer overflow on line " << __LINE__;
        return nullptr;
    }
    auto& intfields = *reinterpret_cast<IntFields*>(buffer);
    intfields.rtype = htons(rtype);
    intfields.rclass = htons(rclass);
    intfields.ttl = htonl(ttl);
    intfields.rdlen = htons(rdlen);
    return buffer + sizeof(IntFields);
}

const char* DNSHeader::read(const char* buffer, const char* buffer_end) {
    unsigned qdcount;
    unsigned ancount;
    unsigned nscount;
    unsigned arcount;
    const char* cur = readHeader(buffer, buffer_end, &qdcount, &ancount, &nscount, &arcount);
    if (cur == nullptr) {
        LOG(ERROR) << "parsing failed at line " << __LINE__;
        return nullptr;
    }
    if (qdcount) {
        questions.resize(qdcount);
        for (unsigned i = 0; i < qdcount; ++i) {
            cur = questions[i].read(cur, buffer_end);
            if (cur == nullptr) {
                LOG(ERROR) << "parsing failed at line " << __LINE__;
                return nullptr;
            }
        }
    }
    if (ancount) {
        answers.resize(ancount);
        for (unsigned i = 0; i < ancount; ++i) {
            cur = answers[i].read(cur, buffer_end);
            if (cur == nullptr) {
                LOG(ERROR) << "parsing failed at line " << __LINE__;
                return nullptr;
            }
        }
    }
    if (nscount) {
        authorities.resize(nscount);
        for (unsigned i = 0; i < nscount; ++i) {
            cur = authorities[i].read(cur, buffer_end);
            if (cur == nullptr) {
                LOG(ERROR) << "parsing failed at line " << __LINE__;
                return nullptr;
            }
        }
    }
    if (arcount) {
        additionals.resize(arcount);
        for (unsigned i = 0; i < arcount; ++i) {
            cur = additionals[i].read(cur, buffer_end);
            if (cur == nullptr) {
                LOG(ERROR) << "parsing failed at line " << __LINE__;
                return nullptr;
            }
        }
    }
    return cur;
}

char* DNSHeader::write(char* buffer, const char* buffer_end) const {
    if (buffer + sizeof(Header) > buffer_end) {
        LOG(ERROR) << "buffer overflow on line " << __LINE__;
        return nullptr;
    }
    Header& header = *reinterpret_cast<Header*>(buffer);
    // bytes 0-1
    header.id = htons(id);
    // byte 2: 7:qr, 3-6:opcode, 2:aa, 1:tr, 0:rd
    header.flags0 = (qr << 7) | (opcode << 3) | (aa << 2) | (tr << 1) | rd;
    // byte 3: 7:ra, 6:zero, 5:ad, 4:cd, 0-3:rcode
    // Fake behavior: if the query set the "ad" bit, set it in the response too.
    // In a real server, this should be set only if the data is authentic and the
    // query contained an "ad" bit or DNSSEC extensions.
    header.flags1 = (ad << 5) | rcode;
    // rest of header
    header.qdcount = htons(questions.size());
    header.ancount = htons(answers.size());
    header.nscount = htons(authorities.size());
    header.arcount = htons(additionals.size());
    char* buffer_cur = buffer + sizeof(Header);
    for (const DNSQuestion& question : questions) {
        buffer_cur = question.write(buffer_cur, buffer_end);
        if (buffer_cur == nullptr) return nullptr;
    }
    for (const DNSRecord& answer : answers) {
        buffer_cur = answer.write(buffer_cur, buffer_end);
        if (buffer_cur == nullptr) return nullptr;
    }
    for (const DNSRecord& authority : authorities) {
        buffer_cur = authority.write(buffer_cur, buffer_end);
        if (buffer_cur == nullptr) return nullptr;
    }
    for (const DNSRecord& additional : additionals) {
        buffer_cur = additional.write(buffer_cur, buffer_end);
        if (buffer_cur == nullptr) return nullptr;
    }
    return buffer_cur;
}

// TODO: convert all callers to this interface, then delete the old one.
bool DNSHeader::write(std::vector<uint8_t>* out) const {
    char buffer[16384];
    char* end = this->write(buffer, buffer + sizeof buffer);
    if (end == nullptr) return false;
    out->insert(out->end(), buffer, end);
    return true;
}

std::string DNSHeader::toString() const {
    // TODO
    return std::string();
}

const char* DNSHeader::readHeader(const char* buffer, const char* buffer_end, unsigned* qdcount,
                                  unsigned* ancount, unsigned* nscount, unsigned* arcount) {
    if (buffer + sizeof(Header) > buffer_end) return nullptr;
    const auto& header = *reinterpret_cast<const Header*>(buffer);
    // bytes 0-1
    id = ntohs(header.id);
    // byte 2: 7:qr, 3-6:opcode, 2:aa, 1:tr, 0:rd
    qr = header.flags0 >> 7;
    opcode = (header.flags0 >> 3) & 0x0F;
    aa = (header.flags0 >> 2) & 1;
    tr = (header.flags0 >> 1) & 1;
    rd = header.flags0 & 1;
    // byte 3: 7:ra, 6:zero, 5:ad, 4:cd, 0-3:rcode
    ra = header.flags1 >> 7;
    ad = (header.flags1 >> 5) & 1;
    rcode = header.flags1 & 0xF;
    // rest of header
    *qdcount = ntohs(header.qdcount);
    *ancount = ntohs(header.ancount);
    *nscount = ntohs(header.nscount);
    *arcount = ntohs(header.arcount);
    return buffer + sizeof(Header);
}

/* DNS responder */

DNSResponder::DNSResponder(std::string listen_address, std::string listen_service,
                           ns_rcode error_rcode, MappingType mapping_type)
    : listen_address_(std::move(listen_address)),
      listen_service_(std::move(listen_service)),
      error_rcode_(error_rcode),
      mapping_type_(mapping_type) {}

DNSResponder::~DNSResponder() {
    stopServer();
}

void DNSResponder::addMapping(const std::string& name, ns_type type, const std::string& addr) {
    std::lock_guard lock(mappings_mutex_);
    mappings_[{name, type}] = addr;
}

void DNSResponder::addMappingDnsHeader(const std::string& name, ns_type type,
                                       const DNSHeader& header) {
    std::lock_guard lock(mappings_mutex_);
    dnsheader_mappings_[{name, type}] = header;
}

void DNSResponder::addMappingBinaryPacket(const std::vector<uint8_t>& query,
                                          const std::vector<uint8_t>& response) {
    std::lock_guard lock(mappings_mutex_);
    packet_mappings_[query] = response;
}

void DNSResponder::removeMapping(const std::string& name, ns_type type) {
    std::lock_guard lock(mappings_mutex_);
    if (!mappings_.erase({name, type})) {
        LOG(ERROR) << "Cannot remove mapping from (" << name << ", " << dnstype2str(type)
                   << "), not present in registered mappings";
    }
}

void DNSResponder::removeMappingDnsHeader(const std::string& name, ns_type type) {
    std::lock_guard lock(mappings_mutex_);
    if (!dnsheader_mappings_.erase({name, type})) {
        LOG(ERROR) << "Cannot remove mapping from (" << name << ", " << dnstype2str(type)
                   << "), not present in registered DnsHeader mappings";
    }
}

void DNSResponder::removeMappingBinaryPacket(const std::vector<uint8_t>& query) {
    std::lock_guard lock(mappings_mutex_);
    if (!packet_mappings_.erase(query)) {
        LOG(ERROR) << "Cannot remove mapping, not present in registered BinaryPacket mappings";
        LOG(INFO) << "Hex dump:";
        LOG(INFO) << android::netdutils::toHex(
                Slice(const_cast<uint8_t*>(query.data()), query.size()), 32);
    }
}

// Set response probability on all supported protocols.
void DNSResponder::setResponseProbability(double response_probability) {
    setResponseProbability(response_probability, IPPROTO_TCP);
    setResponseProbability(response_probability, IPPROTO_UDP);
}

void DNSResponder::setResponseDelayMs(unsigned timeMs) {
    response_delayed_ms_ = timeMs;
}

// Set response probability on specific protocol. It's caller's duty to ensure that the |protocol|
// can be supported by DNSResponder.
void DNSResponder::setResponseProbability(double response_probability, int protocol) {
    switch (protocol) {
        case IPPROTO_TCP:
            response_probability_tcp_ = response_probability;
            break;
        case IPPROTO_UDP:
            response_probability_udp_ = response_probability;
            break;
        default:
            LOG(FATAL) << "Unsupported protocol " << protocol;  // abort() by log level FATAL
    }
}

double DNSResponder::getResponseProbability(int protocol) const {
    switch (protocol) {
        case IPPROTO_TCP:
            return response_probability_tcp_;
        case IPPROTO_UDP:
            return response_probability_udp_;
        default:
            LOG(FATAL) << "Unsupported protocol " << protocol;  // abort() by log level FATAL
            // unreachable
            return -1;
    }
}

void DNSResponder::setEdns(Edns edns) {
    edns_ = edns;
}

void DNSResponder::setTtl(unsigned ttl) {
    answer_record_ttl_sec_ = ttl;
}

bool DNSResponder::running() const {
    return (udp_socket_.ok()) && (tcp_socket_.ok());
}

bool DNSResponder::startServer() {
    if (running()) {
        LOG(ERROR) << "server already running";
        return false;
    }

    // Create UDP, TCP socket
    if (udp_socket_ = createListeningSocket(SOCK_DGRAM); udp_socket_.get() < 0) {
        PLOG(ERROR) << "failed to create UDP socket";
        return false;
    }

    if (tcp_socket_ = createListeningSocket(SOCK_STREAM); tcp_socket_.get() < 0) {
        PLOG(ERROR) << "failed to create TCP socket";
        return false;
    }

    if (listen(tcp_socket_.get(), 1) < 0) {
        PLOG(ERROR) << "failed to listen TCP socket";
        return false;
    }

    // Set up eventfd socket.
    event_fd_.reset(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
    if (event_fd_.get() == -1) {
        PLOG(ERROR) << "failed to create eventfd";
        return false;
    }

    // Set up epoll socket.
    epoll_fd_.reset(epoll_create1(EPOLL_CLOEXEC));
    if (epoll_fd_.get() < 0) {
        PLOG(ERROR) << "epoll_create1() failed on fd";
        return false;
    }

    LOG(INFO) << "adding UDP socket to epoll";
    if (!addFd(udp_socket_.get(), EPOLLIN)) {
        LOG(ERROR) << "failed to add the UDP socket to epoll";
        return false;
    }

    LOG(INFO) << "adding TCP socket to epoll";
    if (!addFd(tcp_socket_.get(), EPOLLIN)) {
        LOG(ERROR) << "failed to add the TCP socket to epoll";
        return false;
    }

    LOG(INFO) << "adding eventfd to epoll";
    if (!addFd(event_fd_.get(), EPOLLIN)) {
        LOG(ERROR) << "failed to add the eventfd to epoll";
        return false;
    }

    {
        std::lock_guard lock(update_mutex_);
        handler_thread_ = std::thread(&DNSResponder::requestHandler, this);
    }
    LOG(INFO) << "server started successfully";
    return true;
}

bool DNSResponder::stopServer() {
    std::lock_guard lock(update_mutex_);
    if (!running()) {
        LOG(ERROR) << "server not running";
        return false;
    }
    LOG(INFO) << "stopping server";
    if (!sendToEventFd()) {
        return false;
    }
    handler_thread_.join();
    epoll_fd_.reset();
    event_fd_.reset();
    udp_socket_.reset();
    tcp_socket_.reset();
    LOG(INFO) << "server stopped successfully";
    return true;
}

std::vector<DNSResponder::QueryInfo> DNSResponder::queries() const {
    std::lock_guard lock(queries_mutex_);
    return queries_;
}

std::string DNSResponder::dumpQueries() const {
    std::lock_guard lock(queries_mutex_);
    std::string out;

    for (const auto& q : queries_) {
        out += "{\"" + q.name + "\", " + std::to_string(q.type) + "\", " +
               dnsproto2str(q.protocol) + "} ";
    }
    return out;
}

void DNSResponder::clearQueries() {
    std::lock_guard lock(queries_mutex_);
    queries_.clear();
}

bool DNSResponder::hasOptPseudoRR(DNSHeader* header) const {
    if (header->additionals.empty()) return false;

    // OPT RR may be placed anywhere within the additional section. See RFC 6891 section 6.1.1.
    auto found = std::find_if(header->additionals.begin(), header->additionals.end(),
                              [](const auto& a) { return a.rtype == ns_type::ns_t_opt; });
    return found != header->additionals.end();
}

void DNSResponder::requestHandler() {
    epoll_event evs[EPOLL_MAX_EVENTS];
    while (true) {
        int n = epoll_wait(epoll_fd_.get(), evs, EPOLL_MAX_EVENTS, -1);
        if (n <= 0) {
            PLOG(ERROR) << "epoll_wait() failed, n=" << n;
            return;
        }

        for (int i = 0; i < n; i++) {
            const int fd = evs[i].data.fd;
            const uint32_t events = evs[i].events;
            if (fd == event_fd_.get() && (events & (EPOLLIN | EPOLLERR))) {
                handleEventFd();
                return;
            } else if (fd == udp_socket_.get() && (events & (EPOLLIN | EPOLLERR))) {
                handleQuery(IPPROTO_UDP);
            } else if (fd == tcp_socket_.get() && (events & (EPOLLIN | EPOLLERR))) {
                handleQuery(IPPROTO_TCP);
            } else {
                LOG(WARNING) << "unexpected epoll events " << events << " on fd " << fd;
            }
        }
    }
}

bool DNSResponder::handleDNSRequest(const char* buffer, ssize_t len, int protocol, char* response,
                                    size_t* response_len) const {
    LOG(DEBUG) << "request: '" << str2hex(buffer, len) << "', on " << dnsproto2str(protocol);
    const char* buffer_end = buffer + len;
    DNSHeader header;
    const char* cur = header.read(buffer, buffer_end);
    // TODO(imaipi): for now, unparsable messages are silently dropped, fix.
    if (cur == nullptr) {
        LOG(ERROR) << "failed to parse query";
        return false;
    }
    if (header.qr) {
        LOG(ERROR) << "response received instead of a query";
        return false;
    }
    if (header.opcode != ns_opcode::ns_o_query) {
        LOG(INFO) << "unsupported request opcode received";
        return makeErrorResponse(&header, ns_rcode::ns_r_notimpl, response, response_len);
    }
    if (header.questions.empty()) {
        LOG(INFO) << "no questions present";
        return makeErrorResponse(&header, ns_rcode::ns_r_formerr, response, response_len);
    }
    if (!header.answers.empty()) {
        LOG(INFO) << "already " << header.answers.size() << " answers present in query";
        return makeErrorResponse(&header, ns_rcode::ns_r_formerr, response, response_len);
    }

    if (edns_ == Edns::FORMERR_UNCOND) {
        LOG(INFO) << "force to return RCODE FORMERR";
        return makeErrorResponse(&header, ns_rcode::ns_r_formerr, response, response_len);
    }

    if (!header.additionals.empty() && edns_ != Edns::ON) {
        LOG(INFO) << "DNS request has an additional section (assumed EDNS). Simulating an ancient "
                     "(pre-EDNS) server, and returning "
                  << (edns_ == Edns::FORMERR_ON_EDNS ? "RCODE FORMERR." : "no response.");
        if (edns_ == Edns::FORMERR_ON_EDNS) {
            return makeErrorResponse(&header, ns_rcode::ns_r_formerr, response, response_len);
        }
        // No response.
        return false;
    }
    {
        std::lock_guard lock(queries_mutex_);
        for (const DNSQuestion& question : header.questions) {
            queries_.push_back({question.qname.name, ns_type(question.qtype), protocol});
        }
    }
    // Ignore requests with the preset probability.
    auto constexpr bound = std::numeric_limits<unsigned>::max();
    if (arc4random_uniform(bound) > bound * getResponseProbability(protocol)) {
        if (error_rcode_ < 0) {
            LOG(ERROR) << "Returning no response";
            return false;
        } else {
            LOG(INFO) << "returning RCODE " << static_cast<int>(error_rcode_)
                      << " in accordance with probability distribution";
            return makeErrorResponse(&header, error_rcode_, response, response_len);
        }
    }

    // Make the response. The query has been read into |header| which is used to build and return
    // the response as well.
    return makeResponse(&header, protocol, response, response_len);
}

bool DNSResponder::addAnswerRecords(const DNSQuestion& question,
                                    std::vector<DNSRecord>* answers) const {
    std::lock_guard guard(mappings_mutex_);
    std::string rname = question.qname.name;
    std::vector<int> rtypes;

    if (question.qtype == ns_type::ns_t_a || question.qtype == ns_type::ns_t_aaaa ||
        question.qtype == ns_type::ns_t_ptr)
        rtypes.push_back(ns_type::ns_t_cname);
    rtypes.push_back(question.qtype);
    for (int rtype : rtypes) {
        std::set<std::string> cnames_Loop;
        std::unordered_map<QueryKey, std::string, QueryKeyHash>::const_iterator it;
        while ((it = mappings_.find(QueryKey(rname, rtype))) != mappings_.end()) {
            if (rtype == ns_type::ns_t_cname) {
                // When detect CNAME infinite loops by cnames_Loop, it won't save the duplicate one.
                // As following, the query will stop on loop3 by detecting the same cname.
                // loop1.{"a.xxx.com", ns_type::ns_t_cname, "b.xxx.com"}(insert in answer record)
                // loop2.{"b.xxx.com", ns_type::ns_t_cname, "a.xxx.com"}(insert in answer record)
                // loop3.{"a.xxx.com", ns_type::ns_t_cname, "b.xxx.com"}(When the same cname record
                //   is found in cnames_Loop already, break the query loop.)
                if (cnames_Loop.find(it->first.name) != cnames_Loop.end()) break;
                cnames_Loop.insert(it->first.name);
            }
            DNSRecord record{
                    .name = {.name = it->first.name},
                    .rtype = it->first.type,
                    .rclass = ns_class::ns_c_in,
                    .ttl = answer_record_ttl_sec_,  // seconds
            };
            if (!fillRdata(it->second, record)) return false;
            answers->push_back(std::move(record));
            if (rtype != ns_type::ns_t_cname) break;
            rname = it->second;
        }
    }

    if (answers->size() == 0) {
        // TODO(imaipi): handle correctly
        LOG(INFO) << "no mapping found for " << question.qname.name << " "
                  << dnstype2str(question.qtype) << ", lazily refusing to add an answer";
    }

    return true;
}

bool DNSResponder::fillRdata(const std::string& rdatastr, DNSRecord& record) {
    if (record.rtype == ns_type::ns_t_a) {
        record.rdata.resize(4);
        if (inet_pton(AF_INET, rdatastr.c_str(), record.rdata.data()) != 1) {
            LOG(ERROR) << "inet_pton(AF_INET, " << rdatastr << ") failed";
            return false;
        }
    } else if (record.rtype == ns_type::ns_t_aaaa) {
        record.rdata.resize(16);
        if (inet_pton(AF_INET6, rdatastr.c_str(), record.rdata.data()) != 1) {
            LOG(ERROR) << "inet_pton(AF_INET6, " << rdatastr << ") failed";
            return false;
        }
    } else if ((record.rtype == ns_type::ns_t_ptr) || (record.rtype == ns_type::ns_t_cname) ||
               (record.rtype == ns_type::ns_t_ns)) {
        constexpr char delimiter = '.';
        std::string name = rdatastr;
        std::vector<char> rdata;

        // Generating PTRDNAME field(section 3.3.12) or CNAME field(section 3.3.1) in rfc1035.
        // The "name" should be an absolute domain name which ends in a dot.
        if (name.back() != delimiter) {
            LOG(ERROR) << "invalid absolute domain name";
            return false;
        }
        name.pop_back();  // remove the dot in tail
        for (const std::string& label : android::base::Split(name, {delimiter})) {
            // The length of label is limited to 63 octets or less. See RFC 1035 section 3.1.
            if (label.length() == 0 || label.length() > 63) {
                LOG(ERROR) << "invalid label length";
                return false;
            }

            rdata.push_back(label.length());
            rdata.insert(rdata.end(), label.begin(), label.end());
        }
        rdata.push_back(0);  // Length byte of zero terminates the label list

        // The length of domain name is limited to 255 octets or less. See RFC 1035 section 3.1.
        if (rdata.size() > 255) {
            LOG(ERROR) << "invalid name length";
            return false;
        }
        record.rdata = move(rdata);
    } else {
        LOG(ERROR) << "unhandled qtype " << dnstype2str(record.rtype);
        return false;
    }
    return true;
}

bool DNSResponder::writePacket(const DNSHeader* header, char* response,
                               size_t* response_len) const {
    char* response_cur = header->write(response, response + *response_len);
    if (response_cur == nullptr) {
        return false;
    }
    *response_len = response_cur - response;
    return true;
}

bool DNSResponder::makeErrorResponse(DNSHeader* header, ns_rcode rcode, char* response,
                                     size_t* response_len) const {
    header->answers.clear();
    header->authorities.clear();
    header->additionals.clear();
    header->rcode = rcode;
    header->qr = true;
    return writePacket(header, response, response_len);
}

bool DNSResponder::makeTruncatedResponse(DNSHeader* header, char* response,
                                         size_t* response_len) const {
    // Build a minimal response for non-EDNS response over UDP. Truncate all stub RRs in answer,
    // authority and additional section. EDNS response truncation has not supported here yet
    // because the EDNS response must have an OPT record. See RFC 6891 section 7.
    header->answers.clear();
    header->authorities.clear();
    header->additionals.clear();
    header->qr = true;
    header->tr = true;
    return writePacket(header, response, response_len);
}

bool DNSResponder::makeResponse(DNSHeader* header, int protocol, char* response,
                                size_t* response_len) const {
    char buffer[16384];
    size_t buffer_len = sizeof(buffer);
    bool ret;

    switch (mapping_type_) {
        case MappingType::DNS_HEADER:
            ret = makeResponseFromDnsHeader(header, buffer, &buffer_len);
            break;
        case MappingType::BINARY_PACKET:
            ret = makeResponseFromBinaryPacket(header, buffer, &buffer_len);
            break;
        case MappingType::ADDRESS_OR_HOSTNAME:
        default:
            ret = makeResponseFromAddressOrHostname(header, buffer, &buffer_len);
    }

    if (!ret) return false;

    // Return truncated response if the built non-EDNS response size which is larger than 512 bytes
    // will be responded over UDP. The truncated response implementation here just simply set up
    // the TC bit and truncate all stub RRs in answer, authority and additional section. It is
    // because the resolver will retry DNS query over TCP and use the full TCP response. See also
    // RFC 1035 section 4.2.1 for UDP response truncation and RFC 6891 section 4.3 for EDNS larger
    // response size capability.
    // TODO: Perhaps keep the stub RRs as possible.
    // TODO: Perhaps truncate the EDNS based response over UDP. See also RFC 6891 section 4.3,
    // section 6.2.5 and section 7.
    if (protocol == IPPROTO_UDP && buffer_len > kMaximumUdpSize &&
        !hasOptPseudoRR(header) /* non-EDNS */) {
        LOG(INFO) << "Return truncated response because original response length " << buffer_len
                  << " is larger than " << kMaximumUdpSize << " bytes.";
        return makeTruncatedResponse(header, response, response_len);
    }

    if (buffer_len > *response_len) {
        LOG(ERROR) << "buffer overflow on line " << __LINE__;
        return false;
    }
    memcpy(response, buffer, buffer_len);
    *response_len = buffer_len;
    return true;
}

bool DNSResponder::makeResponseFromAddressOrHostname(DNSHeader* header, char* response,
                                                     size_t* response_len) const {
    for (const DNSQuestion& question : header->questions) {
        if (question.qclass != ns_class::ns_c_in && question.qclass != ns_class::ns_c_any) {
            LOG(INFO) << "unsupported question class " << question.qclass;
            return makeErrorResponse(header, ns_rcode::ns_r_notimpl, response, response_len);
        }

        if (!addAnswerRecords(question, &header->answers)) {
            return makeErrorResponse(header, ns_rcode::ns_r_servfail, response, response_len);
        }
    }
    header->qr = true;
    return writePacket(header, response, response_len);
}

bool DNSResponder::makeResponseFromDnsHeader(DNSHeader* header, char* response,
                                             size_t* response_len) const {
    std::lock_guard guard(mappings_mutex_);

    // Support single question record only. It should be okay because res_mkquery() sets "qdcount"
    // as one for the operation QUERY and handleDNSRequest() checks ns_opcode::ns_o_query before
    // making a response. In other words, only need to handle the query which has single question
    // section. See also res_mkquery() in system/netd/resolv/res_mkquery.cpp.
    // TODO: Perhaps add support for multi-question records.
    const std::vector<DNSQuestion>& questions = header->questions;
    if (questions.size() != 1) {
        LOG(INFO) << "unsupported question count " << questions.size();
        return makeErrorResponse(header, ns_rcode::ns_r_notimpl, response, response_len);
    }

    if (questions[0].qclass != ns_class::ns_c_in && questions[0].qclass != ns_class::ns_c_any) {
        LOG(INFO) << "unsupported question class " << questions[0].qclass;
        return makeErrorResponse(header, ns_rcode::ns_r_notimpl, response, response_len);
    }

    const std::string name = questions[0].qname.name;
    const int qtype = questions[0].qtype;
    const auto it = dnsheader_mappings_.find(QueryKey(name, qtype));
    if (it != dnsheader_mappings_.end()) {
        // Store both "id" and "rd" which comes from query.
        const unsigned id = header->id;
        const bool rd = header->rd;

        // Build a response from the registered DNSHeader mapping.
        *header = it->second;
        // Assign both "ID" and "RD" fields from query to response. See RFC 1035 section 4.1.1.
        header->id = id;
        header->rd = rd;
    } else {
        // TODO: handle correctly. See also TODO in addAnswerRecords().
        LOG(INFO) << "no mapping found for " << name << " " << dnstype2str(qtype)
                  << ", couldn't build a response from DNSHeader mapping";

        // Note that do nothing as makeResponseFromAddressOrHostname() if no mapping is found. It
        // just changes the QR flag from query (0) to response (1) in the query. Then, send the
        // modified query back as a response.
        header->qr = true;
    }
    return writePacket(header, response, response_len);
}

bool DNSResponder::makeResponseFromBinaryPacket(DNSHeader* header, char* response,
                                                size_t* response_len) const {
    std::lock_guard guard(mappings_mutex_);

    // Build a search key of mapping from the query.
    // TODO: Perhaps pass the query packet buffer directly from the caller.
    std::vector<uint8_t> queryKey;
    if (!header->write(&queryKey)) return false;
    // Clear ID field (byte 0-1) because it is not required by the mapping key.
    queryKey[0] = 0;
    queryKey[1] = 0;

    const auto it = packet_mappings_.find(queryKey);
    if (it != packet_mappings_.end()) {
        if (it->second.size() > *response_len) {
            LOG(ERROR) << "buffer overflow on line " << __LINE__;
            return false;
        } else {
            std::copy(it->second.begin(), it->second.end(), response);
            // Leave the "RD" flag assignment for testing. The "RD" flag of the response keep
            // using the one from the raw packet mapping but the received query.
            // Assign "ID" field from query to response. See RFC 1035 section 4.1.1.
            reinterpret_cast<uint16_t*>(response)[0] = htons(header->id);  // bytes 0-1: id
            *response_len = it->second.size();
            return true;
        }
    } else {
        // TODO: handle correctly. See also TODO in addAnswerRecords().
        // TODO: Perhaps dump packet content to indicate which query failed.
        LOG(INFO) << "no mapping found, couldn't build a response from BinaryPacket mapping";
        // Note that do nothing as makeResponseFromAddressOrHostname() if no mapping is found. It
        // just changes the QR flag from query (0) to response (1) in the query. Then, send the
        // modified query back as a response.
        header->qr = true;
        return writePacket(header, response, response_len);
    }
}

void DNSResponder::setDeferredResp(bool deferred_resp) {
    std::lock_guard<std::mutex> guard(cv_mutex_for_deferred_resp_);
    deferred_resp_ = deferred_resp;
    if (!deferred_resp_) {
        cv_for_deferred_resp_.notify_one();
    }
}

bool DNSResponder::addFd(int fd, uint32_t events) {
    epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, fd, &ev) < 0) {
        PLOG(ERROR) << "epoll_ctl() for socket " << fd << " failed";
        return false;
    }
    return true;
}

void DNSResponder::handleQuery(int protocol) {
    char buffer[16384];
    sockaddr_storage sa;
    socklen_t sa_len = sizeof(sa);
    ssize_t len = 0;
    android::base::unique_fd tcpFd;
    switch (protocol) {
        case IPPROTO_UDP:
            do {
                len = recvfrom(udp_socket_.get(), buffer, sizeof(buffer), 0, (sockaddr*)&sa,
                               &sa_len);
            } while (len < 0 && (errno == EAGAIN || errno == EINTR));
            if (len <= 0) {
                PLOG(ERROR) << "recvfrom() failed, len=" << len;
                return;
            }
            break;
        case IPPROTO_TCP:
            tcpFd.reset(accept4(tcp_socket_.get(), reinterpret_cast<sockaddr*>(&sa), &sa_len,
                                SOCK_CLOEXEC));
            if (tcpFd.get() < 0) {
                PLOG(ERROR) << "failed to accept client socket";
                return;
            }
            // Get the message length from two byte length field.
            // See also RFC 1035, section 4.2.2 and RFC 7766, section 8
            uint8_t queryMessageLengthField[2];
            if (read(tcpFd.get(), &queryMessageLengthField, 2) != 2) {
                PLOG(ERROR) << "Not enough length field bytes";
                return;
            }

            const uint16_t qlen = (queryMessageLengthField[0] << 8) | queryMessageLengthField[1];
            while (len < qlen) {
                ssize_t ret = read(tcpFd.get(), buffer + len, qlen - len);
                if (ret <= 0) {
                    PLOG(ERROR) << "Error while reading query";
                    return;
                }
                len += ret;
            }
            break;
    }
    LOG(DEBUG) << "read " << len << " bytes on " << dnsproto2str(protocol);
    std::lock_guard lock(cv_mutex_);
    char response[16384];
    size_t response_len = sizeof(response);
    // TODO: check whether sending malformed packets to DnsResponder
    if (handleDNSRequest(buffer, len, protocol, response, &response_len) && response_len > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(response_delayed_ms_));
        // place wait_for after handleDNSRequest() so we can check the number of queries in
        // test case before it got responded.
        std::unique_lock guard(cv_mutex_for_deferred_resp_);
        cv_for_deferred_resp_.wait(
                guard, [this]() REQUIRES(cv_mutex_for_deferred_resp_) { return !deferred_resp_; });
        len = 0;

        switch (protocol) {
            case IPPROTO_UDP:
                len = sendto(udp_socket_.get(), response, response_len, 0,
                             reinterpret_cast<const sockaddr*>(&sa), sa_len);
                if (len < 0) {
                    PLOG(ERROR) << "Failed to send response";
                }
                break;
            case IPPROTO_TCP:
                // Get the message length from two byte length field.
                // See also RFC 1035, section 4.2.2 and RFC 7766, section 8
                uint8_t responseMessageLengthField[2];
                responseMessageLengthField[0] = response_len >> 8;
                responseMessageLengthField[1] = response_len;
                if (write(tcpFd.get(), responseMessageLengthField, 2) != 2) {
                    PLOG(ERROR) << "Failed to write response length field";
                    break;
                }
                if (write(tcpFd.get(), response, response_len) !=
                    static_cast<ssize_t>(response_len)) {
                    PLOG(ERROR) << "Failed to write response";
                    break;
                }
                len = response_len;
                break;
        }
        const std::string host_str = addr2str(reinterpret_cast<const sockaddr*>(&sa), sa_len);
        if (len > 0) {
            LOG(DEBUG) << "sent " << len << " bytes to " << host_str;
        } else {
            const char* method_str = (protocol == IPPROTO_TCP) ? "write()" : "sendto()";
            LOG(ERROR) << method_str << " failed for " << host_str;
        }
        // Test that the response is actually a correct DNS message.
        // TODO: Perhaps make DNS message validation to support name compression. Or it throws
        // a warning for a valid DNS message with name compression while the binary packet mapping
        // is used.
        const char* response_end = response + len;
        DNSHeader header;
        const char* cur = header.read(response, response_end);
        if (cur == nullptr) LOG(WARNING) << "response is flawed";
    } else {
        LOG(WARNING) << "not responding";
    }
    cv.notify_one();
    return;
}

bool DNSResponder::sendToEventFd() {
    const uint64_t data = 1;
    if (const ssize_t rt = write(event_fd_.get(), &data, sizeof(data)); rt != sizeof(data)) {
        PLOG(ERROR) << "failed to write eventfd, rt=" << rt;
        return false;
    }
    return true;
}

void DNSResponder::handleEventFd() {
    int64_t data;
    if (const ssize_t rt = read(event_fd_.get(), &data, sizeof(data)); rt != sizeof(data)) {
        PLOG(INFO) << "ignore reading eventfd failed, rt=" << rt;
    }
}

android::base::unique_fd DNSResponder::createListeningSocket(int socket_type) {
    addrinfo ai_hints{
            .ai_flags = AI_PASSIVE,
            .ai_family = AF_UNSPEC,
            .ai_socktype = socket_type,
    };
    addrinfo* ai_res = nullptr;
    const int rv =
            getaddrinfo(listen_address_.c_str(), listen_service_.c_str(), &ai_hints, &ai_res);
    ScopedAddrinfo ai_res_cleanup(ai_res);
    if (rv) {
        LOG(ERROR) << "getaddrinfo(" << listen_address_ << ", " << listen_service_
                   << ") failed: " << gai_strerror(rv);
        return {};
    }
    for (const addrinfo* ai = ai_res; ai; ai = ai->ai_next) {
        android::base::unique_fd fd(
                socket(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK, ai->ai_protocol));
        if (fd.get() < 0) {
            PLOG(ERROR) << "ignore creating socket failed";
            continue;
        }
        enableSockopt(fd.get(), SOL_SOCKET, SO_REUSEPORT).ignoreError();
        enableSockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR).ignoreError();
        const std::string host_str = addr2str(ai->ai_addr, ai->ai_addrlen);
        const char* socket_str = (socket_type == SOCK_STREAM) ? "TCP" : "UDP";

        if (bind(fd.get(), ai->ai_addr, ai->ai_addrlen)) {
            PLOG(ERROR) << "failed to bind " << socket_str << " " << host_str << ":"
                        << listen_service_;
            continue;
        }
        LOG(INFO) << "bound to " << socket_str << " " << host_str << ":" << listen_service_;
        return fd;
    }
    return {};
}

}  // namespace test
