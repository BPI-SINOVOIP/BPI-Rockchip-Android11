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

#include <netdb.h>

#include <array>
#include <atomic>
#include <chrono>
#include <ctime>
#include <thread>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <android/multinetwork.h>
#include <arpa/inet.h>
#include <cutils/properties.h>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "res_init.h"
#include "resolv_cache.h"
#include "resolv_private.h"
#include "stats.h"
#include "tests/dns_responder/dns_responder.h"

using namespace std::chrono_literals;

using android::netdutils::IPSockAddr;

constexpr int TEST_NETID = 30;
constexpr int TEST_NETID_2 = 31;
constexpr int DNS_PORT = 53;

// Constant values sync'd from res_cache.cpp
constexpr int DNS_HEADER_SIZE = 12;
constexpr int MAX_ENTRIES = 64 * 2 * 5;

namespace {

struct CacheEntry {
    std::vector<char> query;
    std::vector<char> answer;
};

struct SetupParams {
    std::vector<std::string> servers;
    std::vector<std::string> domains;
    res_params params;
    aidl::android::net::ResolverOptionsParcel resolverOptions;
    std::vector<int32_t> transportTypes;
};

struct CacheStats {
    SetupParams setup;
    std::vector<res_stats> stats;
    int pendingReqTimeoutCount;
};

std::vector<char> makeQuery(int op, const char* qname, int qclass, int qtype) {
    uint8_t buf[MAXPACKET] = {};
    const int len = res_nmkquery(op, qname, qclass, qtype, /*data=*/nullptr, /*datalen=*/0, buf,
                                 sizeof(buf),
                                 /*netcontext_flags=*/0);
    return std::vector<char>(buf, buf + len);
}

std::vector<char> makeAnswer(const std::vector<char>& query, const char* rdata_str,
                             const unsigned ttl) {
    test::DNSHeader header;
    header.read(query.data(), query.data() + query.size());

    for (const test::DNSQuestion& question : header.questions) {
        std::string rname(question.qname.name);
        test::DNSRecord record{
                .name = {.name = question.qname.name},
                .rtype = question.qtype,
                .rclass = question.qclass,
                .ttl = ttl,
        };
        test::DNSResponder::fillRdata(rdata_str, record);
        header.answers.push_back(std::move(record));
    }

    char answer[MAXPACKET] = {};
    char* answer_end = header.write(answer, answer + sizeof(answer));
    return std::vector<char>(answer, answer_end);
}

// Get the current time in unix timestamp since the Epoch.
time_t currentTime() {
    return std::time(nullptr);
}

// Comparison for res_sample.
bool operator==(const res_sample& a, const res_sample& b) {
    return std::tie(a.at, a.rtt, a.rcode) == std::tie(b.at, b.rtt, b.rcode);
}

// Comparison for res_stats.
bool operator==(const res_stats& a, const res_stats& b) {
    if (std::tie(a.sample_count, a.sample_next) != std::tie(b.sample_count, b.sample_next)) {
        return false;
    }
    for (int i = 0; i < a.sample_count; i++) {
        if (a.samples[i] != b.samples[i]) return false;
    }
    return true;
}

// Comparison for res_params.
bool operator==(const res_params& a, const res_params& b) {
    return std::tie(a.sample_validity, a.success_threshold, a.min_samples, a.max_samples,
                    a.base_timeout_msec, a.retry_count) ==
           std::tie(b.sample_validity, b.success_threshold, b.min_samples, b.max_samples,
                    b.base_timeout_msec, b.retry_count);
}

}  // namespace

class ResolvCacheTest : public ::testing::Test {
  protected:
    static constexpr res_params kParams = {
            .sample_validity = 300,
            .success_threshold = 25,
            .min_samples = 8,
            .max_samples = 8,
            .base_timeout_msec = 1000,
            .retry_count = 2,
    };

    ResolvCacheTest() {
        // Store the default one and conceal 10000+ lines of resolver cache logs.
        defaultLogSeverity = android::base::SetMinimumLogSeverity(
                static_cast<android::base::LogSeverity>(android::base::WARNING));
    }
    ~ResolvCacheTest() {
        cacheDelete(TEST_NETID);
        cacheDelete(TEST_NETID_2);

        // Restore the log severity.
        android::base::SetMinimumLogSeverity(defaultLogSeverity);
    }

    [[nodiscard]] bool cacheLookup(ResolvCacheStatus expectedCacheStatus, uint32_t netId,
                                   const CacheEntry& ce, uint32_t flags = 0) {
        int anslen = 0;
        std::vector<char> answer(MAXPACKET);
        const auto cacheStatus = resolv_cache_lookup(netId, ce.query.data(), ce.query.size(),
                                                     answer.data(), answer.size(), &anslen, flags);
        if (cacheStatus != expectedCacheStatus) {
            ADD_FAILURE() << "cacheStatus: expected = " << expectedCacheStatus
                          << ", actual =" << cacheStatus;
            return false;
        }

        if (cacheStatus == RESOLV_CACHE_FOUND) {
            answer.resize(anslen);
            if (answer != ce.answer) {
                ADD_FAILURE() << "The answer from the cache is not as expected.";
                return false;
            }
        }
        return true;
    }

    int cacheCreate(uint32_t netId) {
        return resolv_create_cache_for_net(netId);
    }

    void cacheDelete(uint32_t netId) {
        resolv_delete_cache_for_net(netId);
    }

