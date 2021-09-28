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

#define LOG_TAG "resolv_gold_test"

#include <Fwmark.h>
#include <android-base/chrono_utils.h>
#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/result.h>
#include <android-base/stringprintf.h>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "PrivateDnsConfiguration.h"
#include "getaddrinfo.h"
#include "gethnamaddr.h"
#include "golddata.pb.h"
#include "resolv_cache.h"
#include "resolv_test_utils.h"
#include "tests/dns_responder/dns_responder.h"
#include "tests/dns_responder/dns_responder_client_ndk.h"
#include "tests/dns_responder/dns_tls_certificate.h"
#include "tests/dns_responder/dns_tls_frontend.h"

namespace android::net {

using android::base::Result;
using android::base::StringPrintf;
using android::netdutils::ScopedAddrinfo;
using std::chrono::milliseconds;

enum class DnsProtocol { CLEARTEXT, TLS };

// The buffer size of resolv_gethostbyname().
// TODO: Consider moving to packages/modules/DnsResolver/tests/resolv_test_utils.h.
constexpr unsigned int MAXPACKET = 8 * 1024;

// The testdata/*.pb are generated from testdata/*.pbtext.
const std::string kTestDataPath = android::base::GetExecutableDirectory() + "/testdata/";
const std::vector<std::string> kGoldFilesGetAddrInfo = {
        "getaddrinfo.topsite.google.pb",    "getaddrinfo.topsite.youtube.pb",
        "getaddrinfo.topsite.amazon.pb",    "getaddrinfo.topsite.yahoo.pb",
        "getaddrinfo.topsite.facebook.pb",  "getaddrinfo.topsite.reddit.pb",
        "getaddrinfo.topsite.wikipedia.pb", "getaddrinfo.topsite.ebay.pb",
        "getaddrinfo.topsite.netflix.pb",   "getaddrinfo.topsite.bing.pb"};
const std::vector<std::string> kGoldFilesGetAddrInfoTls = {"getaddrinfo.tls.topsite.google.pb"};
const std::vector<std::string> kGoldFilesGetHostByName = {"gethostbyname.topsite.youtube.pb"};
const std::vector<std::string> kGoldFilesGetHostByNameTls = {
        "gethostbyname.tls.topsite.youtube.pb"};

// Fixture test class definition.
class TestBase : public ::testing::Test {
  protected:
    static void SetUpTestSuite() {
        // Unzip *.pb from pb.zip. The unzipped files get 777 permission by default. Remove execute
        // permission so that Trade Federation test harness has no chance mis-executing on *.pb.
        const std::string unzipCmd = "unzip -o " + kTestDataPath + "pb.zip -d " + kTestDataPath +
                                     "&& chmod -R 666 " + kTestDataPath;
        // NOLINTNEXTLINE(cert-env33-c)
        if (W_EXITCODE(0, 0) != system(unzipCmd.c_str())) {
            LOG(ERROR) << "fail to inflate .pb files";
            GTEST_LOG_(FATAL) << "fail to inflate .pb files";
        }
    }

    void SetUp() override {
        // Create cache for test
        resolv_create_cache_for_net(TEST_NETID);
    }

    void TearDown() override {
        // Clear TLS configuration for test
        gPrivateDnsConfiguration.clear(TEST_NETID);
        // Delete cache for test
        resolv_delete_cache_for_net(TEST_NETID);
    }

    void SetResolverConfiguration(const std::vector<std::string>& servers,
                                  const std::vector<std::string>& domains,
                                  const std::vector<std::string>& tlsServers = {},
                                  const std::string& tlsHostname = "",
                                  const std::string& caCert = "") {
        // Determine the DNS configuration steps from setResolverConfiguration() in
        // packages/modules/DnsResolver/ResolverController.cpp. The gold test just needs to setup
        // simply DNS and DNS-over-TLS server configuration. Some implementation in
        // setResolverConfiguration() are not required. For example, limiting TLS server amount is
        // not necessary for gold test because gold test has only one TLS server for testing
        // so far.
        Fwmark fwmark;
        fwmark.netId = TEST_NETID;
        fwmark.explicitlySelected = true;
        fwmark.protectedFromVpn = true;
        fwmark.permission = PERMISSION_SYSTEM;
        ASSERT_EQ(gPrivateDnsConfiguration.set(TEST_NETID, fwmark.intValue, tlsServers, tlsHostname,
                                               caCert),
                  0);
        ASSERT_EQ(resolv_set_nameservers(TEST_NETID, servers, domains, kParams), 0);
    }

