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

#define LOG_TAG "dns_responder_client"

#include "dns_responder_client_ndk.h"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include <android/binder_manager.h>
#include "NetdClient.h"

// TODO: make this dynamic and stop depending on implementation details.
#define TEST_OEM_NETWORK "oem29"
#define TEST_NETID 30

// TODO: move this somewhere shared.
static const char* ANDROID_DNS_MODE = "ANDROID_DNS_MODE";

using aidl::android::net::IDnsResolver;
using aidl::android::net::INetd;
using aidl::android::net::ResolverParamsParcel;
using android::base::StringPrintf;
using android::net::ResolverStats;

void DnsResponderClient::SetupMappings(unsigned numHosts, const std::vector<std::string>& domains,
                                       std::vector<Mapping>* mappings) {
    mappings->resize(numHosts * domains.size());
    auto mappingsIt = mappings->begin();
    for (unsigned i = 0; i < numHosts; ++i) {
        for (const auto& domain : domains) {
            mappingsIt->host = StringPrintf("host%u", i);
            mappingsIt->entry = StringPrintf("%s.%s.", mappingsIt->host.c_str(), domain.c_str());
            mappingsIt->ip4 = StringPrintf("192.0.2.%u", i % 253 + 1);
            mappingsIt->ip6 = StringPrintf("2001:db8::%x", i % 65534 + 1);
            ++mappingsIt;
        }
    }
}

// TODO: Use SetResolverConfiguration() with ResolverParamsParcel struct directly.
// DEPRECATED: Use SetResolverConfiguration() in new code
ResolverParamsParcel DnsResponderClient::makeResolverParamsParcel(
        int netId, const std::vector<int>& params, const std::vector<std::string>& servers,
        const std::vector<std::string>& domains, const std::string& tlsHostname,
        const std::vector<std::string>& tlsServers, const std::string& caCert) {
    ResolverParamsParcel paramsParcel;

    paramsParcel.netId = netId;
    paramsParcel.sampleValiditySeconds = params[IDnsResolver::RESOLVER_PARAMS_SAMPLE_VALIDITY];
    paramsParcel.successThreshold = params[IDnsResolver::RESOLVER_PARAMS_SUCCESS_THRESHOLD];
    paramsParcel.minSamples = params[IDnsResolver::RESOLVER_PARAMS_MIN_SAMPLES];
    paramsParcel.maxSamples = params[IDnsResolver::RESOLVER_PARAMS_MAX_SAMPLES];
    if (params.size() > IDnsResolver::RESOLVER_PARAMS_BASE_TIMEOUT_MSEC) {
        paramsParcel.baseTimeoutMsec = params[IDnsResolver::RESOLVER_PARAMS_BASE_TIMEOUT_MSEC];
    } else {
        paramsParcel.baseTimeoutMsec = 0;
    }
    if (params.size() > IDnsResolver::RESOLVER_PARAMS_RETRY_COUNT) {
        paramsParcel.retryCount = params[IDnsResolver::RESOLVER_PARAMS_RETRY_COUNT];
    } else {
        paramsParcel.retryCount = 0;
    }
    paramsParcel.servers = servers;
    paramsParcel.domains = domains;
    paramsParcel.tlsName = tlsHostname;
    paramsParcel.tlsServers = tlsServers;
    paramsParcel.tlsFingerprints = {};
    paramsParcel.caCertificate = caCert;

    // Note, do not remove this otherwise the ResolverTest#ConnectTlsServerTimeout won't pass in M4
    // module.
    // TODO: remove after 2020-01 rolls out.
    paramsParcel.tlsConnectTimeoutMs = 1000;

    return paramsParcel;
}

bool DnsResponderClient::GetResolverInfo(aidl::android::net::IDnsResolver* dnsResolverService,
                                         unsigned netId, std::vector<std::string>* servers,
                                         std::vector<std::string>* domains,
                                         std::vector<std::string>* tlsServers, res_params* params,
                                         std::vector<ResolverStats>* stats,
                                         int* waitForPendingReqTimeoutCount) {
    using aidl::android::net::IDnsResolver;
    std::vector<int32_t> params32;
    std::vector<int32_t> stats32;
    std::vector<int32_t> waitForPendingReqTimeoutCount32{0};
    auto rv = dnsResolverService->getResolverInfo(netId, servers, domains, tlsServers, &params32,
                                                  &stats32, &waitForPendingReqTimeoutCount32);

    if (!rv.isOk() || params32.size() != static_cast<size_t>(IDnsResolver::RESOLVER_PARAMS_COUNT)) {
        return false;
    }
    *params = res_params{
            .sample_validity =
                    static_cast<uint16_t>(params32[IDnsResolver::RESOLVER_PARAMS_SAMPLE_VALIDITY]),
            .success_threshold =
                    static_cast<uint8_t>(params32[IDnsResolver::RESOLVER_PARAMS_SUCCESS_THRESHOLD]),
            .min_samples =
                    static_cast<uint8_t>(params32[IDnsResolver::RESOLVER_PARAMS_MIN_SAMPLES]),
            .max_samples =
                    static_cast<uint8_t>(params32[IDnsResolver::RESOLVER_PARAMS_MAX_SAMPLES]),
            .base_timeout_msec = params32[IDnsResolver::RESOLVER_PARAMS_BASE_TIMEOUT_MSEC],
            .retry_count = params32[IDnsResolver::RESOLVER_PARAMS_RETRY_COUNT],
    };
    *waitForPendingReqTimeoutCount = waitForPendingReqTimeoutCount32[0];
    return ResolverStats::decodeAll(stats32, stats);
}