    int cacheAdd(uint32_t netId, const CacheEntry& ce) {
        return resolv_cache_add(netId, ce.query.data(), ce.query.size(), ce.answer.data(),
                                ce.answer.size());
    }

    int cacheAdd(uint32_t netId, const std::vector<char>& query, const std::vector<char>& answer) {
        return resolv_cache_add(netId, query.data(), query.size(), answer.data(), answer.size());
    }

    int cacheGetExpiration(uint32_t netId, const std::vector<char>& query, time_t* expiration) {
        return resolv_cache_get_expiration(netId, query, expiration);
    }

    void cacheQueryFailed(uint32_t netId, const CacheEntry& ce, uint32_t flags) {
        _resolv_cache_query_failed(netId, ce.query.data(), ce.query.size(), flags);
    }

    int cacheSetupResolver(uint32_t netId, const SetupParams& setup) {
        return resolv_set_nameservers(netId, setup.servers, setup.domains, setup.params,
                                      setup.resolverOptions, setup.transportTypes);
    }

    void cacheAddStats(uint32_t netId, int revision_id, const IPSockAddr& ipsa,
                       const res_sample& sample, int max_samples) {
        resolv_cache_add_resolver_stats_sample(netId, revision_id, ipsa, sample, max_samples);
    }

    int cacheFlush(uint32_t netId) { return resolv_flush_cache_for_net(netId); }

    void expectCacheStats(const std::string& msg, uint32_t netId, const CacheStats& expected) {
        int nscount = -1;
        sockaddr_storage servers[MAXNS];
        int dcount = -1;
        char domains[MAXDNSRCH][MAXDNSRCHPATH];
        res_stats stats[MAXNS];
        res_params params = {};
        int res_wait_for_pending_req_timeout_count;
        android_net_res_stats_get_info_for_net(netId, &nscount, servers, &dcount, domains, &params,
                                               stats, &res_wait_for_pending_req_timeout_count);

        // Server checking.
        EXPECT_EQ(nscount, static_cast<int>(expected.setup.servers.size())) << msg;
        for (int i = 0; i < nscount; i++) {
            EXPECT_EQ(addrToString(&servers[i]), expected.setup.servers[i]) << msg;
        }

        // Domain checking
        EXPECT_EQ(dcount, static_cast<int>(expected.setup.domains.size())) << msg;
        for (int i = 0; i < dcount; i++) {
            EXPECT_EQ(std::string(domains[i]), expected.setup.domains[i]) << msg;
        }

        // res_params checking.
        EXPECT_TRUE(params == expected.setup.params) << msg;

        // res_stats checking.
        if (expected.stats.size() == 0) {
            for (int ns = 0; ns < nscount; ns++) {
                EXPECT_EQ(0U, stats[ns].sample_count) << msg;
            }
        }
        for (size_t i = 0; i < expected.stats.size(); i++) {
            EXPECT_TRUE(stats[i] == expected.stats[i]) << msg;
        }

        // wait_for_pending_req_timeout_count checking.
        EXPECT_EQ(res_wait_for_pending_req_timeout_count, expected.pendingReqTimeoutCount) << msg;
    }

    CacheEntry makeCacheEntry(int op, const char* qname, int qclass, int qtype, const char* rdata,
                              std::chrono::seconds ttl = 10s) {
        CacheEntry ce;
        ce.query = makeQuery(op, qname, qclass, qtype);
        ce.answer = makeAnswer(ce.query, rdata, static_cast<unsigned>(ttl.count()));
        return ce;
    }

  private:
    android::base::LogSeverity defaultLogSeverity;
};

TEST_F(ResolvCacheTest, CreateAndDeleteCache) {
    // Create the cache for network 1.
    EXPECT_EQ(0, cacheCreate(TEST_NETID));
    EXPECT_EQ(-EEXIST, cacheCreate(TEST_NETID));
    EXPECT_TRUE(has_named_cache(TEST_NETID));

    // Create the cache for network 2.
    EXPECT_EQ(0, cacheCreate(TEST_NETID_2));
    EXPECT_EQ(-EEXIST, cacheCreate(TEST_NETID_2));
    EXPECT_TRUE(has_named_cache(TEST_NETID_2));

    // Delete the cache in network 1.
    cacheDelete(TEST_NETID);
    EXPECT_FALSE(has_named_cache(TEST_NETID));
    EXPECT_TRUE(has_named_cache(TEST_NETID_2));
}

// Missing checks for the argument 'answer'.
TEST_F(ResolvCacheTest, CacheAdd_InvalidArgs) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));

    const std::vector<char> queryEmpty(MAXPACKET, 0);
    const std::vector<char> queryTooSmall(DNS_HEADER_SIZE - 1, 0);
    CacheEntry ce = makeCacheEntry(QUERY, "valid.cache", ns_c_in, ns_t_a, "1.2.3.4");

    EXPECT_EQ(-EINVAL, cacheAdd(TEST_NETID, queryEmpty, ce.answer));
    EXPECT_EQ(-EINVAL, cacheAdd(TEST_NETID, queryTooSmall, ce.answer));

    // Cache not existent in TEST_NETID_2.
    EXPECT_EQ(-ENONET, cacheAdd(TEST_NETID_2, ce));
}

