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
 *
 */

#pragma once

#include <arpa/nameser.h>
#include <netdb.h>

#include <string>
#include <vector>

#include <aidl/android/net/INetd.h>
#include <gtest/gtest.h>
#include <netdutils/InternetAddresses.h>

#include "dns_responder/dns_responder.h"

class ScopeBlockedUIDRule {
    using INetd = aidl::android::net::INetd;

  public:
    ScopeBlockedUIDRule(INetd* netSrv, uid_t testUid)
        : mNetSrv(netSrv), mTestUid(testUid), mSavedUid(getuid()) {
        // Add drop rule for testUid. Also enable the standby chain because it might not be
        // enabled. Unfortunately we cannot use FIREWALL_CHAIN_NONE, or custom iptables rules, for
        // this purpose because netd calls fchown() on the DNS query sockets, and "iptables -m
        // owner" matches the UID of the socket creator, not the UID set by fchown().
        // TODO: migrate FIREWALL_CHAIN_NONE to eBPF as well.
        EXPECT_TRUE(mNetSrv->firewallEnableChildChain(INetd::FIREWALL_CHAIN_STANDBY, true).isOk());
        EXPECT_TRUE(mNetSrv->firewallSetUidRule(INetd::FIREWALL_CHAIN_STANDBY, mTestUid,
                                                INetd::FIREWALL_RULE_DENY)
                            .isOk());
        EXPECT_TRUE(seteuid(mTestUid) == 0);
    };
    ~ScopeBlockedUIDRule() {
        // Restore uid
        EXPECT_TRUE(seteuid(mSavedUid) == 0);
        // Remove drop rule for testUid, and disable the standby chain.
        EXPECT_TRUE(mNetSrv->firewallSetUidRule(INetd::FIREWALL_CHAIN_STANDBY, mTestUid,
                                                INetd::FIREWALL_RULE_ALLOW)
                            .isOk());
        EXPECT_TRUE(mNetSrv->firewallEnableChildChain(INetd::FIREWALL_CHAIN_STANDBY, false).isOk());
    }

  private:
    INetd* mNetSrv;
    const uid_t mTestUid;
    const uid_t mSavedUid;
};

class ScopedChangeUID {
  public:
    ScopedChangeUID(uid_t testUid) : mTestUid(testUid), mSavedUid(getuid()) {
        EXPECT_TRUE(seteuid(mTestUid) == 0);
    };
    ~ScopedChangeUID() { EXPECT_TRUE(seteuid(mSavedUid) == 0); }

  private:
    const uid_t mTestUid;
    const uid_t mSavedUid;
};

struct DnsRecord {
    std::string host_name;  // host name
    ns_type type;           // record type
    std::string addr;       // ipv4/v6 address
};

// TODO: make this dynamic and stop depending on implementation details.
constexpr int TEST_NETID = 30;
// Use maximum reserved appId for applications to avoid conflict with existing uids.
constexpr int TEST_UID = 99999;

static constexpr char kLocalHost[] = "localhost";
static constexpr char kLocalHostAddr[] = "127.0.0.1";
static constexpr char kIp6LocalHost[] = "ip6-localhost";
static constexpr char kIp6LocalHostAddr[] = "::1";
static constexpr char kHelloExampleCom[] = "hello.example.com.";
static constexpr char kHelloExampleComAddrV4[] = "1.2.3.4";
static constexpr char kHelloExampleComAddrV6[] = "::1.2.3.4";
static constexpr char kExampleComDomain[] = ".example.com";

constexpr size_t kMaxmiumLabelSize = 63;  // see RFC 1035 section 2.3.4.

static const std::vector<uint8_t> kHelloExampleComQueryV4 = {
        /* Header */
        0x00, 0x00, /* Transaction ID: 0x0000 */
        0x01, 0x00, /* Flags: rd */
        0x00, 0x01, /* Questions: 1 */
        0x00, 0x00, /* Answer RRs: 0 */
        0x00, 0x00, /* Authority RRs: 0 */
        0x00, 0x00, /* Additional RRs: 0 */
        /* Queries */
        0x05, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x07, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x03,
        0x63, 0x6f, 0x6d, 0x00, /* Name: hello.example.com */
        0x00, 0x01,             /* Type: A */
        0x00, 0x01              /* Class: IN */
};

