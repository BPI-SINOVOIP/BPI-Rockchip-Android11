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
 *
 */

#ifndef DNS_RESPONDER_H
#define DNS_RESPONDER_H

#include <arpa/nameser.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <android-base/thread_annotations.h>
#include "android-base/unique_fd.h"

// Default TTL of the DNS answer record.
constexpr unsigned kAnswerRecordTtlSec = 5;

// The maximum UDP response size in bytes the DNS responder allows to send. It is used in non-EDNS
// case. See RFC 1035 section 4.2.1.
constexpr unsigned kMaximumUdpSize = 512;

namespace test {

struct DNSName {
    std::string name;
    const char* read(const char* buffer, const char* buffer_end);
    char* write(char* buffer, const char* buffer_end) const;

  private:
    const char* parseField(const char* buffer, const char* buffer_end, bool* last);
};

struct DNSQuestion {
    DNSName qname;
    unsigned qtype;
    unsigned qclass;
    const char* read(const char* buffer, const char* buffer_end);
    char* write(char* buffer, const char* buffer_end) const;
    std::string toString() const;
};

struct DNSRecord {
    DNSName name;
    unsigned rtype;
    unsigned rclass;
    unsigned ttl;
    std::vector<char> rdata;
    const char* read(const char* buffer, const char* buffer_end);
    char* write(char* buffer, const char* buffer_end) const;
    std::string toString() const;

  private:
    struct IntFields {
        uint16_t rtype;
        uint16_t rclass;
        uint32_t ttl;
        uint16_t rdlen;
    } __attribute__((__packed__));

    const char* readIntFields(const char* buffer, const char* buffer_end, unsigned* rdlen);
    char* writeIntFields(unsigned rdlen, char* buffer, const char* buffer_end) const;
};

// TODO: Perhaps rename to DNSMessage. Per RFC 1035 section 4.1, struct DNSHeader more likes a
// message because it has not only header section but also question section and other RRs.
struct DNSHeader {
    unsigned id;
    bool ra;
    uint8_t rcode;
    bool qr;
    uint8_t opcode;
    bool aa;
    bool tr;
    bool rd;
    bool ad;
    std::vector<DNSQuestion> questions;
    std::vector<DNSRecord> answers;
    std::vector<DNSRecord> authorities;
    std::vector<DNSRecord> additionals;
    const char* read(const char* buffer, const char* buffer_end);
    char* write(char* buffer, const char* buffer_end) const;
    bool write(std::vector<uint8_t>* out) const;
    std::string toString() const;

  private:
    struct Header {
        uint16_t id;
        uint8_t flags0;
        uint8_t flags1;
        uint16_t qdcount;
        uint16_t ancount;
        uint16_t nscount;
        uint16_t arcount;
    } __attribute__((__packed__));

    const char* readHeader(const char* buffer, const char* buffer_end, unsigned* qdcount,
                           unsigned* ancount, unsigned* nscount, unsigned* arcount);
};

inline const std::string kDefaultListenAddr = "127.0.0.3";
inline const std::string kDefaultListenService = "53";
inline const ns_rcode kDefaultErrorCode = ns_rcode::ns_r_servfail;

/*
 * Simple DNS responder, which replies to queries with the registered response
 * for that type. Class is assumed to be IN. If no response is registered, the
 * default error response code is returned.
 */
class DNSResponder {
  public:
    enum class Edns {
        ON,
        FORMERR_ON_EDNS,  // DNS server not supporting EDNS will reply FORMERR.
        FORMERR_UNCOND,   // DNS server reply FORMERR unconditionally
        DROP              // DNS server not supporting EDNS will not do any response.
    };
    // Indicate which mapping the DNS server used to build the response.
    // See also addMapping{, DnsHeader, BinaryPacket}, removeMapping{, DnsHeader, BinaryPacket},
    // makeResponse{, FromDnsHeader, FromBinaryPacket}.
    // TODO: Perhaps break class DNSResponder for each mapping.
    enum class MappingType {
        ADDRESS_OR_HOSTNAME,  // Use the mapping from (name, type) to (address or hostname)
        DNS_HEADER,           // Use the mapping from (name, type) to (DNSHeader)
        BINARY_PACKET,        // Use the mapping from (query packet) to (response packet)
    };

    struct QueryInfo {
        std::string name;
        ns_type type;
        int protocol;  // Either IPPROTO_TCP or IPPROTO_UDP
    };