bool DnsResponderClient::isRemoteVersionSupported(
        aidl::android::net::IDnsResolver* dnsResolverService, int requiredVersion) {
    int remoteVersion = 0;
    if (!dnsResolverService->getInterfaceVersion(&remoteVersion).isOk()) {
        LOG(FATAL) << "Can't get 'dnsresolver' remote version";
    }
    if (remoteVersion < requiredVersion) {
        LOG(WARNING) << StringPrintf("Remote version: %d < Required version: %d", remoteVersion,
                                     requiredVersion);
        return false;
    }
    return true;
}

bool DnsResponderClient::SetResolversForNetwork(const std::vector<std::string>& servers,
                                                const std::vector<std::string>& domains,
                                                const std::vector<int>& params) {
    const auto& resolverParams =
            makeResolverParamsParcel(TEST_NETID, params, servers, domains, "", {}, "");
    const auto rv = mDnsResolvSrv->setResolverConfiguration(resolverParams);
    return rv.isOk();
}

bool DnsResponderClient::SetResolversWithTls(const std::vector<std::string>& servers,
                                             const std::vector<std::string>& domains,
                                             const std::vector<int>& params,
                                             const std::vector<std::string>& tlsServers,
                                             const std::string& name) {
    const auto& resolverParams = makeResolverParamsParcel(TEST_NETID, params, servers, domains,
                                                          name, tlsServers, kCaCert);
    const auto rv = mDnsResolvSrv->setResolverConfiguration(resolverParams);
    if (!rv.isOk()) LOG(ERROR) << "SetResolversWithTls() -> " << rv.getMessage();
    return rv.isOk();
}

bool DnsResponderClient::SetResolversFromParcel(const ResolverParamsParcel& resolverParams) {
    const auto rv = mDnsResolvSrv->setResolverConfiguration(resolverParams);
    if (!rv.isOk()) LOG(ERROR) << "SetResolversFromParcel() -> " << rv.getMessage();
    return rv.isOk();
}

ResolverParamsParcel DnsResponderClient::GetDefaultResolverParamsParcel() {
    return makeResolverParamsParcel(TEST_NETID, kDefaultParams, kDefaultServers,
                                    kDefaultSearchDomains, {} /* tlsHostname */, kDefaultServers,
                                    kCaCert);
}

void DnsResponderClient::SetupDNSServers(unsigned numServers, const std::vector<Mapping>& mappings,
                                         std::vector<std::unique_ptr<test::DNSResponder>>* dns,
                                         std::vector<std::string>* servers) {
    const char* listenSrv = "53";
    dns->resize(numServers);
    servers->resize(numServers);
    for (unsigned i = 0; i < numServers; ++i) {
        auto& server = (*servers)[i];
        auto& d = (*dns)[i];
        server = StringPrintf("127.0.0.%u", i + 100);
        d = std::make_unique<test::DNSResponder>(server, listenSrv, ns_rcode::ns_r_servfail);
        for (const auto& mapping : mappings) {
            d->addMapping(mapping.entry.c_str(), ns_type::ns_t_a, mapping.ip4.c_str());
            d->addMapping(mapping.entry.c_str(), ns_type::ns_t_aaaa, mapping.ip6.c_str());
        }
        d->startServer();
    }
}

int DnsResponderClient::SetupOemNetwork() {
    mNetdSrv->networkDestroy(TEST_NETID);
    mDnsResolvSrv->destroyNetworkCache(TEST_NETID);
    auto ret = mNetdSrv->networkCreatePhysical(TEST_NETID, INetd::PERMISSION_NONE);
    if (!ret.isOk()) {
        fprintf(stderr, "Creating physical network %d failed, %s\n", TEST_NETID, ret.getMessage());
        return -1;
    }
    ret = mDnsResolvSrv->createNetworkCache(TEST_NETID);
    if (!ret.isOk()) {
        fprintf(stderr, "Creating network cache %d failed, %s\n", TEST_NETID, ret.getMessage());
        return -1;
    }
    setNetworkForProcess(TEST_NETID);
    if ((unsigned)TEST_NETID != getNetworkForProcess()) {
        return -1;
    }
    return TEST_NETID;
}

void DnsResponderClient::TearDownOemNetwork(int oemNetId) {
    if (oemNetId != -1) {
        mNetdSrv->networkDestroy(oemNetId);
        mDnsResolvSrv->destroyNetworkCache(oemNetId);
    }
}

void DnsResponderClient::SetUp() {
    // binder setup
    ndk::SpAIBinder netdBinder = ndk::SpAIBinder(AServiceManager_getService("netd"));
    mNetdSrv = INetd::fromBinder(netdBinder);
    if (mNetdSrv.get() == nullptr) {
        LOG(FATAL) << "Can't connect to service 'netd'. Missing root privileges? uid=" << getuid();
    }

    ndk::SpAIBinder resolvBinder = ndk::SpAIBinder(AServiceManager_getService("dnsresolver"));
    mDnsResolvSrv = IDnsResolver::fromBinder(resolvBinder);
    if (mDnsResolvSrv.get() == nullptr) {
        LOG(FATAL) << "Can't connect to service 'dnsresolver'. Missing root privileges? uid="
                   << getuid();
    }

    // Ensure resolutions go via proxy.
    setenv(ANDROID_DNS_MODE, "", 1);
    mOemNetId = SetupOemNetwork();
}

void DnsResponderClient::TearDown() {
    TearDownOemNetwork(mOemNetId);
}
