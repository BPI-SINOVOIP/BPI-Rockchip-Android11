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

#define LOG_TAG "resolv"

#include <aidl/android/net/IDnsResolver.h>
#include <android-base/stringprintf.h>
#include <arpa/inet.h>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <netdb.h>
#include <netdutils/InternetAddresses.h>
#include <resolv_stats_test_utils.h>

#include "dns_responder.h"
#include "getaddrinfo.h"
#include "gethnamaddr.h"
#include "resolv_cache.h"
#include "stats.pb.h"
#include "tests/resolv_test_utils.h"

#define NAME(variable) #variable

namespace android {
namespace net {

using aidl::android::net::IDnsResolver;
using android::base::StringPrintf;
using android::net::NetworkDnsEventReported;
using android::netdutils::ScopedAddrinfo;

// The buffer size of resolv_gethostbyname().
constexpr unsigned int MAXPACKET = 8 * 1024;

// Specifying 0 in ai_socktype or ai_protocol of struct addrinfo indicates
// that any type or protocol can be returned by getaddrinfo().
constexpr unsigned int ANY = 0;

class TestBase : public ::testing::Test {
  protected:
    struct DnsMessage {
        std::string host_name;   // host name
        ns_type type;            // record type
        test::DNSHeader header;  // dns header
    };

    void SetUp() override {
        // Create cache for test
        resolv_create_cache_for_net(TEST_NETID);
    }
    void TearDown() override {
        // Delete cache for test
        resolv_delete_cache_for_net(TEST_NETID);
    }

    test::DNSRecord MakeAnswerRecord(const std::string& name, unsigned rclass, unsigned rtype,
                                     const std::string& rdata, unsigned ttl = kAnswerRecordTtlSec) {
        test::DNSRecord record{
                .name = {.name = name},
                .rtype = rtype,
                .rclass = rclass,
                .ttl = ttl,
        };
        EXPECT_TRUE(test::DNSResponder::fillRdata(rdata, record));
        return record;
    }

    DnsMessage MakeDnsMessage(const std::string& qname, ns_type qtype,
                              const std::vector<std::string>& rdata) {
        const unsigned qclass = ns_c_in;
        // Build a DNSHeader in the following format.
        // Question
        //   <qname>                IN      <qtype>
        // Answer
        //   <qname>                IN      <qtype>     <rdata[0]>
        //   ..
        //   <qname>                IN      <qtype>     <rdata[n]>
        //
        // Example:
        // Question
        //   hello.example.com.     IN      A
        // Answer
        //   hello.example.com.     IN      A           1.2.3.1
        //   ..
        //   hello.example.com.     IN      A           1.2.3.9
        test::DNSHeader header(kDefaultDnsHeader);

        // Question section
        test::DNSQuestion question{
                .qname = {.name = qname},
                .qtype = qtype,
                .qclass = qclass,
        };
        header.questions.push_back(std::move(question));

        // Answer section
        for (const auto& r : rdata) {
            test::DNSRecord record = MakeAnswerRecord(qname, qclass, qtype, r);
            header.answers.push_back(std::move(record));
        }
        // TODO: Perhaps add support for authority RRs and additional RRs.
        return {qname, qtype, header};
    }

    void StartDns(test::DNSResponder& dns, const std::vector<DnsMessage>& messages) {
        for (const auto& m : messages) {
            dns.addMappingDnsHeader(m.host_name, m.type, m.header);
        }
        ASSERT_TRUE(dns.startServer());
        dns.clearQueries();
    }

    int SetResolvers() { return resolv_set_nameservers(TEST_NETID, servers, domains, params); }