TEST_F(ResolvCacheTest, CacheAdd_DuplicateEntry) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));
    CacheEntry ce = makeCacheEntry(QUERY, "existent.in.cache", ns_c_in, ns_t_a, "1.2.3.4");
    time_t now = currentTime();

    // Add the cache entry.
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ce));
    EXPECT_EQ(0, cacheAdd(TEST_NETID, ce));
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_FOUND, TEST_NETID, ce));

    // Get the expiration time and verify its value is greater than now.
    time_t expiration1;
    EXPECT_EQ(0, cacheGetExpiration(TEST_NETID, ce.query, &expiration1));
    EXPECT_GT(expiration1, now);

    // Adding the duplicate entry will return an error, and the expiration time won't be modified.
    EXPECT_EQ(-EEXIST, cacheAdd(TEST_NETID, ce));
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_FOUND, TEST_NETID, ce));
    time_t expiration2;
    EXPECT_EQ(0, cacheGetExpiration(TEST_NETID, ce.query, &expiration2));
    EXPECT_EQ(expiration1, expiration2);
}

TEST_F(ResolvCacheTest, CacheLookup) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));
    EXPECT_EQ(0, cacheCreate(TEST_NETID_2));
    CacheEntry ce = makeCacheEntry(QUERY, "existent.in.cache", ns_c_in, ns_t_a, "1.2.3.4");

    // Cache found in network 1.
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ce));
    EXPECT_EQ(0, cacheAdd(TEST_NETID, ce));
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_FOUND, TEST_NETID, ce));

    // No cache found in network 2.
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID_2, ce));

    ce = makeCacheEntry(QUERY, "existent.in.cache", ns_c_in, ns_t_aaaa, "2001:db8::1.2.3.4");

    // type A and AAAA are independent.
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ce));
    EXPECT_EQ(0, cacheAdd(TEST_NETID, ce));
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_FOUND, TEST_NETID, ce));
}

TEST_F(ResolvCacheTest, CacheLookup_CacheFlags) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));
    std::vector<char> answerFromCache;
    CacheEntry ce = makeCacheEntry(QUERY, "existent.in.cache", ns_c_in, ns_t_a, "1.2.3.4");

    // The entry can't be found when only no-cache-lookup bit is carried.
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ce, ANDROID_RESOLV_NO_CACHE_LOOKUP));

    // Ensure RESOLV_CACHE_SKIP is returned when there's no such the same entry in the cache.
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_SKIP, TEST_NETID, ce, ANDROID_RESOLV_NO_CACHE_STORE));

    // Skip the cache lookup if no-cache-lookup and no-cache-store bits are carried
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_SKIP, TEST_NETID, ce,
                            ANDROID_RESOLV_NO_CACHE_LOOKUP | ANDROID_RESOLV_NO_CACHE_STORE));

    // Add the cache entry.
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ce));
    EXPECT_EQ(0, cacheAdd(TEST_NETID, ce));
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_FOUND, TEST_NETID, ce));

    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ce, ANDROID_RESOLV_NO_CACHE_LOOKUP));

    // Now no-cache-store has no effect if a same entry is existent in the cache.
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_SKIP, TEST_NETID, ce, ANDROID_RESOLV_NO_CACHE_STORE));

    // Skip the cache lookup again regardless of a same entry being already in the cache.
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_SKIP, TEST_NETID, ce,
                            ANDROID_RESOLV_NO_CACHE_LOOKUP | ANDROID_RESOLV_NO_CACHE_STORE));
}

TEST_F(ResolvCacheTest, CacheLookup_Types) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));
    static const struct QueryTypes {
        int type;
        std::string rdata;
    } Types[] = {
            {ns_t_a, "1.2.3.4"},
            {ns_t_aaaa, "2001:db8::1.2.3.4"},
            {ns_t_ptr, "4.3.2.1.in-addr.arpa."},
            {ns_t_ptr, "4.0.3.0.2.0.1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.b.d.0.1.0.0.2.ip6.arpa."},
    };

    for (const auto& t : Types) {
        std::string name = android::base::StringPrintf("cache.lookup.type.%s", t.rdata.c_str());
        SCOPED_TRACE(name);

        CacheEntry ce = makeCacheEntry(QUERY, name.data(), ns_c_in, t.type, t.rdata.data());
        EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ce));
        EXPECT_EQ(0, cacheAdd(TEST_NETID, ce));
        EXPECT_TRUE(cacheLookup(RESOLV_CACHE_FOUND, TEST_NETID, ce));
    }
}

TEST_F(ResolvCacheTest, CacheLookup_InvalidArgs) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));

    const std::vector<char> queryEmpty(MAXPACKET, 0);
    const std::vector<char> queryTooSmall(DNS_HEADER_SIZE - 1, 0);
    std::vector<char> answerTooSmall(DNS_HEADER_SIZE - 1, 0);
    const CacheEntry ce = makeCacheEntry(QUERY, "valid.cache", ns_c_in, ns_t_a, "1.2.3.4");
    auto cacheLookupFn = [](const std::vector<char>& query,
                            std::vector<char> answer) -> ResolvCacheStatus {
        int anslen = 0;
        return resolv_cache_lookup(TEST_NETID, query.data(), query.size(), answer.data(),
                                   answer.size(), &anslen, 0);
    };

    EXPECT_EQ(0, cacheAdd(TEST_NETID, ce));

    EXPECT_EQ(RESOLV_CACHE_UNSUPPORTED, cacheLookupFn(queryEmpty, ce.answer));
    EXPECT_EQ(RESOLV_CACHE_UNSUPPORTED, cacheLookupFn(queryTooSmall, ce.answer));
    EXPECT_EQ(RESOLV_CACHE_UNSUPPORTED, cacheLookupFn(ce.query, answerTooSmall));

    // It can actually be found with valid arguments.
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_FOUND, TEST_NETID, ce));

    // Cache not existent in TEST_NETID_2.
    EXPECT_EQ(-ENONET, cacheAdd(TEST_NETID_2, ce));
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_UNSUPPORTED, TEST_NETID_2, ce));
}

