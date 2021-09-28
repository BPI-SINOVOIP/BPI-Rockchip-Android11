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

#define LOG_TAG "resolv_stress_test"

#include <chrono>
#include <thread>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "ResolverStats.h"
#include "dns_responder/dns_responder_client_ndk.h"
#include "params.h"  // MAX_NS
#include "resolv_test_utils.h"

using android::base::StringPrintf;
using android::net::ResolverStats;

class ResolverStressTest : public ::testing::Test {
  public:
    ResolverStressTest() { mDnsClient.SetUp(); }
    ~ResolverStressTest() { mDnsClient.TearDown(); }

  protected:
    void RunGetAddrInfoStressTest(unsigned num_hosts, unsigned num_threads,
                                         unsigned num_queries) {
        std::vector<std::string> domains = {"example.com"};
        std::vector<std::unique_ptr<test::DNSResponder>> dns;
        std::vector<std::string> servers;
        std::vector<DnsResponderClient::DnsResponderClient::Mapping> mappings;
        ASSERT_NO_FATAL_FAILURE(mDnsClient.SetupMappings(num_hosts, domains, &mappings));
        ASSERT_NO_FATAL_FAILURE(mDnsClient.SetupDNSServers(MAXNS, mappings, &dns, &servers));

        ASSERT_TRUE(mDnsClient.SetResolversForNetwork(servers, domains, kDefaultParams));

        auto t0 = std::chrono::steady_clock::now();
        std::vector<std::thread> threads(num_threads);
        for (std::thread& thread : threads) {
            thread = std::thread([&mappings, num_queries]() {
                for (unsigned i = 0; i < num_queries; ++i) {
                    uint32_t ofs = arc4random_uniform(mappings.size());
                    auto& mapping = mappings[ofs];
                    addrinfo* result = nullptr;
                    int rv = getaddrinfo(mapping.host.c_str(), nullptr, nullptr, &result);
                    EXPECT_EQ(0, rv) << "error [" << rv << "] " << gai_strerror(rv);
                    if (rv == 0) {
                        std::string result_str = ToString(result);
                        EXPECT_TRUE(result_str == mapping.ip4 || result_str == mapping.ip6)
                                << "result='" << result_str << "', ip4='" << mapping.ip4
                                << "', ip6='" << mapping.ip6;
                    }
                    if (result) {
                        freeaddrinfo(result);
                        result = nullptr;
                    }
                }
            });
        }

        for (std::thread& thread : threads) {
            thread.join();
        }
        auto t1 = std::chrono::steady_clock::now();
        LOG(INFO) << StringPrintf("%u hosts, %u threads, %u queries, %Es", num_hosts, num_threads,
                                  num_queries, std::chrono::duration<double>(t1 - t0).count());

        std::vector<std::string> res_servers;
        std::vector<std::string> res_domains;
        std::vector<std::string> res_tls_servers;
        res_params res_params;
        std::vector<ResolverStats> res_stats;
        int wait_for_pending_req_timeout_count;
        ASSERT_TRUE(DnsResponderClient::GetResolverInfo(
                mDnsClient.resolvService(), TEST_NETID, &res_servers, &res_domains,
                &res_tls_servers, &res_params, &res_stats, &wait_for_pending_req_timeout_count));
        EXPECT_EQ(0, wait_for_pending_req_timeout_count);
    }

    DnsResponderClient mDnsClient;
};

TEST_F(ResolverStressTest, GetAddrInfoStressTest_100) {
    const unsigned num_hosts = 100;
    const unsigned num_threads = 100;
    const unsigned num_queries = 100;
    ASSERT_NO_FATAL_FAILURE(RunGetAddrInfoStressTest(num_hosts, num_threads, num_queries));
}

TEST_F(ResolverStressTest, GetAddrInfoStressTest_100000) {
    const unsigned num_hosts = 100000;
    const unsigned num_threads = 100;
    const unsigned num_queries = 100;
    ASSERT_NO_FATAL_FAILURE(RunGetAddrInfoStressTest(num_hosts, num_threads, num_queries));
}