    const android_net_context mNetcontext = {
            .app_netid = TEST_NETID,
            .app_mark = MARK_UNSET,
            .dns_netid = TEST_NETID,
            .dns_mark = MARK_UNSET,
            .uid = NET_CONTEXT_INVALID_UID,
    };
    const std::vector<std::string> servers = {test::kDefaultListenAddr};
    const std::vector<std::string> domains = {"example.com"};
    const res_params params = {
            .sample_validity = 300,
            .success_threshold = 25,
            .min_samples = 8,
            .max_samples = 8,
            .base_timeout_msec = 1000,
            .retry_count = 2,
    };
};

class ResolvGetAddrInfoTest : public TestBase {};
class GetHostByNameForNetContextTest : public TestBase {};
class ResolvCommonFunctionTest : public TestBase {};

TEST_F(ResolvGetAddrInfoTest, InvalidParameters) {
    // Both null "netcontext" and null "res" of resolv_getaddrinfo() are not tested
    // here because they are checked by assert() without returning any error number.

    // Invalid hostname and servname.
    // Both hostname and servname are null pointers. Expect error number EAI_NONAME.
    {
        addrinfo* result = nullptr;
        NetworkDnsEventReported event;
        int rv = resolv_getaddrinfo(nullptr /*hostname*/, nullptr /*servname*/, nullptr /*hints*/,
                                    &mNetcontext, &result, &event);
        ScopedAddrinfo result_cleanup(result);
        EXPECT_EQ(EAI_NONAME, rv);
    }

    // Invalid hints.
    // These place holders are used to test function call with unrequired parameters.
    // The content is not important because function call returns error directly if
    // there have any unrequired parameter.
    char placeholder_cname[] = "invalid_cname";
    sockaddr placeholder_addr = {};
    addrinfo placeholder_next = {};
    static const struct TestConfig {
        int ai_flags;
        socklen_t ai_addrlen;
        char* ai_canonname;
        sockaddr* ai_addr;
        addrinfo* ai_next;

        int expected_eai_error;

        std::string asParameters() const {
            return StringPrintf("0x%x/%u/%s/%p/%p", ai_flags, ai_addrlen,
                                ai_canonname ? ai_canonname : "(null)", (void*)ai_addr,
                                (void*)ai_next);
        }
    } testConfigs[]{
            {0, sizeof(in_addr) /*bad*/, nullptr, nullptr, nullptr, EAI_BADHINTS},
            {0, 0, placeholder_cname /*bad*/, nullptr, nullptr, EAI_BADHINTS},
            {0, 0, nullptr, &placeholder_addr /*bad*/, nullptr, EAI_BADHINTS},
            {0, 0, nullptr, nullptr, &placeholder_next /*bad*/, EAI_BADHINTS},
            {AI_ALL /*bad*/, 0, nullptr, nullptr, nullptr, EAI_BADFLAGS},
            {AI_V4MAPPED_CFG /*bad*/, 0, nullptr, nullptr, nullptr, EAI_BADFLAGS},
            {AI_V4MAPPED /*bad*/, 0, nullptr, nullptr, nullptr, EAI_BADFLAGS},
            {AI_DEFAULT /*bad*/, 0, nullptr, nullptr, nullptr, EAI_BADFLAGS},
    };

    for (const auto& config : testConfigs) {
        SCOPED_TRACE(config.asParameters());

        addrinfo* result = nullptr;
        // In current test configuration set, ai_family, ai_protocol and ai_socktype are not
        // checked because other fields cause hints error check failed first.
        const addrinfo hints = {
                .ai_flags = config.ai_flags,
                .ai_family = AF_UNSPEC,
                .ai_socktype = ANY,
                .ai_protocol = ANY,
                .ai_addrlen = config.ai_addrlen,
                .ai_canonname = config.ai_canonname,
                .ai_addr = config.ai_addr,
                .ai_next = config.ai_next,
        };
        NetworkDnsEventReported event;
        int rv = resolv_getaddrinfo("localhost", nullptr /*servname*/, &hints, &mNetcontext,
                                    &result, &event);
        ScopedAddrinfo result_cleanup(result);
        EXPECT_EQ(config.expected_eai_error, rv);
    }
}

TEST_F(ResolvGetAddrInfoTest, InvalidParameters_Family) {
    for (int family = 0; family < AF_MAX; ++family) {
        if (family == AF_UNSPEC || family == AF_INET || family == AF_INET6) {
            continue;  // skip supported family
        }
        SCOPED_TRACE(StringPrintf("family: %d", family));

        addrinfo* result = nullptr;
        const addrinfo hints = {
                .ai_family = family,  // unsupported family
        };
        NetworkDnsEventReported event;
        int rv = resolv_getaddrinfo("localhost", nullptr /*servname*/, &hints, &mNetcontext,
                                    &result, &event);
        ScopedAddrinfo result_cleanup(result);
        EXPECT_EQ(EAI_FAMILY, rv);
    }
}

TEST_F(ResolvGetAddrInfoTest, InvalidParameters_SocketType) {
    for (const auto& family : {AF_INET, AF_INET6, AF_UNSPEC}) {
        for (int protocol = 0; protocol < IPPROTO_MAX; ++protocol) {
            // Socket types which are not in explore_options.
            for (const auto& socktype : {SOCK_RDM, SOCK_SEQPACKET, SOCK_DCCP, SOCK_PACKET}) {
                const addrinfo hints = {
                        .ai_family = family,
                        .ai_socktype = socktype,
                        .ai_protocol = protocol,
                };
                for (const char* service : {static_cast<const char*>(nullptr),  // service is null
                                            "80",
                                            "",  // empty service name
                                            "ftp",
                                            "65536",  // out of valid port range from 0 to 65535
                                            "invalid"}) {
                    SCOPED_TRACE(StringPrintf("family: %d, socktype: %d, protocol: %d, service: %s",
                                              family, socktype, protocol,
                                              service ? service : "service is nullptr"));
                    addrinfo* result = nullptr;
                    NetworkDnsEventReported event;
                    int rv = resolv_getaddrinfo("localhost", service, &hints, &mNetcontext, &result,
                                                &event);
                    ScopedAddrinfo result_cleanup(result);
                    EXPECT_EQ(EAI_SOCKTYPE, rv);
                }
            }
        }
    }
}

TEST_F(ResolvGetAddrInfoTest, InvalidParameters_MeaningfulSocktypeAndProtocolCombination) {
    static const int families[] = {PF_INET, PF_INET6, PF_UNSPEC};
    // Skip to test socket type SOCK_RAW in meaningful combination (explore_options[]) of
    // system\netd\resolv\getaddrinfo.cpp. In explore_options[], the socket type SOCK_RAW always
    // comes with protocol ANY which causes skipping meaningful socktype/protocol combination
    // check. So it nerver returns error number EAI_BADHINTS which we want to test in this test
    // case.
    static const int socktypes[] = {SOCK_STREAM, SOCK_DGRAM};

    // If both socktype/protocol are specified, check non-meaningful combination returns
    // expected error number EAI_BADHINTS. See meaningful combination in explore_options[] of
    // system\netd\resolv\getaddrinfo.cpp.
    for (const auto& family : families) {
        for (const auto& socktype : socktypes) {
            for (int protocol = 0; protocol < IPPROTO_MAX; ++protocol) {
                SCOPED_TRACE(StringPrintf("family: %d, socktype: %d, protocol: %d", family,
                                          socktype, protocol));

                // Both socktype/protocol need to be specified.
                if (!socktype || !protocol) continue;

                // Skip meaningful combination in explore_options[] of
                // system\netd\resolv\getaddrinfo.cpp.
                if ((family == AF_INET6 && socktype == SOCK_DGRAM && protocol == IPPROTO_UDP) ||
                    (family == AF_INET6 && socktype == SOCK_STREAM && protocol == IPPROTO_TCP) ||
                    (family == AF_INET && socktype == SOCK_DGRAM && protocol == IPPROTO_UDP) ||
                    (family == AF_INET && socktype == SOCK_STREAM && protocol == IPPROTO_TCP) ||
                    (family == AF_UNSPEC && socktype == SOCK_DGRAM && protocol == IPPROTO_UDP) ||
                    (family == AF_UNSPEC && socktype == SOCK_STREAM && protocol == IPPROTO_TCP)) {
                    continue;
                }

                addrinfo* result = nullptr;
                const addrinfo hints = {
                        .ai_family = family,
                        .ai_socktype = socktype,
                        .ai_protocol = protocol,
                };
                NetworkDnsEventReported event;
                int rv = resolv_getaddrinfo("localhost", nullptr /*servname*/, &hints, &mNetcontext,
                                            &result, &event);
                ScopedAddrinfo result_cleanup(result);
                EXPECT_EQ(EAI_BADHINTS, rv);
            }
        }
    }
}

// The test configs are used for verifying the error path of get_port().
// Note that the EAI_SOCKTYPE verification are moved to an independent
// test case because validateHints() verify invalid socket type early now.
// See also InvalidParameters_SocketType.
TEST_F(ResolvGetAddrInfoTest, InvalidParameters_PortNameAndNumber) {
    constexpr char http_portno[] = "80";
    constexpr char invalid_portno[] = "65536";  // out of valid port range from 0 to 65535
    constexpr char http_portname[] = "http";
    constexpr char invalid_portname[] = "invalid_portname";

    static const struct TestConfig {
        int ai_flags;
        int ai_family;
        int ai_socktype;
        const char* servname;

        int expected_eai_error;

        std::string asParameters() const {
            return StringPrintf("0x%x/%d/%d/%s", ai_flags, ai_family, ai_socktype,
                                servname ? servname : "(null)");
        }
    } testConfigs[]{
            {0, AF_INET, SOCK_RAW /*bad*/, http_portno, EAI_SERVICE},
            {0, AF_INET6, SOCK_RAW /*bad*/, http_portno, EAI_SERVICE},
            {0, AF_UNSPEC, SOCK_RAW /*bad*/, http_portno, EAI_SERVICE},
            {0, AF_INET, ANY, invalid_portno /*bad*/, EAI_SERVICE},
            {0, AF_INET, SOCK_STREAM, invalid_portno /*bad*/, EAI_SERVICE},
            {0, AF_INET, SOCK_DGRAM, invalid_portno /*bad*/, EAI_SERVICE},
            {0, AF_INET6, ANY, invalid_portno /*bad*/, EAI_SERVICE},
            {0, AF_INET6, SOCK_STREAM, invalid_portno /*bad*/, EAI_SERVICE},
            {0, AF_INET6, SOCK_DGRAM, invalid_portno /*bad*/, EAI_SERVICE},
            {0, AF_UNSPEC, ANY, invalid_portno /*bad*/, EAI_SERVICE},
            {0, AF_UNSPEC, SOCK_STREAM, invalid_portno /*bad*/, EAI_SERVICE},
            {0, AF_UNSPEC, SOCK_DGRAM, invalid_portno /*bad*/, EAI_SERVICE},
            {AI_NUMERICSERV, AF_INET, ANY, http_portname /*bad*/, EAI_NONAME},
            {AI_NUMERICSERV, AF_INET, SOCK_STREAM, http_portname /*bad*/, EAI_NONAME},
            {AI_NUMERICSERV, AF_INET, SOCK_DGRAM, http_portname /*bad*/, EAI_NONAME},
            {AI_NUMERICSERV, AF_INET6, ANY, http_portname /*bad*/, EAI_NONAME},
            {AI_NUMERICSERV, AF_INET6, SOCK_STREAM, http_portname /*bad*/, EAI_NONAME},
            {AI_NUMERICSERV, AF_INET6, SOCK_DGRAM, http_portname /*bad*/, EAI_NONAME},
            {AI_NUMERICSERV, AF_UNSPEC, ANY, http_portname /*bad*/, EAI_NONAME},
            {AI_NUMERICSERV, AF_UNSPEC, SOCK_STREAM, http_portname /*bad*/, EAI_NONAME},
            {AI_NUMERICSERV, AF_UNSPEC, SOCK_DGRAM, http_portname /*bad*/, EAI_NONAME},
            {0, AF_INET, ANY, invalid_portname /*bad*/, EAI_SERVICE},
            {0, AF_INET, SOCK_STREAM, invalid_portname /*bad*/, EAI_SERVICE},
            {0, AF_INET, SOCK_DGRAM, invalid_portname /*bad*/, EAI_SERVICE},
            {0, AF_INET6, ANY, invalid_portname /*bad*/, EAI_SERVICE},
            {0, AF_INET6, SOCK_STREAM, invalid_portname /*bad*/, EAI_SERVICE},
            {0, AF_INET6, SOCK_DGRAM, invalid_portname /*bad*/, EAI_SERVICE},
            {0, AF_UNSPEC, ANY, invalid_portname /*bad*/, EAI_SERVICE},
            {0, AF_UNSPEC, SOCK_STREAM, invalid_portname /*bad*/, EAI_SERVICE},
            {0, AF_UNSPEC, SOCK_DGRAM, invalid_portname /*bad*/, EAI_SERVICE},
    };

    for (const auto& config : testConfigs) {
        const std::string testParameters = config.asParameters();
        SCOPED_TRACE(testParameters);

        const addrinfo hints = {
                .ai_flags = config.ai_flags,
                .ai_family = config.ai_family,
                .ai_socktype = config.ai_socktype,
        };

        addrinfo* result = nullptr;
        NetworkDnsEventReported event;
        int rv = resolv_getaddrinfo("localhost", config.servname, &hints, &mNetcontext, &result,
                                    &event);
        ScopedAddrinfo result_cleanup(result);
        EXPECT_EQ(config.expected_eai_error, rv);
    }
}

TEST_F(ResolvGetAddrInfoTest, AlphabeticalHostname_NoData) {
    constexpr char v4_host_name[] = "v4only.example.com.";
    // Following fields will not be verified during the test in proto NetworkDnsEventReported.
    // So don't need to config those values: event_type, return_code, latency_micros,
    // hints_ai_flags, res_nsend_flags, network_type, private_dns_modes.
    constexpr char event_ipv6[] = R"Event(
             NetworkDnsEventReported {
             dns_query_events:
             {
               dns_query_event:[
                {
                 rcode: 0,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 latency_micros: 0,
                 linux_errno: 0,
                },
                {
                 rcode: 0,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 latency_micros: 0,
                 linux_errno: 0,
                },
                {
                 rcode: 0,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 latency_micros: 0,
                 linux_errno: 0,
                },
                {
                 rcode: 0,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 latency_micros: 0,
                 linux_errno: 0,
                }
               ]
             }
        })Event";