TEST_F(ResolvCacheTest, CacheLookup_Expired) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));

    // An entry with zero ttl won't be stored in the cache.
    CacheEntry ce = makeCacheEntry(QUERY, "expired.in.0s", ns_c_in, ns_t_a, "1.2.3.4", 0s);
    EXPECT_EQ(0, cacheAdd(TEST_NETID, ce));
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ce));

    // Create an entry expired in 1s.
    ce = makeCacheEntry(QUERY, "expired.in.1s", ns_c_in, ns_t_a, "1.2.3.4", 1s);
    EXPECT_EQ(0, cacheAdd(TEST_NETID, ce));

    // Cache found.
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_FOUND, TEST_NETID, ce));
    time_t expiration;
    EXPECT_EQ(0, cacheGetExpiration(TEST_NETID, ce.query, &expiration));

    // Wait for the cache expired.
    std::this_thread::sleep_for(1500ms);
    EXPECT_GE(currentTime(), expiration);
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ce));
}

TEST_F(ResolvCacheTest, PendingRequest_QueryDeferred) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));
    EXPECT_EQ(0, cacheCreate(TEST_NETID_2));

    CacheEntry ce = makeCacheEntry(QUERY, "query.deferred", ns_c_in, ns_t_a, "1.2.3.4");
    std::atomic_bool done(false);

    // This is the first lookup. The following lookups from other threads will be in the
    // pending request list.
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ce));

    std::vector<std::thread> threads(5);
    for (std::thread& thread : threads) {
        thread = std::thread([&]() {
            EXPECT_TRUE(cacheLookup(RESOLV_CACHE_FOUND, TEST_NETID, ce));

            // Ensure this thread gets stuck in lookups before we wake it.
            EXPECT_TRUE(done);
        });
    }

    // Wait for a while for the threads performing lookups.
    // TODO: Perhaps implement a test-only function to get the number of pending requests
    // instead of sleep.
    std::this_thread::sleep_for(100ms);

    // The threads keep waiting regardless of any other networks or even if cache flag is set.
    EXPECT_EQ(0, cacheAdd(TEST_NETID_2, ce));
    cacheQueryFailed(TEST_NETID, ce, ANDROID_RESOLV_NO_CACHE_STORE);
    cacheQueryFailed(TEST_NETID, ce, ANDROID_RESOLV_NO_CACHE_LOOKUP);
    cacheQueryFailed(TEST_NETID_2, ce, ANDROID_RESOLV_NO_CACHE_STORE);
    cacheQueryFailed(TEST_NETID_2, ce, ANDROID_RESOLV_NO_CACHE_LOOKUP);
    cacheDelete(TEST_NETID_2);

    // Ensure none of the threads has finished the lookups.
    std::this_thread::sleep_for(100ms);

    // Wake up the threads
    done = true;
    EXPECT_EQ(0, cacheAdd(TEST_NETID, ce));

    for (std::thread& thread : threads) {
        thread.join();
    }
}

TEST_F(ResolvCacheTest, PendingRequest_QueryFailed) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));

    CacheEntry ce = makeCacheEntry(QUERY, "query.failed", ns_c_in, ns_t_a, "1.2.3.4");
    std::atomic_bool done(false);

    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ce));

    std::vector<std::thread> threads(5);
    for (std::thread& thread : threads) {
        thread = std::thread([&]() {
            EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ce));

            // Ensure this thread gets stuck in lookups before we wake it.
            EXPECT_TRUE(done);
        });
    }

    // Wait for a while for the threads performing lookups.
    std::this_thread::sleep_for(100ms);

    // Wake up the threads
    done = true;
    cacheQueryFailed(TEST_NETID, ce, 0);

    for (std::thread& thread : threads) {
        thread.join();
    }
}

TEST_F(ResolvCacheTest, PendingRequest_CacheDestroyed) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));
    EXPECT_EQ(0, cacheCreate(TEST_NETID_2));

    CacheEntry ce = makeCacheEntry(QUERY, "query.failed", ns_c_in, ns_t_a, "1.2.3.4");
    std::atomic_bool done(false);

    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ce));

    std::vector<std::thread> threads(5);
    for (std::thread& thread : threads) {
        thread = std::thread([&]() {
            EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ce));

            // Ensure this thread gets stuck in lookups before we wake it.
            EXPECT_TRUE(done);
        });
    }

    // Wait for a while for the threads performing lookups.
    std::this_thread::sleep_for(100ms);

    // Deleting another network must not cause the threads to wake up.
    cacheDelete(TEST_NETID_2);

    // Ensure none of the threads has finished the lookups.
    std::this_thread::sleep_for(100ms);

    // Wake up the threads
    done = true;
    cacheDelete(TEST_NETID);

    for (std::thread& thread : threads) {
        thread.join();
    }
}