    DNSResponder(std::string listen_address = kDefaultListenAddr,
                 std::string listen_service = kDefaultListenService,
                 ns_rcode error_rcode = kDefaultErrorCode,
                 DNSResponder::MappingType mapping_type = MappingType::ADDRESS_OR_HOSTNAME);

    DNSResponder(ns_rcode error_rcode)
        : DNSResponder(kDefaultListenAddr, kDefaultListenService, error_rcode){};

    DNSResponder(MappingType mapping_type)
        : DNSResponder(kDefaultListenAddr, kDefaultListenService, kDefaultErrorCode,
                       mapping_type){};

    ~DNSResponder();

    // Functions used for accessing mapping {ADDRESS_OR_HOSTNAME, DNS_HEADER, BINARY_PACKET}.
    void addMapping(const std::string& name, ns_type type, const std::string& addr);
    void addMappingDnsHeader(const std::string& name, ns_type type, const DNSHeader& header);
    void addMappingBinaryPacket(const std::vector<uint8_t>& query,
                                const std::vector<uint8_t>& response);
    void removeMapping(const std::string& name, ns_type type);
    void removeMappingDnsHeader(const std::string& name, ns_type type);
    void removeMappingBinaryPacket(const std::vector<uint8_t>& query);

    void setResponseProbability(double response_probability);
    void setResponseProbability(double response_probability, int protocol);
    void setResponseDelayMs(unsigned);
    void setEdns(Edns edns);
    void setTtl(unsigned ttl);
    bool running() const;
    bool startServer();
    bool stopServer();
    const std::string& listen_address() const { return listen_address_; }
    const std::string& listen_service() const { return listen_service_; }
    std::vector<QueryInfo> queries() const;
    std::string dumpQueries() const;
    void clearQueries();
    std::condition_variable& getCv() { return cv; }
    std::mutex& getCvMutex() { return cv_mutex_; }
    void setDeferredResp(bool deferred_resp);
    static bool fillRdata(const std::string& rdatastr, DNSRecord& record);

    // TODO: Make DNSResponder record unknown queries in a vector for improving the debugging.
    // Unit test could dump the unexpected query for further debug if any unexpected failure.

  private:
    // Key used for accessing mappings.
    struct QueryKey {
        std::string name;
        unsigned type;

        QueryKey(std::string n, unsigned t) : name(move(n)), type(t) {}
        bool operator==(const QueryKey& o) const { return name == o.name && type == o.type; }
        bool operator<(const QueryKey& o) const {
            if (name < o.name) return true;
            if (name > o.name) return false;
            return type < o.type;
        }
    };

    struct QueryKeyHash {
        size_t operator()(const QueryKey& key) const {
            return std::hash<std::string>()(key.name) + static_cast<size_t>(key.type);
        }
    };

    // Used for generating combined hash value of a vector.
    // std::hash<T> doesn't provide a specialization for std::vector<T>.
    struct QueryKeyVectorHash {
        std::size_t operator()(const std::vector<uint8_t>& v) const {
            std::size_t combined = 0;
            for (const uint8_t i : v) {
                // Hash combination comes from boost::hash_combine
                // See also system/extras/simpleperf/utils.h
                combined ^=
                        std::hash<uint8_t>{}(i) + 0x9e3779b9 + (combined << 6) + (combined >> 2);
            }
            return combined;
        }
    };

    void requestHandler();

    // Check if any OPT Pseudo RR in the additional section.
    bool hasOptPseudoRR(DNSHeader* header) const;

    // Parses and generates a response message for incoming DNS requests.
    // Returns false to ignore the request, which might be due to either parsing error
    // or unresponsiveness.
    bool handleDNSRequest(const char* buffer, ssize_t buffer_len, int protocol, char* response,
                          size_t* response_len) const;

    bool addAnswerRecords(const DNSQuestion& question, std::vector<DNSRecord>* answers) const;

    bool generateErrorResponse(DNSHeader* header, ns_rcode rcode, char* response,
                               size_t* response_len) const;