    void SetResolvers() { SetResolverConfiguration(kDefaultServers, kDefaultSearchDomains); }

    void SetResolversWithTls() {
        // Pass servers as both network-assigned and TLS servers. Tests can
        // determine on which server and by which protocol queries arrived.
        // See also DnsClient::SetResolversWithTls() in
        // packages/modules/DnsResolver/tests/dns_responder/dns_responder_client.h.
        SetResolverConfiguration(kDefaultServers, kDefaultSearchDomains, kDefaultServers,
                                 kDefaultPrivateDnsHostName, kCaCert);
    }

    bool WaitForPrivateDnsValidation(const std::string& serverAddr) {
        constexpr milliseconds retryIntervalMs{20};
        constexpr milliseconds timeoutMs{3000};
        android::base::Timer t;
        while (t.duration() < timeoutMs) {
            const auto& validatedServers =
                    gPrivateDnsConfiguration.getStatus(TEST_NETID).validatedServers();
            for (const auto& server : validatedServers) {
                if (serverAddr == ToString(&server.ss)) return true;
            }
            std::this_thread::sleep_for(retryIntervalMs);
        }
        return false;
    }

    Result<GoldTest> ToProto(const std::string& filename) {
        // Convert the testing configuration from binary .pb file to proto.
        std::string content;
        const std::string path = kTestDataPath + filename;

        bool ret = android::base::ReadFileToString(path, &content);
        if (!ret) return Errorf("Read {} failed: {}", path, strerror(errno));

        android::net::GoldTest goldtest;
        ret = goldtest.ParseFromString(content);
        if (!ret) return Errorf("Parse {} failed", path);

        return goldtest;
    }

    void SetupMappings(const android::net::GoldTest& goldtest, test::DNSResponder& dns) {
        for (const auto& m : goldtest.packet_mapping()) {
            // Convert string to bytes because .proto type "bytes" is "string" type in C++.
            // See also the section "Scalar Value Types" in "Language Guide (proto3)".
            // TODO: Use C++20 std::span in addMappingBinaryPacket. It helps to take both
            // std::string and std::vector without conversions.
            dns.addMappingBinaryPacket(
                    std::vector<uint8_t>(m.query().begin(), m.query().end()),
                    std::vector<uint8_t>(m.response().begin(), m.response().end()));
        }
    }

    android_net_context GetNetContext(const DnsProtocol protocol) {
        return protocol == DnsProtocol::TLS ? kNetcontextTls : kNetcontext;
    }

    template <class AddressType>
    void VerifyAddress(const android::net::GoldTest& goldtest, const AddressType& result) {
        if (goldtest.result().return_code() != GT_EAI_NO_ERROR) {
            EXPECT_EQ(result, nullptr);
        } else {
            ASSERT_NE(result, nullptr);
            const auto& addresses = goldtest.result().addresses();
            EXPECT_THAT(ToStrings(result), ::testing::UnorderedElementsAreArray(addresses));
        }
    }

    void VerifyGetAddrInfo(const android::net::GoldTest& goldtest, const DnsProtocol protocol) {
        ASSERT_TRUE(goldtest.config().has_addrinfo());
        const auto& args = goldtest.config().addrinfo();
        const addrinfo hints = {
                // Clear the flag AI_ADDRCONFIG to avoid flaky test because AI_ADDRCONFIG looks at
                // whether connectivity is available. It makes that the resolver may send only A
                // or AAAA DNS query per connectivity even AF_UNSPEC has been assigned. See also
                // have_ipv6() and have_ipv4() in packages/modules/DnsResolver/getaddrinfo.cpp.
                // TODO: Consider keeping the configuration flag AI_ADDRCONFIG once the unit
                // test can treat the IPv4 and IPv6 connectivity.
                .ai_flags = args.ai_flags() & ~AI_ADDRCONFIG,
                .ai_family = args.family(),
                .ai_socktype = args.socktype(),
                .ai_protocol = args.protocol(),
        };
        addrinfo* res = nullptr;
        const android_net_context netcontext = GetNetContext(protocol);
        NetworkDnsEventReported event;
        const int rv =
                resolv_getaddrinfo(args.host().c_str(), nullptr, &hints, &netcontext, &res, &event);
        ScopedAddrinfo result(res);
        ASSERT_EQ(rv, goldtest.result().return_code());
        VerifyAddress(goldtest, result);
    }