TEST_F(ResolvCacheTest, MaxEntries) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));
    std::vector<CacheEntry> ces;

    for (int i = 0; i < 2 * MAX_ENTRIES; i++) {
        std::string qname = android::base::StringPrintf("cache.%04d", i);
        SCOPED_TRACE(qname);
        CacheEntry ce = makeCacheEntry(QUERY, qname.data(), ns_c_in, ns_t_a, "1.2.3.4");
        EXPECT_EQ(0, cacheAdd(TEST_NETID, ce));
        EXPECT_TRUE(cacheLookup(RESOLV_CACHE_FOUND, TEST_NETID, ce));
        ces.emplace_back(ce);
    }

    for (int i = 0; i < 2 * MAX_ENTRIES; i++) {
        std::string qname = android::base::StringPrintf("cache.%04d", i);
        SCOPED_TRACE(qname);
        if (i < MAX_ENTRIES) {
            // Because the cache is LRU, the oldest queries should have been purged,
            // and the most recent MAX_ENTRIES ones should still be present.
            EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ces[i]));
        } else {
            EXPECT_TRUE(cacheLookup(RESOLV_CACHE_FOUND, TEST_NETID, ces[i]));
        }
    }
}

TEST_F(ResolvCacheTest, CacheFull) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));

    CacheEntry ce1 = makeCacheEntry(QUERY, "cache.0000", ns_c_in, ns_t_a, "1.2.3.4", 100s);
    EXPECT_EQ(0, cacheAdd(TEST_NETID, ce1));
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_FOUND, TEST_NETID, ce1));

    CacheEntry ce2 = makeCacheEntry(QUERY, "cache.0001", ns_c_in, ns_t_a, "1.2.3.4", 1s);
    EXPECT_EQ(0, cacheAdd(TEST_NETID, ce2));
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_FOUND, TEST_NETID, ce2));

    // Stuff the resolver cache.
    for (int i = 2; i < MAX_ENTRIES; i++) {
        std::string qname = android::base::StringPrintf("cache.%04d", i);
        SCOPED_TRACE(qname);
        CacheEntry ce = makeCacheEntry(QUERY, qname.data(), ns_c_in, ns_t_a, "1.2.3.4", 50s);
        EXPECT_EQ(0, cacheAdd(TEST_NETID, ce));
        EXPECT_TRUE(cacheLookup(RESOLV_CACHE_FOUND, TEST_NETID, ce));
    }

    // Wait for ce2 expired.
    std::this_thread::sleep_for(1500ms);

    // The cache is full now, and the expired ce2 will be removed first.
    CacheEntry ce3 = makeCacheEntry(QUERY, "cache.overfilled.1", ns_c_in, ns_t_a, "1.2.3.4", 50s);
    EXPECT_EQ(0, cacheAdd(TEST_NETID, ce3));
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_FOUND, TEST_NETID, ce3));
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ce2));

    // The cache is full again but there's no one expired, so the oldest ce1 will be removed.
    CacheEntry ce4 = makeCacheEntry(QUERY, "cache.overfilled.2", ns_c_in, ns_t_a, "1.2.3.4", 50s);
    EXPECT_EQ(0, cacheAdd(TEST_NETID, ce4));
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_FOUND, TEST_NETID, ce4));
    EXPECT_TRUE(cacheLookup(RESOLV_CACHE_NOTFOUND, TEST_NETID, ce1));
}

TEST_F(ResolvCacheTest, ResolverSetup) {
    const SetupParams setup = {
            .servers = {"127.0.0.1", "::127.0.0.2", "fe80::3"},
            .domains = {"domain1.com", "domain2.com"},
            .params = kParams,
    };

    // Failed to setup resolver because of the cache not created.
    EXPECT_EQ(-ENONET, cacheSetupResolver(TEST_NETID, setup));
    EXPECT_FALSE(resolv_has_nameservers(TEST_NETID));

    // The cache is created now.
    EXPECT_EQ(0, cacheCreate(TEST_NETID));
    EXPECT_EQ(0, cacheSetupResolver(TEST_NETID, setup));
    EXPECT_TRUE(resolv_has_nameservers(TEST_NETID));
}

TEST_F(ResolvCacheTest, ResolverSetup_InvalidNameServers) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));
    const std::string invalidServers[]{
            "127.A.b.1",
            "127.^.0",
            "::^:1",
            "",
    };
    SetupParams setup = {
            .servers = {},
            .domains = {"domain1.com"},
            .params = kParams,
    };

    // Failed to setup resolver because of invalid name servers.
    for (const auto& server : invalidServers) {
        SCOPED_TRACE(server);
        setup.servers = {"127.0.0.1", server, "127.0.0.2"};
        EXPECT_EQ(-EINVAL, cacheSetupResolver(TEST_NETID, setup));
        EXPECT_FALSE(resolv_has_nameservers(TEST_NETID));
    }
}