    // TODO: Change writePacket, makeErrorResponse, makeTruncatedResponse and
    // makeResponse{, FromAddressOrHostname, FromDnsHeader, FromBinaryPacket} to use C++ containers
    // instead of the unsafe pointer + length buffer.
    bool writePacket(const DNSHeader* header, char* response, size_t* response_len) const;
    // Build an error response with a given rcode.
    bool makeErrorResponse(DNSHeader* header, ns_rcode rcode, char* response,
                           size_t* response_len) const;
    // Build a truncated response.
    bool makeTruncatedResponse(DNSHeader* header, char* response, size_t* response_len) const;
    // Build a response.
    bool makeResponse(DNSHeader* header, int protocol, char* response, size_t* response_len) const;
    // Helper for building a response from mapping {ADDRESS_OR_HOSTNAME, DNS_HEADER, BINARY_PACKET}.
    bool makeResponseFromAddressOrHostname(DNSHeader* header, char* response,
                                           size_t* response_len) const;
    bool makeResponseFromDnsHeader(DNSHeader* header, char* response, size_t* response_len) const;
    bool makeResponseFromBinaryPacket(DNSHeader* header, char* response,
                                      size_t* response_len) const;

    // Add a new file descriptor to be polled by the handler thread.
    bool addFd(int fd, uint32_t events);

    // Read the query sent from the client and send the answer back to the client. It
    // makes sure the I/O communicated with the client is correct.
    void handleQuery(int protocol);

    // Trigger the handler thread to terminate.
    bool sendToEventFd();

    // Used in the handler thread for the termination signal.
    void handleEventFd();

    // TODO: Move createListeningSocket to resolv_test_utils.h
    android::base::unique_fd createListeningSocket(int socket_type);

    double getResponseProbability(int protocol) const;

    // Address and service to listen on TCP and UDP.
    const std::string listen_address_;
    const std::string listen_service_;
    // Error code to return for requests for an unknown name.
    const ns_rcode error_rcode_;
    // Mapping type the DNS server used to build the response.
    const MappingType mapping_type_;
    // Probability that a valid response on TCP is being sent instead of
    // returning error_rcode_ or no response.
    std::atomic<double> response_probability_tcp_ = 1.0;
    // Probability that a valid response on UDP is being sent instead of
    // returning error_rcode_ or no response.
    std::atomic<double> response_probability_udp_ = 1.0;

    std::atomic<unsigned> answer_record_ttl_sec_ = kAnswerRecordTtlSec;

    std::atomic<unsigned> response_delayed_ms_ = 0;

    // Maximum number of fds for epoll.
    const int EPOLL_MAX_EVENTS = 2;

    // Control how the DNS server behaves when it receives the requests containing OPT RR.
    // If it's set Edns::ON, the server can recognize and reply the response; if it's set
    // Edns::FORMERR_ON_EDNS, the server behaves like an old DNS server that doesn't support EDNS0,
    // and replying FORMERR; if it's Edns::DROP, the server doesn't support EDNS0 either, and
    // ignoring the requests.
    std::atomic<Edns> edns_ = Edns::ON;

    // Mappings used for building the DNS response by registered mapping items. |mapping_type_|
    // decides which mapping is used. See also makeResponse{, FromDnsHeader}.
    // - mappings_: Mapping from (name, type) to (address or hostname).
    // - dnsheader_mappings_: Mapping from (name, type) to (DNSHeader).
    // - packet_mappings_: Mapping from (query packet) to (response packet).
    std::unordered_map<QueryKey, std::string, QueryKeyHash> mappings_ GUARDED_BY(mappings_mutex_);
    std::unordered_map<QueryKey, DNSHeader, QueryKeyHash> dnsheader_mappings_
            GUARDED_BY(mappings_mutex_);
    std::unordered_map<std::vector<uint8_t>, std::vector<uint8_t>, QueryKeyVectorHash>
            packet_mappings_ GUARDED_BY(mappings_mutex_);

    mutable std::mutex mappings_mutex_;
    // Query names received so far and the corresponding mutex.
    mutable std::vector<QueryInfo> queries_ GUARDED_BY(queries_mutex_);
    mutable std::mutex queries_mutex_;
    // Socket on which the server is listening.
    android::base::unique_fd udp_socket_;
    android::base::unique_fd tcp_socket_;
    // File descriptor for epoll.
    android::base::unique_fd epoll_fd_;
    // Eventfd used to signal for the handler thread termination.
    android::base::unique_fd event_fd_;
    // Thread for handling incoming threads.
    std::thread handler_thread_ GUARDED_BY(update_mutex_);
    std::mutex update_mutex_;
    std::condition_variable cv;
    std::mutex cv_mutex_;

    std::condition_variable cv_for_deferred_resp_;
    std::mutex cv_mutex_for_deferred_resp_;
    bool deferred_resp_ GUARDED_BY(cv_mutex_for_deferred_resp_) = false;
};

}  // namespace test

#endif  // DNS_RESPONDER_H