    test::DNSResponder dns;
    dns.addMapping(v4_host_name, ns_type::ns_t_a, "1.2.3.3");
    ASSERT_TRUE(dns.startServer());
    ASSERT_EQ(0, SetResolvers());

    // Want AAAA answer but DNS server has A answer only.
    addrinfo* result = nullptr;
    const addrinfo hints = {.ai_family = AF_INET6};
    NetworkDnsEventReported event;
    int rv = resolv_getaddrinfo("v4only", nullptr, &hints, &mNetcontext, &result, &event);
    EXPECT_THAT(event, NetworkDnsEventEq(fromNetworkDnsEventReportedStr(event_ipv6)));
    ScopedAddrinfo result_cleanup(result);
    EXPECT_LE(1U, GetNumQueries(dns, v4_host_name));
    EXPECT_EQ(nullptr, result);
    EXPECT_EQ(EAI_NODATA, rv);
}

TEST_F(ResolvGetAddrInfoTest, AlphabeticalHostname) {
    constexpr char host_name[] = "sawadee.example.com.";
    constexpr char v4addr[] = "1.2.3.4";
    constexpr char v6addr[] = "::1.2.3.4";
    // Following fields will not be verified during the test in proto NetworkDnsEventReported.
    // So don't need to config those values: event_type, return_code, latency_micros,
    // hints_ai_flags, res_nsend_flags, network_type, private_dns_modes.
    constexpr char event_ipv4[] = R"Event(
             NetworkDnsEventReported {
             dns_query_events:
             {
               dns_query_event:[
                {
                 rcode: 0,
                 type: 1,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 0,
                },
                {
                 rcode: 0,
                 type: 1,
                 cache_hit: 2,
                 ip_version: 0,
                 protocol: 0,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 0,
                }
               ]
             }
        })Event";

    constexpr char event_ipv6[] = R"Event(
             NetworkDnsEventReported {
             dns_query_events:
             {
               dns_query_event:[
                {
                 rcode: 0,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 0,
                },
                {
                 rcode: 0,
                 type: 28,
                 cache_hit: 2,
                 ip_version: 0,
                 protocol: 0,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 0,
                }
               ]
             }
        })Event";
    test::DNSResponder dns;
    dns.addMapping(host_name, ns_type::ns_t_a, v4addr);
    dns.addMapping(host_name, ns_type::ns_t_aaaa, v6addr);
    ASSERT_TRUE(dns.startServer());
    ASSERT_EQ(0, SetResolvers());

    static const struct TestConfig {
        int ai_family;
        const std::string expected_addr;
        const std::string expected_event;
    } testConfigs[]{
            {AF_INET, v4addr, event_ipv4},
            {AF_INET6, v6addr, event_ipv6},
    };

    for (const auto& config : testConfigs) {
        SCOPED_TRACE(StringPrintf("family: %d", config.ai_family));
        dns.clearQueries();

        addrinfo* result = nullptr;
        const addrinfo hints = {.ai_family = config.ai_family};
        NetworkDnsEventReported event;
        int rv = resolv_getaddrinfo("sawadee", nullptr, &hints, &mNetcontext, &result, &event);
        EXPECT_THAT(event,
                    NetworkDnsEventEq(fromNetworkDnsEventReportedStr(config.expected_event)));
        ScopedAddrinfo result_cleanup(result);
        EXPECT_EQ(0, rv);
        EXPECT_TRUE(result != nullptr);
        EXPECT_EQ(1U, GetNumQueries(dns, host_name));
        EXPECT_EQ(config.expected_addr, ToString(result));
    }
}

TEST_F(ResolvGetAddrInfoTest, IllegalHostname) {
    test::DNSResponder dns;
    ASSERT_TRUE(dns.startServer());
    ASSERT_EQ(0, SetResolvers());

    // Illegal hostname is verified by res_hnok() in system/netd/resolv/res_comp.cpp.
    static constexpr char const* illegalHostnames[] = {
            kBadCharAfterPeriodHost,
            kBadCharBeforePeriodHost,
            kBadCharAtTheEndHost,
            kBadCharInTheMiddleOfLabelHost,
    };

    for (const auto& hostname : illegalHostnames) {
        // Expect to get no address because hostname format is illegal.
        //
        // Ex:
        // ANSWER SECTION:
        // a.ex^ample.com.      IN  A       1.2.3.3
        // a.ex^ample.com.      IN  AAAA    2001:db8::42
        //
        // In this example, querying "a.ex^ample.com" should get no address because
        // "a.ex^ample.com" has an illegal char '^' in the middle of label.
        dns.addMapping(hostname, ns_type::ns_t_a, "1.2.3.3");
        dns.addMapping(hostname, ns_type::ns_t_aaaa, "2001:db8::42");

        for (const auto& family : {AF_INET, AF_INET6, AF_UNSPEC}) {
            SCOPED_TRACE(StringPrintf("family: %d, config.name: %s", family, hostname));

            addrinfo* res = nullptr;
            const addrinfo hints = {.ai_family = family};
            NetworkDnsEventReported event;
            int rv = resolv_getaddrinfo(hostname, nullptr, &hints, &mNetcontext, &res, &event);
            ScopedAddrinfo result(res);
            EXPECT_EQ(nullptr, result);
            EXPECT_EQ(EAI_FAIL, rv);
        }
    }
}