TEST_F(ResolvCacheTest, ResolverSetup_DropDomain) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));

    // Setup with one domain which is too long.
    const std::vector<std::string> servers = {"127.0.0.1", "fe80::1"};
    const std::string domainTooLong(MAXDNSRCHPATH, '1');
    const std::string validDomain1(MAXDNSRCHPATH - 1, '2');
    const std::string validDomain2(MAXDNSRCHPATH - 1, '3');
    SetupParams setup = {
            .servers = servers,
            .domains = {},
            .params = kParams,
    };
    CacheStats expect = {
            .setup = setup,
            .stats = {},
            .pendingReqTimeoutCount = 0,
    };

    // Overlength domains are dropped.
    setup.domains = {validDomain1, domainTooLong, validDomain2};
    expect.setup.domains = {validDomain1, validDomain2};
    EXPECT_EQ(0, cacheSetupResolver(TEST_NETID, setup));
    EXPECT_TRUE(resolv_has_nameservers(TEST_NETID));
    expectCacheStats("ResolverSetup_Domains drop overlength", TEST_NETID, expect);

    // Duplicate domains are dropped.
    setup.domains = {validDomain1, validDomain2, validDomain1, validDomain2};
    expect.setup.domains = {validDomain1, validDomain2};
    EXPECT_EQ(0, cacheSetupResolver(TEST_NETID, setup));
    EXPECT_TRUE(resolv_has_nameservers(TEST_NETID));
    expectCacheStats("ResolverSetup_Domains drop duplicates", TEST_NETID, expect);
}

TEST_F(ResolvCacheTest, ResolverSetup_Prune) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));
    const std::vector<std::string> servers = {"127.0.0.1", "::127.0.0.2", "fe80::1", "fe80::2",
                                              "fe80::3"};
    const std::vector<std::string> domains = {"d1.com", "d2.com", "d3.com", "d4.com",
                                              "d5.com", "d6.com", "d7.com"};
    const SetupParams setup = {
            .servers = servers,
            .domains = domains,
            .params = kParams,
    };

    EXPECT_EQ(0, cacheSetupResolver(TEST_NETID, setup));
    EXPECT_TRUE(resolv_has_nameservers(TEST_NETID));

    const CacheStats cacheStats = {
            .setup = {.servers = std::vector(servers.begin(), servers.begin() + MAXNS),
                      .domains = std::vector(domains.begin(), domains.begin() + MAXDNSRCH),
                      .params = setup.params},
            .stats = {},
            .pendingReqTimeoutCount = 0,
    };
    expectCacheStats("ResolverSetup_Prune", TEST_NETID, cacheStats);
}

TEST_F(ResolvCacheTest, GetStats) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));
    const SetupParams setup = {
            .servers = {"127.0.0.1", "::127.0.0.2", "fe80::3"},
            .domains = {"domain1.com", "domain2.com"},
            .params = kParams,
    };

    EXPECT_EQ(0, cacheSetupResolver(TEST_NETID, setup));
    EXPECT_TRUE(resolv_has_nameservers(TEST_NETID));

    const CacheStats cacheStats = {
            .setup = setup,
            .stats = {},
            .pendingReqTimeoutCount = 0,
    };
    expectCacheStats("GetStats", TEST_NETID, cacheStats);
}

TEST_F(ResolvCacheTest, FlushCache) {
    EXPECT_EQ(0, cacheCreate(TEST_NETID));
    const SetupParams setup = {
            .servers = {"127.0.0.1", "::127.0.0.2", "fe80::3"},
            .domains = {"domain1.com", "domain2.com"},
            .params = kParams,
    };
    EXPECT_EQ(0, cacheSetupResolver(TEST_NETID, setup));
    EXPECT_TRUE(resolv_has_nameservers(TEST_NETID));

    res_sample sample = {.at = time(NULL), .rtt = 100, .rcode = ns_r_noerror};
    sockaddr_in sin = {.sin_family = AF_INET, .sin_port = htons(DNS_PORT)};
    ASSERT_TRUE(inet_pton(AF_INET, setup.servers[0].c_str(), &sin.sin_addr));
    cacheAddStats(TEST_NETID, 1 /*revision_id*/, IPSockAddr(sin), sample, setup.params.max_samples);

    const CacheStats cacheStats = {
            .setup = setup,
            .stats = {{{sample}, 1 /*sample_count*/, 1 /*sample_next*/}},
            .pendingReqTimeoutCount = 0,
    };
    expectCacheStats("FlushCache: a record in cache stats", TEST_NETID, cacheStats);

    EXPECT_EQ(0, cacheFlush(TEST_NETID));
    const CacheStats cacheStats_empty = {
            .setup = setup,
            .stats = {},
            .pendingReqTimeoutCount = 0,
    };
    expectCacheStats("FlushCache: no record in cache stats", TEST_NETID, cacheStats_empty);
}

TEST_F(ResolvCacheTest, GetHostByAddrFromCache_InvalidArgs) {
    char domain_name[NS_MAXDNAME] = {};
    const char query_v4[] = "1.2.3.5";

    // invalid buffer size
    EXPECT_FALSE(resolv_gethostbyaddr_from_cache(TEST_NETID, domain_name, NS_MAXDNAME + 1, nullptr,
                                                 AF_INET));
    EXPECT_STREQ("", domain_name);

    // invalid query
    EXPECT_FALSE(resolv_gethostbyaddr_from_cache(TEST_NETID, domain_name, NS_MAXDNAME, nullptr,
                                                 AF_INET));
    EXPECT_STREQ("", domain_name);

    // unsupported AF
    EXPECT_FALSE(resolv_gethostbyaddr_from_cache(TEST_NETID, domain_name, NS_MAXDNAME, query_v4,
                                                 AF_UNSPEC));
    EXPECT_STREQ("", domain_name);
}