    void VerifyGetHostByName(const android::net::GoldTest& goldtest, const DnsProtocol protocol) {
        ASSERT_TRUE(goldtest.config().has_hostbyname());
        const auto& args = goldtest.config().hostbyname();
        hostent* hp = nullptr;
        hostent hbuf;
        char tmpbuf[MAXPACKET];
        const android_net_context netcontext = GetNetContext(protocol);
        NetworkDnsEventReported event;
        const int rv = resolv_gethostbyname(args.host().c_str(), args.family(), &hbuf, tmpbuf,
                                            sizeof(tmpbuf), &netcontext, &hp, &event);
        ASSERT_EQ(rv, goldtest.result().return_code());
        VerifyAddress(goldtest, hp);
    }

    void VerifyResolver(const android::net::GoldTest& goldtest, const test::DNSResponder& dns,
                        const test::DnsTlsFrontend& tls, const DnsProtocol protocol) {
        size_t queries;
        std::string name;

        // Verify DNS query calls and results by proto. Then, determine expected query times and
        // queried name for checking server query status later.
        switch (const auto calltype = goldtest.config().call()) {
            case android::net::CallType::CALL_GETADDRINFO:
                ASSERT_TRUE(goldtest.config().has_addrinfo());
                ASSERT_NO_FATAL_FAILURE(VerifyGetAddrInfo(goldtest, protocol));
                queries = goldtest.config().addrinfo().family() == AF_UNSPEC ? 2U : 1U;
                name = goldtest.config().addrinfo().host();
                break;
            case android::net::CallType::CALL_GETHOSTBYNAME:
                ASSERT_TRUE(goldtest.config().has_hostbyname());
                ASSERT_NO_FATAL_FAILURE(VerifyGetHostByName(goldtest, protocol));
                queries = 1U;
                name = goldtest.config().hostbyname().host();
                break;
            default:
                FAIL() << "Unsupported call type: " << calltype;
        }

        // Verify DNS server query status.
        EXPECT_EQ(GetNumQueries(dns, name.c_str()), queries);
        if (protocol == DnsProtocol::TLS) EXPECT_TRUE(tls.waitForQueries(queries));
    }