TEST_F(ResolvGetAddrInfoTest, ServerResponseError) {
    constexpr char host_name[] = "hello.example.com.";

    static const struct TestConfig {
        ns_rcode rcode;
        int expected_eai_error;

        // Only test failure RCODE [1..5] in RFC 1035 section 4.1.1 and skip successful RCODE 0
        // which means no error.
    } testConfigs[]{
            // clang-format off
            {ns_rcode::ns_r_formerr,  EAI_FAIL},
            {ns_rcode::ns_r_servfail, EAI_AGAIN},
            {ns_rcode::ns_r_nxdomain, EAI_NODATA},
            {ns_rcode::ns_r_notimpl,  EAI_FAIL},
            {ns_rcode::ns_r_refused,  EAI_FAIL},
            // clang-format on
    };

    for (const auto& config : testConfigs) {
        SCOPED_TRACE(StringPrintf("rcode: %d", config.rcode));

        test::DNSResponder dns(config.rcode);
        dns.addMapping(host_name, ns_type::ns_t_a, "1.2.3.4");
        dns.setResponseProbability(0.0);  // always ignore requests and response preset rcode
        ASSERT_TRUE(dns.startServer());
        ASSERT_EQ(0, SetResolvers());

        addrinfo* result = nullptr;
        const addrinfo hints = {.ai_family = AF_UNSPEC};
        NetworkDnsEventReported event;
        int rv = resolv_getaddrinfo(host_name, nullptr, &hints, &mNetcontext, &result, &event);
        EXPECT_EQ(config.expected_eai_error, rv);
    }
}

// TODO: Add private DNS server timeout test.
TEST_F(ResolvGetAddrInfoTest, ServerTimeout) {
    constexpr char host_name[] = "hello.example.com.";
    // Following fields will not be verified during the test in proto NetworkDnsEventReported.
    // So don't need to config those values: event_type, return_code, latency_micros,
    // hints_ai_flags, res_nsend_flags, network_type, private_dns_modes.
    // Expected_event is 16 DNS queries and only "type" and "retry_times" fields are changed.
    // 2(T_AAAA + T_A) * 2(w/ retry) * 2(query w/ and w/o domain) * 2(SOCK_DGRAM and SOCK_STREAM)
    constexpr char expected_event[] = R"Event(
             NetworkDnsEventReported {
             dns_query_events:
             {
               dns_query_event:[
                {
                 rcode: 255,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 110,
                },
                {
                 rcode: 255,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 1,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 110,
                }
                {
                 rcode: 255,
                 type: 1,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 110,
                },
                {
                 rcode: 255,
                 type: 1,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 1,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 110,
                },
                {
                 rcode: 255,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 110,
                },
                {
                 rcode: 255,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 1,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 110,
                }
                {
                 rcode: 255,
                 type: 1,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 110,
                },
                {
                 rcode: 255,
                 type: 1,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 1,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 110,
                },
                {
                 rcode: 255,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 110,
                },
                {
                 rcode: 255,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 1,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 110,
                }
                {
                 rcode: 255,
                 type: 1,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 110,
                },
                {
                 rcode: 255,
                 type: 1,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 1,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 110,
                },
                {
                 rcode: 255,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 110,
                },
                {
                 rcode: 255,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 1,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 110,
                }
                {
                 rcode: 255,
                 type: 1,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 110,
                },
                {
                 rcode: 255,
                 type: 1,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 1,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 110,
                },
               ]
             }
        })Event";
    test::DNSResponder dns(static_cast<ns_rcode>(-1) /*no response*/);
    dns.addMapping(host_name, ns_type::ns_t_a, "1.2.3.4");
    dns.setResponseProbability(0.0);  // always ignore requests and don't response
    ASSERT_TRUE(dns.startServer());
    ASSERT_EQ(0, SetResolvers());

    addrinfo* result = nullptr;
    const addrinfo hints = {.ai_family = AF_UNSPEC};
    NetworkDnsEventReported event;
    int rv = resolv_getaddrinfo("hello", nullptr, &hints, &mNetcontext, &result, &event);
    EXPECT_THAT(event, NetworkDnsEventEq(fromNetworkDnsEventReportedStr(expected_event)));
    EXPECT_EQ(NETD_RESOLV_TIMEOUT, rv);
}

TEST_F(ResolvGetAddrInfoTest, CnamesNoIpAddress) {
    constexpr char ACNAME[] = "acname";  // expect a cname in answer
    constexpr char CNAMES[] = "cnames";  // expect cname chain in answer

    test::DNSResponder dns;
    dns.addMapping("cnames.example.com.", ns_type::ns_t_cname, "acname.example.com.");
    dns.addMapping("acname.example.com.", ns_type::ns_t_cname, "hello.example.com.");
    ASSERT_TRUE(dns.startServer());
    ASSERT_EQ(0, SetResolvers());

    static const struct TestConfig {
        const char* name;
        int family;
    } testConfigs[]{
            // clang-format off
            {ACNAME, AF_INET},
            {ACNAME, AF_INET6},
            {ACNAME, AF_UNSPEC},
            {CNAMES, AF_INET},
            {CNAMES, AF_INET6},
            {CNAMES, AF_UNSPEC},
            // clang-format on
    };

    for (const auto& config : testConfigs) {
        SCOPED_TRACE(
                StringPrintf("config.family: %d, config.name: %s", config.family, config.name));

        addrinfo* res = nullptr;
        const addrinfo hints = {.ai_family = config.family};
        NetworkDnsEventReported event;
        int rv = resolv_getaddrinfo(config.name, nullptr, &hints, &mNetcontext, &res, &event);
        ScopedAddrinfo result(res);
        EXPECT_EQ(nullptr, result);
        EXPECT_EQ(EAI_FAIL, rv);
    }
}