static const std::vector<uint8_t> kHelloExampleComResponseV4 = {
        /* Header */
        0x00, 0x00, /* Transaction ID: 0x0000 */
        0x81, 0x80, /* Flags: qr rd ra */
        0x00, 0x01, /* Questions: 1 */
        0x00, 0x01, /* Answer RRs: 1 */
        0x00, 0x00, /* Authority RRs: 0 */
        0x00, 0x00, /* Additional RRs: 0 */
        /* Queries */
        0x05, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x07, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x03,
        0x63, 0x6f, 0x6d, 0x00, /* Name: hello.example.com */
        0x00, 0x01,             /* Type: A */
        0x00, 0x01,             /* Class: IN */
        /* Answers */
        0x05, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x07, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x03,
        0x63, 0x6f, 0x6d, 0x00, /* Name: hello.example.com */
        0x00, 0x01,             /* Type: A */
        0x00, 0x01,             /* Class: IN */
        0x00, 0x00, 0x00, 0x00, /* Time to live: 0 */
        0x00, 0x04,             /* Data length: 4 */
        0x01, 0x02, 0x03, 0x04  /* Address: 1.2.3.4 */
};

// Illegal hostnames
static constexpr char kBadCharAfterPeriodHost[] = "hello.example.^com.";
static constexpr char kBadCharBeforePeriodHost[] = "hello.example^.com.";
static constexpr char kBadCharAtTheEndHost[] = "hello.example.com^.";
static constexpr char kBadCharInTheMiddleOfLabelHost[] = "hello.ex^ample.com.";

static const test::DNSHeader kDefaultDnsHeader = {
        // Don't need to initialize the flag "id" and "rd" because DNS responder assigns them from
        // query to response. See RFC 1035 section 4.1.1.
        .id = 0,                // unused. should be assigned from query to response
        .ra = false,            // recursive query support is not available
        .rcode = ns_r_noerror,  // no error
        .qr = true,             // message is a response
        .opcode = QUERY,        // a standard query
        .aa = false,            // answer/authority portion was not authenticated by the server
        .tr = false,            // message is not truncated
        .rd = false,            // unused. should be assigned from query to response
        .ad = false,            // non-authenticated data is unacceptable
};

// The CNAME chain records for building a response message which exceeds 512 bytes.
//
// Ignoring the other fields of the message, the response message has 8 CNAMEs in 5 answer RRs
// and each CNAME has 77 bytes as the follows. The response message at least has 616 bytes in
// answer section and has already exceeded 512 bytes totally.
//
// The CNAME is presented as:
//   0   1            64  65                          72  73          76  77
//   +---+--........--+---+---+---+---+---+---+---+---+---+---+---+---+---+
//   | 63| {x, .., x} | 7 | e | x | a | m | p | l | e | 3 | c | o | m | 0 |
//   +---+--........--+---+---+---+---+---+---+---+---+---+---+---+---+---+
//          ^-- x = {a, b, c, d}
//
const std::string kCnameA = std::string(kMaxmiumLabelSize, 'a') + kExampleComDomain + ".";
const std::string kCnameB = std::string(kMaxmiumLabelSize, 'b') + kExampleComDomain + ".";
const std::string kCnameC = std::string(kMaxmiumLabelSize, 'c') + kExampleComDomain + ".";
const std::string kCnameD = std::string(kMaxmiumLabelSize, 'd') + kExampleComDomain + ".";
const std::vector<DnsRecord> kLargeCnameChainRecords = {
        {kHelloExampleCom, ns_type::ns_t_cname, kCnameA},
        {kCnameA, ns_type::ns_t_cname, kCnameB},
        {kCnameB, ns_type::ns_t_cname, kCnameC},
        {kCnameC, ns_type::ns_t_cname, kCnameD},
        {kCnameD, ns_type::ns_t_a, kHelloExampleComAddrV4},
};

// TODO: Integrate GetNumQueries relevent functions
size_t GetNumQueries(const test::DNSResponder& dns, const char* name);
size_t GetNumQueriesForProtocol(const test::DNSResponder& dns, const int protocol,
                                const char* name);
size_t GetNumQueriesForType(const test::DNSResponder& dns, ns_type type, const char* name);
std::string ToString(const hostent* he);
std::string ToString(const addrinfo* ai);
std::string ToString(const android::netdutils::ScopedAddrinfo& ai);
std::string ToString(const sockaddr_storage* addr);
std::vector<std::string> ToStrings(const hostent* he);
std::vector<std::string> ToStrings(const addrinfo* ai);
std::vector<std::string> ToStrings(const android::netdutils::ScopedAddrinfo& ai);

// Wait for |condition| to be met until |timeout|.
bool PollForCondition(const std::function<bool()>& condition,
                      std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));