TEST_F(ResolvCacheTest, GetHostByAddrFromCache) {
    char domain_name[NS_MAXDNAME] = {};
    const char query_v4[] = "1.2.3.5";
    const char query_v6[] = "2001:db8::102:304";
    const char query_v6_unabbreviated[] = "2001:0db8:0000:0000:0000:0000:0102:0304";
    const char query_v6_mixed[] = "2001:db8::1.2.3.4";
    const char answer[] = "existent.in.cache";

    // cache does not exist
    EXPECT_FALSE(resolv_gethostbyaddr_from_cache(TEST_NETID, domain_name, NS_MAXDNAME, query_v4,
                                                 AF_INET));
    EXPECT_STREQ("", domain_name);

    // cache is empty
    EXPECT_EQ(0, cacheCreate(TEST_NETID));
    EXPECT_FALSE(resolv_gethostbyaddr_from_cache(TEST_NETID, domain_name, NS_MAXDNAME, query_v4,
                                                 AF_INET));
    EXPECT_STREQ("", domain_name);

    // no v4 match in cache
    CacheEntry ce = makeCacheEntry(QUERY, "any.data", ns_c_in, ns_t_a, "1.2.3.4");
    EXPECT_EQ(0, cacheAdd(TEST_NETID, ce));
    EXPECT_FALSE(resolv_gethostbyaddr_from_cache(TEST_NETID, domain_name, NS_MAXDNAME, query_v4,
                                                 AF_INET));
    EXPECT_STREQ("", domain_name);

    // v4 match
    ce = makeCacheEntry(QUERY, answer, ns_c_in, ns_t_a, query_v4);
    EXPECT_EQ(0, cacheAdd(TEST_NETID, ce));
    EXPECT_TRUE(resolv_gethostbyaddr_from_cache(TEST_NETID, domain_name, NS_MAXDNAME, query_v4,
                                                AF_INET));
    EXPECT_STREQ(answer, domain_name);

    // no v6 match in cache
    memset(domain_name, 0, NS_MAXDNAME);
    EXPECT_FALSE(resolv_gethostbyaddr_from_cache(TEST_NETID, domain_name, NS_MAXDNAME, query_v6,
                                                 AF_INET6));
    EXPECT_STREQ("", domain_name);

    // v6 match
    ce = makeCacheEntry(QUERY, answer, ns_c_in, ns_t_aaaa, query_v6);
    EXPECT_EQ(0, cacheAdd(TEST_NETID, ce));
    EXPECT_TRUE(resolv_gethostbyaddr_from_cache(TEST_NETID, domain_name, NS_MAXDNAME, query_v6,
                                                AF_INET6));
    EXPECT_STREQ(answer, domain_name);

    // v6 match with unabbreviated address format
    memset(domain_name, 0, NS_MAXDNAME);
    EXPECT_TRUE(resolv_gethostbyaddr_from_cache(TEST_NETID, domain_name, NS_MAXDNAME,
                                                query_v6_unabbreviated, AF_INET6));
    EXPECT_STREQ(answer, domain_name);

    // v6 with mixed address format
    memset(domain_name, 0, NS_MAXDNAME);
    EXPECT_TRUE(resolv_gethostbyaddr_from_cache(TEST_NETID, domain_name, NS_MAXDNAME,
                                                query_v6_mixed, AF_INET6));
    EXPECT_STREQ(answer, domain_name);
}

TEST_F(ResolvCacheTest, GetResolverStats) {
    const res_sample sample1 = {.at = time(nullptr), .rtt = 100, .rcode = ns_r_noerror};
    const res_sample sample2 = {.at = time(nullptr), .rtt = 200, .rcode = ns_r_noerror};
    const res_sample sample3 = {.at = time(nullptr), .rtt = 300, .rcode = ns_r_noerror};
    const res_stats expectedStats[MAXNS] = {
            {{sample1}, 1 /*sample_count*/, 1 /*sample_next*/},
            {{sample2}, 1, 1},
            {{sample3}, 1, 1},
    };
    std::vector<IPSockAddr> nameserverSockAddrs = {
            IPSockAddr::toIPSockAddr("127.0.0.1", DNS_PORT),
            IPSockAddr::toIPSockAddr("::127.0.0.2", DNS_PORT),
            IPSockAddr::toIPSockAddr("fe80::3", DNS_PORT),
    };
    const SetupParams setup = {
            .servers = {"127.0.0.1", "::127.0.0.2", "fe80::3"},
            .domains = {"domain1.com", "domain2.com"},
            .params = kParams,
    };
    EXPECT_EQ(0, cacheCreate(TEST_NETID));
    EXPECT_EQ(0, cacheSetupResolver(TEST_NETID, setup));
    int revision_id = 1;
    cacheAddStats(TEST_NETID, revision_id, nameserverSockAddrs[0], sample1,
                  setup.params.max_samples);
    cacheAddStats(TEST_NETID, revision_id, nameserverSockAddrs[1], sample2,
                  setup.params.max_samples);
    cacheAddStats(TEST_NETID, revision_id, nameserverSockAddrs[2], sample3,
                  setup.params.max_samples);

    res_stats cacheStats[MAXNS]{};
    res_params params;
    EXPECT_EQ(resolv_cache_get_resolver_stats(TEST_NETID, &params, cacheStats, nameserverSockAddrs),
              revision_id);
    EXPECT_TRUE(params == kParams);
    for (size_t i = 0; i < MAXNS; i++) {
        EXPECT_TRUE(cacheStats[i] == expectedStats[i]);
    }

    // pass another list of IPSockAddr
    const res_stats expectedStats2[MAXNS] = {
            {{sample3, sample2}, 2, 2},
            {{sample2}, 1, 1},
            {{sample1}, 1, 1},
    };
    nameserverSockAddrs = {
            IPSockAddr::toIPSockAddr("fe80::3", DNS_PORT),
            IPSockAddr::toIPSockAddr("::127.0.0.2", DNS_PORT),
            IPSockAddr::toIPSockAddr("127.0.0.1", DNS_PORT),
    };
    cacheAddStats(TEST_NETID, revision_id, nameserverSockAddrs[0], sample2,
                  setup.params.max_samples);
    EXPECT_EQ(resolv_cache_get_resolver_stats(TEST_NETID, &params, cacheStats, nameserverSockAddrs),
              revision_id);
    EXPECT_TRUE(params == kParams);
    for (size_t i = 0; i < MAXNS; i++) {
        EXPECT_TRUE(cacheStats[i] == expectedStats2[i]);
    }
}