TEST_F(ResolvGetAddrInfoTest, CnamesBrokenChainByIllegalCname) {
    test::DNSResponder dns;
    ASSERT_TRUE(dns.startServer());
    ASSERT_EQ(0, SetResolvers());

    static const struct TestConfig {
        const char* name;
        const char* cname;
        std::string asHostName() const { return StringPrintf("%s.example.com.", name); }

        // Illegal cname is verified by res_hnok() in system/netd/resolv/res_comp.cpp.
    } testConfigs[]{
            // clang-format off
            {NAME(kBadCharAfterPeriodHost),        kBadCharAfterPeriodHost},
            {NAME(kBadCharBeforePeriodHost),       kBadCharBeforePeriodHost},
            {NAME(kBadCharAtTheEndHost),           kBadCharAtTheEndHost},
            {NAME(kBadCharInTheMiddleOfLabelHost), kBadCharInTheMiddleOfLabelHost},
            // clang-format on
    };

    for (const auto& config : testConfigs) {
        const std::string testHostName = config.asHostName();

        // Expect to get no address because the cname chain is broken by an illegal cname format.
        //
        // Ex:
        // ANSWER SECTION:
        // hello.example.com.   IN  CNAME   a.ex^ample.com.
        // a.ex^ample.com.      IN  A       1.2.3.3
        // a.ex^ample.com.      IN  AAAA    2001:db8::42
        //
        // In this example, querying hello.example.com should get no address because
        // "a.ex^ample.com" has an illegal char '^' in the middle of label.
        dns.addMapping(testHostName.c_str(), ns_type::ns_t_cname, config.cname);
        dns.addMapping(config.cname, ns_type::ns_t_a, "1.2.3.3");
        dns.addMapping(config.cname, ns_type::ns_t_aaaa, "2001:db8::42");

        for (const auto& family : {AF_INET, AF_INET6, AF_UNSPEC}) {
            SCOPED_TRACE(
                    StringPrintf("family: %d, testHostName: %s", family, testHostName.c_str()));

            addrinfo* res = nullptr;
            const addrinfo hints = {.ai_family = family};
            NetworkDnsEventReported event;
            int rv = resolv_getaddrinfo(config.name, nullptr, &hints, &mNetcontext, &res, &event);
            ScopedAddrinfo result(res);
            EXPECT_EQ(nullptr, result);
            EXPECT_EQ(EAI_FAIL, rv);
        }
    }
}

TEST_F(ResolvGetAddrInfoTest, CnamesInfiniteLoop) {
    test::DNSResponder dns;
    dns.addMapping("hello.example.com.", ns_type::ns_t_cname, "a.example.com.");
    dns.addMapping("a.example.com.", ns_type::ns_t_cname, "hello.example.com.");
    ASSERT_TRUE(dns.startServer());
    ASSERT_EQ(0, SetResolvers());

    for (const auto& family : {AF_INET, AF_INET6, AF_UNSPEC}) {
        SCOPED_TRACE(StringPrintf("family: %d", family));

        addrinfo* res = nullptr;
        const addrinfo hints = {.ai_family = family};
        NetworkDnsEventReported event;
        int rv = resolv_getaddrinfo("hello", nullptr, &hints, &mNetcontext, &res, &event);
        ScopedAddrinfo result(res);
        EXPECT_EQ(nullptr, result);
        EXPECT_EQ(EAI_FAIL, rv);
    }
}

TEST_F(ResolvGetAddrInfoTest, MultiAnswerSections) {
    test::DNSResponder dns(test::DNSResponder::MappingType::DNS_HEADER);
    // Answer section for query type {A, AAAA}
    // Type A:
    //   hello.example.com.   IN    A       1.2.3.1
    //   hello.example.com.   IN    A       1.2.3.2
    // Type AAAA:
    //   hello.example.com.   IN    AAAA    2001:db8::41
    //   hello.example.com.   IN    AAAA    2001:db8::42
    StartDns(dns, {MakeDnsMessage(kHelloExampleCom, ns_type::ns_t_a, {"1.2.3.1", "1.2.3.2"}),
                   MakeDnsMessage(kHelloExampleCom, ns_type::ns_t_aaaa,
                                  {"2001:db8::41", "2001:db8::42"})});
    ASSERT_EQ(0, SetResolvers());

    for (const auto& family : {AF_INET, AF_INET6, AF_UNSPEC}) {
        SCOPED_TRACE(StringPrintf("family: %d", family));

        addrinfo* res = nullptr;
        // If the socket type is not specified, every address will appear twice, once for
        // SOCK_STREAM and one for SOCK_DGRAM. Just pick one because the addresses for
        // the second query of different socket type are responded by the cache.
        const addrinfo hints = {.ai_family = family, .ai_socktype = SOCK_STREAM};
        NetworkDnsEventReported event;
        int rv = resolv_getaddrinfo("hello", nullptr, &hints, &mNetcontext, &res, &event);
        ScopedAddrinfo result(res);
        ASSERT_NE(nullptr, result);
        ASSERT_EQ(0, rv);

        const std::vector<std::string> result_strs = ToStrings(result);
        if (family == AF_INET) {
            EXPECT_EQ(1U, GetNumQueries(dns, kHelloExampleCom));
            EXPECT_THAT(result_strs, testing::UnorderedElementsAreArray({"1.2.3.1", "1.2.3.2"}));
        } else if (family == AF_INET6) {
            EXPECT_EQ(1U, GetNumQueries(dns, kHelloExampleCom));
            EXPECT_THAT(result_strs,
                        testing::UnorderedElementsAreArray({"2001:db8::41", "2001:db8::42"}));
        } else if (family == AF_UNSPEC) {
            EXPECT_EQ(0U, GetNumQueries(dns, kHelloExampleCom));  // no query because of the cache
            EXPECT_THAT(result_strs,
                        testing::UnorderedElementsAreArray(
                                {"1.2.3.1", "1.2.3.2", "2001:db8::41", "2001:db8::42"}));
        }
        dns.clearQueries();
    }
}

TEST_F(ResolvGetAddrInfoTest, TruncatedResponse) {
    // Following fields will not be verified during the test in proto NetworkDnsEventReported.
    // So don't need to config those values: event_type, return_code, latency_micros,
    // hints_ai_flags, res_nsend_flags, network_type, private_dns_modes.
    constexpr char event_ipv4[] = R"Event(
             NetworkDnsEventReported {
             dns_query_events:
             {
               dns_query_event:[
                {
                 rcode: 254,
                 type: 1,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 7,
                },
                {
                 rcode: 0,
                 type: 1,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 2,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 0,
                },
                {
                 rcode: 0,
                 type: 1,
                 cache_hit: 2,
                 ip_version: 0,
                 protocol: 0,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 0,
                }
               ]
             }
        })Event";

    constexpr char event_ipv6[] = R"Event(
             NetworkDnsEventReported {
             dns_query_events:
             {
               dns_query_event:[
                {
                 rcode: 254,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 7,
                },
                {
                 rcode: 0,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 2,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 0,
                },
                {
                 rcode: 0,
                 type: 28,
                 cache_hit: 2,
                 ip_version: 0,
                 protocol: 0,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 linux_errno: 0,
                }
               ]
             }
        })Event";

    test::DNSResponder dns;
    dns.addMapping(kHelloExampleCom, ns_type::ns_t_cname, kCnameA);
    dns.addMapping(kCnameA, ns_type::ns_t_cname, kCnameB);
    dns.addMapping(kCnameB, ns_type::ns_t_cname, kCnameC);
    dns.addMapping(kCnameC, ns_type::ns_t_cname, kCnameD);
    dns.addMapping(kCnameD, ns_type::ns_t_a, kHelloExampleComAddrV4);
    dns.addMapping(kCnameD, ns_type::ns_t_aaaa, kHelloExampleComAddrV6);
    ASSERT_TRUE(dns.startServer());
    ASSERT_EQ(0, SetResolvers());

    static const struct TestConfig {
        int ai_family;
        const std::string expected_addr;
        const std::string expected_event;
    } testConfigs[]{
            {AF_INET, kHelloExampleComAddrV4, event_ipv4},
            {AF_INET6, kHelloExampleComAddrV6, event_ipv6},
    };

    for (const auto& config : testConfigs) {
        SCOPED_TRACE(StringPrintf("family: %d", config.ai_family));
        dns.clearQueries();

        addrinfo* result = nullptr;
        const addrinfo hints = {.ai_family = config.ai_family};
        NetworkDnsEventReported event;
        int rv = resolv_getaddrinfo("hello", nullptr, &hints, &mNetcontext, &result, &event);
        EXPECT_THAT(event,
                    NetworkDnsEventEq(fromNetworkDnsEventReportedStr(config.expected_event)));
        ScopedAddrinfo result_cleanup(result);
        EXPECT_EQ(rv, 0);
        EXPECT_TRUE(result != nullptr);
        // Expect UDP response is truncated. The resolver retries over TCP. See RFC 1035
        // section 4.2.1.
        EXPECT_EQ(GetNumQueriesForProtocol(dns, IPPROTO_UDP, kHelloExampleCom), 1U);
        EXPECT_EQ(GetNumQueriesForProtocol(dns, IPPROTO_TCP, kHelloExampleCom), 1U);
        EXPECT_EQ(ToString(result), config.expected_addr);
    }
}