    static constexpr res_params kParams = {
            .sample_validity = 300,
            .success_threshold = 25,
            .min_samples = 8,
            .max_samples = 8,
            .base_timeout_msec = 1000,
            .retry_count = 2,
    };
    static constexpr android_net_context kNetcontext = {
            .app_netid = TEST_NETID,
            .app_mark = MARK_UNSET,
            .dns_netid = TEST_NETID,
            .dns_mark = MARK_UNSET,
            .uid = NET_CONTEXT_INVALID_UID,
    };
    static constexpr android_net_context kNetcontextTls = {
            .app_netid = TEST_NETID,
            .app_mark = MARK_UNSET,
            .dns_netid = TEST_NETID,
            .dns_mark = MARK_UNSET,
            .uid = NET_CONTEXT_INVALID_UID,
            // Set TLS flags. See also maybeFixupNetContext() in
            // packages/modules/DnsResolver/DnsProxyListener.cpp.
            .flags = NET_CONTEXT_FLAG_USE_DNS_OVER_TLS | NET_CONTEXT_FLAG_USE_EDNS,
    };
};
class ResolvGetAddrInfo : public TestBase {};

// Fixture tests.
TEST_F(ResolvGetAddrInfo, RemovePacketMapping) {
    test::DNSResponder dns(test::DNSResponder::MappingType::BINARY_PACKET);
    ASSERT_TRUE(dns.startServer());
    ASSERT_NO_FATAL_FAILURE(SetResolvers());

    dns.addMappingBinaryPacket(kHelloExampleComQueryV4, kHelloExampleComResponseV4);

    addrinfo* res = nullptr;
    const addrinfo hints = {.ai_family = AF_INET};
    NetworkDnsEventReported event;
    int rv = resolv_getaddrinfo(kHelloExampleCom, nullptr, &hints, &kNetcontext, &res, &event);
    ScopedAddrinfo result(res);
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(rv, 0);
    EXPECT_EQ(ToString(result), kHelloExampleComAddrV4);

    // Remove existing DNS record.
    dns.removeMappingBinaryPacket(kHelloExampleComQueryV4);

    // Expect to have no answer in DNS query result.
    rv = resolv_getaddrinfo(kHelloExampleCom, nullptr, &hints, &kNetcontext, &res, &event);
    result.reset(res);
    ASSERT_EQ(result, nullptr);
    ASSERT_EQ(rv, EAI_NODATA);
}

TEST_F(ResolvGetAddrInfo, ReplacePacketMapping) {
    test::DNSResponder dns(test::DNSResponder::MappingType::BINARY_PACKET);
    ASSERT_TRUE(dns.startServer());
    ASSERT_NO_FATAL_FAILURE(SetResolvers());

    // Register the record which uses IPv4 address 1.2.3.4.
    dns.addMappingBinaryPacket(kHelloExampleComQueryV4, kHelloExampleComResponseV4);

    // Expect that the DNS query returns IPv4 address 1.2.3.4.
    addrinfo* res = nullptr;
    const addrinfo hints = {.ai_family = AF_INET};
    NetworkDnsEventReported event;
    int rv = resolv_getaddrinfo(kHelloExampleCom, nullptr, &hints, &kNetcontext, &res, &event);
    ScopedAddrinfo result(res);
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(rv, 0);
    EXPECT_EQ(ToString(result), "1.2.3.4");

    // Replace the registered record with a record which uses new IPv4 address 5.6.7.8.
    std::vector<uint8_t> newHelloExampleComResponseV4 = {
            /* Header */
            0x00, 0x00, /* Transaction ID: 0x0000 */
            0x81, 0x80, /* Flags: qr rd ra */
            0x00, 0x01, /* Questions: 1 */
            0x00, 0x01, /* Answer RRs: 1 */
            0x00, 0x00, /* Authority RRs: 0 */
            0x00, 0x00, /* Additional RRs: 0 */
            /* Queries */
            0x05, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x07, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65,
            0x03, 0x63, 0x6f, 0x6d, 0x00, /* Name: hello.example.com */
            0x00, 0x01,                   /* Type: A */
            0x00, 0x01,                   /* Class: IN */
            /* Answers */
            0x05, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x07, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65,
            0x03, 0x63, 0x6f, 0x6d, 0x00, /* Name: hello.example.com */
            0x00, 0x01,                   /* Type: A */
            0x00, 0x01,                   /* Class: IN */
            0x00, 0x00, 0x00, 0x00,       /* Time to live: 0 */
            0x00, 0x04,                   /* Data length: 4 */
            0x05, 0x06, 0x07, 0x08        /* Address: 5.6.7.8 */
    };
    dns.addMappingBinaryPacket(kHelloExampleComQueryV4, newHelloExampleComResponseV4);

    // Expect that DNS query returns new IPv4 address 5.6.7.8.
    rv = resolv_getaddrinfo(kHelloExampleCom, nullptr, &hints, &kNetcontext, &res, &event);
    result.reset(res);
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(rv, 0);
    EXPECT_EQ(ToString(result), "5.6.7.8");
}

TEST_F(ResolvGetAddrInfo, BasicTlsQuery) {
    test::DNSResponder dns;
    dns.addMapping(kHelloExampleCom, ns_type::ns_t_a, kHelloExampleComAddrV4);
    dns.addMapping(kHelloExampleCom, ns_type::ns_t_aaaa, kHelloExampleComAddrV6);
    ASSERT_TRUE(dns.startServer());

    test::DnsTlsFrontend tls;
    ASSERT_TRUE(tls.startServer());
    ASSERT_NO_FATAL_FAILURE(SetResolversWithTls());
    EXPECT_TRUE(WaitForPrivateDnsValidation(tls.listen_address()));

    dns.clearQueries();
    addrinfo* res = nullptr;
    // If the socket type is not specified, every address will appear twice, once for
    // SOCK_STREAM and one for SOCK_DGRAM. Just pick one because the addresses for
    // the second query of different socket type are responded by the cache.
    const addrinfo hints = {.ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM};
    NetworkDnsEventReported event;
    const int rv =
            resolv_getaddrinfo(kHelloExampleCom, nullptr, &hints, &kNetcontextTls, &res, &event);
    ScopedAddrinfo result(res);
    ASSERT_EQ(rv, 0);
    EXPECT_EQ(GetNumQueries(dns, kHelloExampleCom), 2U);
    const std::vector<std::string> result_strs = ToStrings(result);
    EXPECT_THAT(result_strs, testing::UnorderedElementsAreArray(
                                     {kHelloExampleComAddrV4, kHelloExampleComAddrV6}));
    EXPECT_TRUE(tls.waitForQueries(3));
}

// Parameterized test class definition.
using GoldTestParamType = std::tuple<DnsProtocol, std::string /* filename */>;
class ResolvGoldTest : public TestBase, public ::testing::WithParamInterface<GoldTestParamType> {
  public:
    // Generate readable string for test name from test parameters.
    static std::string Name(const ::testing::TestParamInfo<GoldTestParamType>& info) {
        const auto& [protocol, file] = info.param;
        std::string name = StringPrintf(
                "%s_%s", protocol == DnsProtocol::CLEARTEXT ? "CLEARTEXT" : "TLS", file.c_str());
        std::replace_if(
                std::begin(name), std::end(name), [](char ch) { return !std::isalnum(ch); }, '_');
        return name;
    }
};

// GetAddrInfo tests.
INSTANTIATE_TEST_SUITE_P(GetAddrInfo, ResolvGoldTest,
                         ::testing::Combine(::testing::Values(DnsProtocol::CLEARTEXT),
                                            ::testing::ValuesIn(kGoldFilesGetAddrInfo)),
                         ResolvGoldTest::Name);
INSTANTIATE_TEST_SUITE_P(GetAddrInfoTls, ResolvGoldTest,
                         ::testing::Combine(::testing::Values(DnsProtocol::TLS),
                                            ::testing::ValuesIn(kGoldFilesGetAddrInfoTls)),
                         ResolvGoldTest::Name);

// GetHostByName tests.
INSTANTIATE_TEST_SUITE_P(GetHostByName, ResolvGoldTest,
                         ::testing::Combine(::testing::Values(DnsProtocol::CLEARTEXT),
                                            ::testing::ValuesIn(kGoldFilesGetHostByName)),
                         ResolvGoldTest::Name);
INSTANTIATE_TEST_SUITE_P(GetHostByNameTls, ResolvGoldTest,
                         ::testing::Combine(::testing::Values(DnsProtocol::TLS),
                                            ::testing::ValuesIn(kGoldFilesGetHostByNameTls)),
                         ResolvGoldTest::Name);

TEST_P(ResolvGoldTest, GoldData) {
    const auto& [protocol, file] = GetParam();

    // Setup DNS server configuration.
    test::DNSResponder dns(test::DNSResponder::MappingType::BINARY_PACKET);
    ASSERT_TRUE(dns.startServer());
    test::DnsTlsFrontend tls;

    if (protocol == DnsProtocol::CLEARTEXT) {
        ASSERT_NO_FATAL_FAILURE(SetResolvers());
    } else if (protocol == DnsProtocol::TLS) {
        ASSERT_TRUE(tls.startServer());
        ASSERT_NO_FATAL_FAILURE(SetResolversWithTls());
        EXPECT_TRUE(WaitForPrivateDnsValidation(tls.listen_address()));
        tls.clearQueries();
    }

    // Read test configuration from serialized binary to proto.
    const Result<GoldTest> result = ToProto(file);
    ASSERT_TRUE(result.ok()) << result.error().message();
    const GoldTest& goldtest = result.value();

    // Register packet mappings (query, response) from proto.
    SetupMappings(goldtest, dns);

    // Verify the resolver by proto.
    VerifyResolver(goldtest, dns, tls, protocol);
}

}  // namespace android::net
