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
 */

#define LOG_TAG "resolv_callback_unit_test"

#include <android-base/properties.h>
#include <gtest/gtest.h>

#include "DnsResolver.h"
#include "getaddrinfo.h"
#include "resolv_cache.h"
#include "tests/resolv_test_utils.h"

namespace android::net {

using android::net::NetworkDnsEventReported;
using android::netdutils::ScopedAddrinfo;

// Use maximum reserved appId for applications to avoid conflict with existing uids.
const int TEST_UID = 99999;
// Use testUid to make sure TagSocketCallback is called.
static int testUid = 0;

bool isApiLevelSupported(size_t requiredVersion) {
    if (android::base::GetUintProperty<uint64_t>("ro.build.version.sdk", 0) >= requiredVersion)
        return true;
    else
        return false;
}

#define SKIP_IF_APILEVEL_LESS_THAN(version)                                          \
    do {                                                                             \
        if (!isApiLevelSupported(version)) {                                         \
            GTEST_LOG_(INFO) << "Skip. Required API version: " << (version) << "\n"; \
            return;                                                                  \
        }                                                                            \
    } while (0)

void getNetworkContextCallback(uint32_t, uint32_t, android_net_context*) {
    // No-op
}

bool checkCallingPermissionCallback(const char*) {
    // No-op
    return true;
}

void logCallback(const char*) {
    // No-op
}

int tagSocketCallback(int, uint32_t, uid_t uid, pid_t) {
    testUid = uid;
    return true;
}

bool evaluateDomainNameCallback(const android_net_context&, const char*) {
    // No-op
    return true;
}

void initDnsResolverCallbacks() {
    ResolverNetdCallbacks callbacks = {
            .check_calling_permission = &checkCallingPermissionCallback,
            .get_network_context = &getNetworkContextCallback,
            .log = &logCallback,
            .tagSocket = &tagSocketCallback,
            .evaluate_domain_name = &evaluateDomainNameCallback,
    };
    // It returns fail since socket 'dnsproxyd' has been occupied.
    // But the callback funtions is configured successfully and can
    // be tested when running unit test cases.
    resolv_init(&callbacks);
}

void resetDnsResolverCallbacks() {
    ResolverNetdCallbacks callbacks = {
            .check_calling_permission = nullptr,
            .get_network_context = nullptr,
            .log = nullptr,
            .tagSocket = nullptr,
            .evaluate_domain_name = nullptr,
    };
    resolv_init(&callbacks);
}

void resetCallbackParams() {
    testUid = 0;
}

class CallbackTest : public ::testing::Test {
  protected:
    void SetUp() override {
        initDnsResolverCallbacks();
        // Create cache for test
        android::net::gDnsResolv->resolverCtrl.createNetworkCache(TEST_NETID);
    }

    void TearDown() override {
        // Reset related parameters and callback functions.
        resetCallbackParams();
        resetDnsResolverCallbacks();
        // Delete cache for test
        android::net::gDnsResolv->resolverCtrl.destroyNetworkCache(TEST_NETID);
    }

    int SetResolvers() {
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
        return resolv_set_nameservers(TEST_NETID, servers, domains, params);
    }

    const android_net_context mNetcontext = {
            .app_netid = TEST_NETID,
            .app_mark = MARK_UNSET,
            .dns_netid = TEST_NETID,
            .dns_mark = MARK_UNSET,
            .uid = TEST_UID,
    };
};

TEST_F(CallbackTest, tagSocketCallback) {
    // tagSocketCallback is used when ro.build.version.sdk >=30.
    // If ro.build.version.sdk < 30, don't run this test case.
    SKIP_IF_APILEVEL_LESS_THAN(30);
    test::DNSResponder dns;
    dns.addMapping(kHelloExampleCom, ns_type::ns_t_a, kHelloExampleComAddrV4);
    ASSERT_TRUE(dns.startServer());
    EXPECT_EQ(SetResolvers(), 0);

    addrinfo* result = nullptr;
    const addrinfo hints = {.ai_family = AF_INET};
    NetworkDnsEventReported event;
    // tagSocketCallback will be called.
    const int rv = resolv_getaddrinfo("hello", nullptr, &hints, &mNetcontext, &result, &event);
    ScopedAddrinfo result_cleanup(result);
    EXPECT_EQ(testUid, TEST_UID);
    EXPECT_EQ(rv, 0);
}
}  // end of namespace android::net