// Audit if Resolver read out of bounds, which needs HWAddressSanitizer build to trigger SIGABRT.
TEST_F(ResolvGetAddrInfoTest, OverlengthResp) {
    std::vector<std::string> nameList;
    // Construct a long enough record that exceeds 8192 bytes (the maximum buffer size):
    // Header: (Transaction ID, Flags, ...)                                        = 12   bytes
    // Query: 19(Name)+2(Type)+2(Class)                                            = 23   bytes
    // The 1st answer RR: 19(Name)+2(Type)+2(Class)+4(TTL)+2(Len)+77(CNAME)        = 106  bytes
    // 2nd-50th answer RRs: 49*(77(Name)+2(Type)+2(Class)+4(TTL)+2(Len)+77(CNAME)) = 8036 bytes
    // The last answer RR: 77(Name)+2(Type)+2(Class)+4(TTL)+2(Len)+4(Address)      = 91   bytes
    // ----------------------------------------------------------------------------------------
    // Sum:                                                                          8268 bytes
    for (int i = 0; i < 10; i++) {
        std::string domain(kMaxmiumLabelSize / 2, 'a' + i);
        for (int j = 0; j < 5; j++) {
            nameList.push_back(domain + std::string(kMaxmiumLabelSize / 2 + 1, '0' + j) +
                               kExampleComDomain + ".");
        }
    }
    test::DNSResponder dns;
    dns.addMapping(kHelloExampleCom, ns_type::ns_t_cname, nameList[0]);
    for (size_t i = 0; i < nameList.size() - 1; i++) {
        dns.addMapping(nameList[i], ns_type::ns_t_cname, nameList[i + 1]);
    }
    dns.addMapping(nameList[nameList.size() - 1], ns_type::ns_t_a, kHelloExampleComAddrV4);

    ASSERT_TRUE(dns.startServer());
    ASSERT_EQ(0, SetResolvers());
    addrinfo* result = nullptr;
    const addrinfo hints = {.ai_family = AF_INET};
    NetworkDnsEventReported event;
    int rv = resolv_getaddrinfo("hello", nullptr, &hints, &mNetcontext, &result, &event);
    ScopedAddrinfo result_cleanup(result);
    EXPECT_EQ(rv, EAI_FAIL);
    EXPECT_TRUE(result == nullptr);
    EXPECT_EQ(GetNumQueriesForProtocol(dns, IPPROTO_UDP, kHelloExampleCom), 2U);
    EXPECT_EQ(GetNumQueriesForProtocol(dns, IPPROTO_TCP, kHelloExampleCom), 2U);
}

TEST_F(GetHostByNameForNetContextTest, AlphabeticalHostname) {
    constexpr char host_name[] = "jiababuei.example.com.";
    constexpr char v4addr[] = "1.2.3.4";
    constexpr char v6addr[] = "::1.2.3.4";
    // Following fields will not be verified during the test in proto NetworkDnsEventReported.
    // So don't need to config those values: event_type, return_code, latency_micros,
    // hints_ai_flags, res_nsend_flags, network_type, private_dns_modes.
    constexpr char event_ipv4[] = R"Event(
             NetworkDnsEventReported {
             dns_query_events:
             {
               dns_query_event:[
                {
                 rcode: 0,
                 type: 1,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 latency_micros: 0,
                 linux_errno: 0,
                }
               ]
             }
        })Event";

    constexpr char event_ipv6[] = R"Event(
             NetworkDnsEventReported {
             dns_query_events:
             {
               dns_query_event:[
                {
                 rcode: 0,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 0,
                 dns_server_index: 0,
                 connected: 0,
                 latency_micros: 0,
                 linux_errno: 0,
                }
               ]
             }
        })Event";
    test::DNSResponder dns;
    dns.addMapping(host_name, ns_type::ns_t_a, v4addr);
    dns.addMapping(host_name, ns_type::ns_t_aaaa, v6addr);
    ASSERT_TRUE(dns.startServer());
    ASSERT_EQ(0, SetResolvers());

    static const struct TestConfig {
        int ai_family;
        const std::string expected_addr;
        const std::string expected_event;
    } testConfigs[]{
            {AF_INET, v4addr, event_ipv4},
            {AF_INET6, v6addr, event_ipv6},
    };

    for (const auto& config : testConfigs) {
        SCOPED_TRACE(StringPrintf("family: %d", config.ai_family));
        dns.clearQueries();

        hostent* hp = nullptr;
        hostent hbuf;
        char tmpbuf[MAXPACKET];
        NetworkDnsEventReported event;
        int rv = resolv_gethostbyname("jiababuei", config.ai_family, &hbuf, tmpbuf, sizeof(tmpbuf),
                                      &mNetcontext, &hp, &event);
        EXPECT_THAT(event,
                    NetworkDnsEventEq(fromNetworkDnsEventReportedStr(config.expected_event)));
        EXPECT_EQ(0, rv);
        EXPECT_TRUE(hp != nullptr);
        EXPECT_EQ(1U, GetNumQueries(dns, host_name));
        EXPECT_EQ(config.expected_addr, ToString(hp));
    }
}

TEST_F(GetHostByNameForNetContextTest, IllegalHostname) {
    test::DNSResponder dns;
    ASSERT_TRUE(dns.startServer());
    ASSERT_EQ(0, SetResolvers());

    // Illegal hostname is verified by res_hnok() in system/netd/resolv/res_comp.cpp.
    static constexpr char const* illegalHostnames[] = {
            kBadCharAfterPeriodHost,
            kBadCharBeforePeriodHost,
            kBadCharAtTheEndHost,
            kBadCharInTheMiddleOfLabelHost,
    };

    for (const auto& hostname : illegalHostnames) {
        // Expect to get no address because hostname format is illegal.
        //
        // Ex:
        // ANSWER SECTION:
        // a.ex^ample.com.      IN  A       1.2.3.3
        // a.ex^ample.com.      IN  AAAA    2001:db8::42
        //
        // In this example, querying "a.ex^ample.com" should get no address because
        // "a.ex^ample.com" has an illegal char '^' in the middle of label.
        dns.addMapping(hostname, ns_type::ns_t_a, "1.2.3.3");
        dns.addMapping(hostname, ns_type::ns_t_aaaa, "2001:db8::42");

        for (const auto& family : {AF_INET, AF_INET6}) {
            SCOPED_TRACE(StringPrintf("family: %d, config.name: %s", family, hostname));

            struct hostent* hp = nullptr;
            hostent hbuf;
            char tmpbuf[MAXPACKET];
            NetworkDnsEventReported event;
            int rv = resolv_gethostbyname(hostname, family, &hbuf, tmpbuf, sizeof(tmpbuf),
                                          &mNetcontext, &hp, &event);
            EXPECT_EQ(nullptr, hp);
            EXPECT_EQ(EAI_FAIL, rv);
        }
    }
}