namespace {

constexpr int EAI_OK = 0;
constexpr char DNS_EVENT_SUBSAMPLING_MAP_FLAG[] =
        "persist.device_config.netd_native.dns_event_subsample_map";

class ScopedCacheCreate {
  public:
    explicit ScopedCacheCreate(unsigned netid, const char* subsampling_map,
                               const char* property = DNS_EVENT_SUBSAMPLING_MAP_FLAG)
        : mStoredNetId(netid), mStoredProperty(property) {
        property_get(property, mStoredMap, "");
        property_set(property, subsampling_map);
        EXPECT_EQ(0, resolv_create_cache_for_net(netid));
    }
    ~ScopedCacheCreate() {
        resolv_delete_cache_for_net(mStoredNetId);
        property_set(mStoredProperty, mStoredMap);
    }

  private:
    unsigned mStoredNetId;
    const char* mStoredProperty;
    char mStoredMap[PROPERTY_VALUE_MAX]{};
};

}  // namespace

TEST_F(ResolvCacheTest, DnsEventSubsampling) {
    // Test defaults, default flag is "default:1 0:100 7:10" if no experiment flag is set
    {
        ScopedCacheCreate scopedCacheCreate(TEST_NETID, "");
        EXPECT_EQ(resolv_cache_get_subsampling_denom(TEST_NETID, EAI_NODATA), 10U);
        EXPECT_EQ(resolv_cache_get_subsampling_denom(TEST_NETID, EAI_OK), 100U);
        EXPECT_EQ(resolv_cache_get_subsampling_denom(TEST_NETID, EAI_BADFLAGS),
                  1U);  // default
        EXPECT_THAT(resolv_cache_dump_subsampling_map(TEST_NETID),
                    testing::UnorderedElementsAreArray({"default:1", "0:100", "7:10"}));
    }
    // Now change the experiment flag to "0:42 default:666"
    {
        ScopedCacheCreate scopedCacheCreate(TEST_NETID, "0:42 default:666");
        EXPECT_EQ(resolv_cache_get_subsampling_denom(TEST_NETID, EAI_OK), 42U);
        EXPECT_EQ(resolv_cache_get_subsampling_denom(TEST_NETID, EAI_NODATA),
                  666U);  // default
        EXPECT_THAT(resolv_cache_dump_subsampling_map(TEST_NETID),
                    testing::UnorderedElementsAreArray({"default:666", "0:42"}));
    }
    // Now change the experiment flag to something illegal
    {
        ScopedCacheCreate scopedCacheCreate(TEST_NETID, "asvaxx");
        // 0(disable log) is the default value if experiment flag is invalid.
        EXPECT_EQ(resolv_cache_get_subsampling_denom(TEST_NETID, EAI_OK), 0U);
        EXPECT_EQ(resolv_cache_get_subsampling_denom(TEST_NETID, EAI_NODATA), 0U);
        EXPECT_TRUE(resolv_cache_dump_subsampling_map(TEST_NETID).empty());
    }
    // Test negative and zero denom
    {
        ScopedCacheCreate scopedCacheCreate(TEST_NETID, "0:-42 default:-666 7:10 10:0");
        // 0(disable log) is the default value if no valid denom is set
        EXPECT_EQ(resolv_cache_get_subsampling_denom(TEST_NETID, EAI_OK), 0U);
        EXPECT_EQ(resolv_cache_get_subsampling_denom(TEST_NETID, EAI_BADFLAGS), 0U);
        EXPECT_EQ(resolv_cache_get_subsampling_denom(TEST_NETID, EAI_NODATA), 10U);
        EXPECT_EQ(resolv_cache_get_subsampling_denom(TEST_NETID, EAI_SOCKTYPE), 0U);
        EXPECT_THAT(resolv_cache_dump_subsampling_map(TEST_NETID),
                    testing::UnorderedElementsAreArray({"7:10", "10:0"}));
    }
}

// TODO: Tests for NetConfig, including:
//     - res_stats
//         -- _resolv_cache_add_resolver_stats_sample()
//         -- android_net_res_stats_get_info_for_net()
// TODO: inject a mock timer into the cache to make TTL tests pass instantly
// TODO: test TTL of RFC 2308 negative caching