TEST_F(GetHostByNameForNetContextTest, NoData) {
    constexpr char v4_host_name[] = "v4only.example.com.";

    test::DNSResponder dns;
    dns.addMapping(v4_host_name, ns_type::ns_t_a, "1.2.3.3");
    ASSERT_TRUE(dns.startServer());
    ASSERT_EQ(0, SetResolvers());
    dns.clearQueries();

    // Want AAAA answer but DNS server has A answer only.
    hostent* hp = nullptr;
    hostent hbuf;
    char tmpbuf[MAXPACKET];
    NetworkDnsEventReported event;
    int rv = resolv_gethostbyname("v4only", AF_INET6, &hbuf, tmpbuf, sizeof tmpbuf, &mNetcontext,
                                  &hp, &event);
    EXPECT_LE(1U, GetNumQueries(dns, v4_host_name));
    EXPECT_EQ(nullptr, hp);
    EXPECT_EQ(EAI_NODATA, rv);
}

TEST_F(GetHostByNameForNetContextTest, ServerResponseError) {
    constexpr char host_name[] = "hello.example.com.";

    static const struct TestConfig {
        ns_rcode rcode;
        int expected_eai_error;  // expected result

        // Only test failure RCODE [1..5] in RFC 1035 section 4.1.1 and skip successful RCODE 0
        // which means no error. Note that the return error codes aren't mapped by rcode in the
        // test case SERVFAIL, NOTIMP and REFUSED. See the comment of res_nsend()
        // in system\netd\resolv\res_query.cpp for more detail.
    } testConfigs[]{
            // clang-format off
            {ns_rcode::ns_r_formerr, EAI_FAIL},
            {ns_rcode::ns_r_servfail, EAI_AGAIN},  // Not mapped by rcode.
            {ns_rcode::ns_r_nxdomain, EAI_NODATA},
            {ns_rcode::ns_r_notimpl, EAI_AGAIN},  // Not mapped by rcode.
            {ns_rcode::ns_r_refused, EAI_AGAIN},  // Not mapped by rcode.
            // clang-format on
    };

    for (const auto& config : testConfigs) {
        SCOPED_TRACE(StringPrintf("rcode: %d", config.rcode));

        test::DNSResponder dns(config.rcode);
        dns.addMapping(host_name, ns_type::ns_t_a, "1.2.3.4");
        dns.setResponseProbability(0.0);  // always ignore requests and response preset rcode
        ASSERT_TRUE(dns.startServer());
        ASSERT_EQ(0, SetResolvers());

        hostent* hp = nullptr;
        hostent hbuf;
        char tmpbuf[MAXPACKET];
        NetworkDnsEventReported event;
        int rv = resolv_gethostbyname(host_name, AF_INET, &hbuf, tmpbuf, sizeof tmpbuf,
                                      &mNetcontext, &hp, &event);
        EXPECT_EQ(nullptr, hp);
        EXPECT_EQ(config.expected_eai_error, rv);
    }
}

// TODO: Add private DNS server timeout test.
TEST_F(GetHostByNameForNetContextTest, ServerTimeout) {
    constexpr char host_name[] = "hello.example.com.";
    test::DNSResponder dns(static_cast<ns_rcode>(-1) /*no response*/);
    dns.addMapping(host_name, ns_type::ns_t_a, "1.2.3.4");
    dns.setResponseProbability(0.0);  // always ignore requests and don't response
    ASSERT_TRUE(dns.startServer());
    ASSERT_EQ(0, SetResolvers());

    hostent* hp = nullptr;
    hostent hbuf;
    char tmpbuf[MAXPACKET];
    NetworkDnsEventReported event;
    int rv = resolv_gethostbyname(host_name, AF_INET, &hbuf, tmpbuf, sizeof tmpbuf, &mNetcontext,
                                  &hp, &event);
    EXPECT_EQ(NETD_RESOLV_TIMEOUT, rv);
}

TEST_F(GetHostByNameForNetContextTest, CnamesNoIpAddress) {
    constexpr char ACNAME[] = "acname";  // expect a cname in answer
    constexpr char CNAMES[] = "cnames";  // expect cname chain in answer

    test::DNSResponder dns;
    dns.addMapping("cnames.example.com.", ns_type::ns_t_cname, "acname.example.com.");
    dns.addMapping("acname.example.com.", ns_type::ns_t_cname, "hello.example.com.");
    ASSERT_TRUE(dns.startServer());
    ASSERT_EQ(0, SetResolvers());

    static const struct TestConfig {
        const char* name;
        int family;
    } testConfigs[]{
            {ACNAME, AF_INET},
            {ACNAME, AF_INET6},
            {CNAMES, AF_INET},
            {CNAMES, AF_INET6},
    };

    for (const auto& config : testConfigs) {
        SCOPED_TRACE(
                StringPrintf("config.family: %d, config.name: %s", config.family, config.name));

        struct hostent* hp = nullptr;
        hostent hbuf;
        char tmpbuf[MAXPACKET];
        NetworkDnsEventReported event;
        int rv = resolv_gethostbyname(config.name, config.family, &hbuf, tmpbuf, sizeof tmpbuf,
                                      &mNetcontext, &hp, &event);
        EXPECT_EQ(nullptr, hp);
        EXPECT_EQ(EAI_FAIL, rv);
    }
}

TEST_F(GetHostByNameForNetContextTest, CnamesBrokenChainByIllegalCname) {
    test::DNSResponder dns;
    ASSERT_TRUE(dns.startServer());
    ASSERT_EQ(0, SetResolvers());

    static const struct TestConfig {
        const char* name;
        const char* cname;
        std::string asHostName() const { return StringPrintf("%s.example.com.", name); }

        // Illegal cname is verified by res_hnok() in system/netd/resolv/res_comp.cpp
    } testConfigs[]{
            // clang-format off
            {NAME(kBadCharAfterPeriodHost),        kBadCharAfterPeriodHost},
            {NAME(kBadCharBeforePeriodHost),       kBadCharBeforePeriodHost},
            {NAME(kBadCharAtTheEndHost),           kBadCharAtTheEndHost},
            {NAME(kBadCharInTheMiddleOfLabelHost), kBadCharInTheMiddleOfLabelHost},
            // clang-format on
    };

    for (const auto& config : testConfigs) {
        const std::string testHostName = config.asHostName();

        // Expect to get no address because the cname chain is broken by an illegal cname format.
        //
        // Ex:
        // ANSWER SECTION:
        // hello.example.com.   IN  CNAME   a.ex^ample.com.
        // a.ex^ample.com.      IN  A       1.2.3.3
        // a.ex^ample.com.      IN  AAAA    2001:db8::42
        //
        // In this example, querying hello.example.com should get no address because
        // "a.ex^ample.com" has an illegal char '^' in the middle of label.
        dns.addMapping(testHostName.c_str(), ns_type::ns_t_cname, config.cname);
        dns.addMapping(config.cname, ns_type::ns_t_a, "1.2.3.3");
        dns.addMapping(config.cname, ns_type::ns_t_aaaa, "2001:db8::42");

        for (const auto& family : {AF_INET, AF_INET6}) {
            SCOPED_TRACE(
                    StringPrintf("family: %d, testHostName: %s", family, testHostName.c_str()));

            struct hostent* hp = nullptr;
            hostent hbuf;
            char tmpbuf[MAXPACKET];
            NetworkDnsEventReported event;
            int rv = resolv_gethostbyname(config.name, family, &hbuf, tmpbuf, sizeof tmpbuf,
                                          &mNetcontext, &hp, &event);
            EXPECT_EQ(nullptr, hp);
            EXPECT_EQ(EAI_FAIL, rv);
        }
    }
}

TEST_F(GetHostByNameForNetContextTest, CnamesInfiniteLoop) {
    test::DNSResponder dns;
    dns.addMapping("hello.example.com.", ns_type::ns_t_cname, "a.example.com.");
    dns.addMapping("a.example.com.", ns_type::ns_t_cname, "hello.example.com.");
    ASSERT_TRUE(dns.startServer());
    ASSERT_EQ(0, SetResolvers());

    for (const auto& family : {AF_INET, AF_INET6}) {
        SCOPED_TRACE(StringPrintf("family: %d", family));

        struct hostent* hp = nullptr;
        hostent hbuf;
        char tmpbuf[MAXPACKET];
        NetworkDnsEventReported event;
        int rv = resolv_gethostbyname("hello", family, &hbuf, tmpbuf, sizeof tmpbuf, &mNetcontext,
                                      &hp, &event);
        EXPECT_EQ(nullptr, hp);
        EXPECT_EQ(EAI_FAIL, rv);
    }
}

TEST_F(ResolvCommonFunctionTest, GetCustTableByName) {
    const char custAddrV4[] = "1.2.3.4";
    const char custAddrV6[] = "::1.2.3.4";
    const char hostnameV4V6[] = "v4v6.example.com.";
    const aidl::android::net::ResolverOptionsParcel& resolverOptions = {
            {
                    {custAddrV4, hostnameV4V6},
                    {custAddrV6, hostnameV4V6},
            },
            aidl::android::net::IDnsResolver::TC_MODE_DEFAULT};
    const std::vector<int32_t>& transportTypes = {IDnsResolver::TRANSPORT_WIFI};
    EXPECT_EQ(0, resolv_set_nameservers(TEST_NETID, servers, domains, params, resolverOptions,
                                        transportTypes));
    EXPECT_THAT(getCustomizedTableByName(TEST_NETID, hostnameV4V6),
                testing::UnorderedElementsAreArray({custAddrV4, custAddrV6}));

    // Query address by mismatch hostname.
    ASSERT_TRUE(getCustomizedTableByName(TEST_NETID, "not.in.cust.table").empty());

    // Query address by different netid.
    ASSERT_TRUE(getCustomizedTableByName(TEST_NETID + 1, hostnameV4V6).empty());
    resolv_create_cache_for_net(TEST_NETID + 1);
    EXPECT_EQ(0, resolv_set_nameservers(TEST_NETID + 1, servers, domains, params, resolverOptions,
                                        transportTypes));
    EXPECT_THAT(getCustomizedTableByName(TEST_NETID + 1, hostnameV4V6),
                testing::UnorderedElementsAreArray({custAddrV4, custAddrV6}));
}

TEST_F(ResolvCommonFunctionTest, GetNetworkTypesForNet) {
    const aidl::android::net::ResolverOptionsParcel& resolverOptions = {
            {} /* hosts */, aidl::android::net::IDnsResolver::TC_MODE_DEFAULT};
    const std::vector<int32_t>& transportTypes = {IDnsResolver::TRANSPORT_WIFI,
                                                  IDnsResolver::TRANSPORT_VPN};
    EXPECT_EQ(0, resolv_set_nameservers(TEST_NETID, servers, domains, params, resolverOptions,
                                        transportTypes));
    EXPECT_EQ(android::net::NT_WIFI_VPN, resolv_get_network_types_for_net(TEST_NETID));
}

TEST_F(ResolvCommonFunctionTest, ConvertTransportsToNetworkType) {
    static const struct TestConfig {
        int32_t networkType;
        std::vector<int32_t> transportTypes;
    } testConfigs[] = {
            {android::net::NT_CELLULAR, {IDnsResolver::TRANSPORT_CELLULAR}},
            {android::net::NT_WIFI, {IDnsResolver::TRANSPORT_WIFI}},
            {android::net::NT_BLUETOOTH, {IDnsResolver::TRANSPORT_BLUETOOTH}},
            {android::net::NT_ETHERNET, {IDnsResolver::TRANSPORT_ETHERNET}},
            {android::net::NT_VPN, {IDnsResolver::TRANSPORT_VPN}},
            {android::net::NT_WIFI_AWARE, {IDnsResolver::TRANSPORT_WIFI_AWARE}},
            {android::net::NT_LOWPAN, {IDnsResolver::TRANSPORT_LOWPAN}},
            {android::net::NT_CELLULAR_VPN,
             {IDnsResolver::TRANSPORT_CELLULAR, IDnsResolver::TRANSPORT_VPN}},
            {android::net::NT_CELLULAR_VPN,
             {IDnsResolver::TRANSPORT_VPN, IDnsResolver::TRANSPORT_CELLULAR}},
            {android::net::NT_WIFI_VPN,
             {IDnsResolver::TRANSPORT_WIFI, IDnsResolver::TRANSPORT_VPN}},
            {android::net::NT_WIFI_VPN,
             {IDnsResolver::TRANSPORT_VPN, IDnsResolver::TRANSPORT_WIFI}},
            {android::net::NT_BLUETOOTH_VPN,
             {IDnsResolver::TRANSPORT_BLUETOOTH, IDnsResolver::TRANSPORT_VPN}},
            {android::net::NT_BLUETOOTH_VPN,
             {IDnsResolver::TRANSPORT_VPN, IDnsResolver::TRANSPORT_BLUETOOTH}},
            {android::net::NT_ETHERNET_VPN,
             {IDnsResolver::TRANSPORT_ETHERNET, IDnsResolver::TRANSPORT_VPN}},
            {android::net::NT_ETHERNET_VPN,
             {IDnsResolver::TRANSPORT_VPN, IDnsResolver::TRANSPORT_ETHERNET}},
            {android::net::NT_UNKNOWN, {IDnsResolver::TRANSPORT_VPN, IDnsResolver::TRANSPORT_VPN}},
            {android::net::NT_UNKNOWN,
             {IDnsResolver::TRANSPORT_WIFI, IDnsResolver::TRANSPORT_LOWPAN}},
            {android::net::NT_UNKNOWN, {}},
            {android::net::NT_UNKNOWN,
             {IDnsResolver::TRANSPORT_CELLULAR, IDnsResolver::TRANSPORT_BLUETOOTH,
              IDnsResolver::TRANSPORT_VPN}},
            {android::net::NT_WIFI_CELLULAR_VPN,
             {IDnsResolver::TRANSPORT_CELLULAR, IDnsResolver::TRANSPORT_WIFI,
              IDnsResolver::TRANSPORT_VPN}},
            {android::net::NT_WIFI_CELLULAR_VPN,
             {IDnsResolver::TRANSPORT_VPN, IDnsResolver::TRANSPORT_WIFI,
              IDnsResolver::TRANSPORT_CELLULAR}},
    };
    for (const auto& config : testConfigs) {
        EXPECT_EQ(config.networkType, convert_network_type(config.transportTypes));
    }
}

// Note that local host file function, files_getaddrinfo(), of resolv_getaddrinfo()
// is not tested because it only returns a boolean (success or failure) without any error number.

// TODO: Add test for resolv_getaddrinfo().
//       - DNS response message parsing.
//           - Unexpected type of resource record (RR).
//           - Invalid length CNAME, or QNAME.
//           - Unexpected amount of questions.
//       - CNAME RDATA with the domain name which has null label(s).
// TODO: Add test for resolv_gethostbyname().
//       - Invalid parameters.
//       - DNS response message parsing.
//           - Unexpected type of resource record (RR).
//           - Invalid length CNAME, or QNAME.
//           - Unexpected amount of questions.
//       - CNAME RDATA with the domain name which has null label(s).
// TODO: Add test for resolv_gethostbyaddr().

}  // end of namespace net
}  // end of namespace android
