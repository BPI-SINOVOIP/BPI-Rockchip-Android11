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

#include <memory>
#include <string>
#include <vector>

#include <aidl/android/net/IDnsResolver.h>
#include <aidl/android/net/INetd.h>
#include "ResolverStats.h"  // TODO: stop depending on this internal header
#include "dns_responder.h"
#include "dns_tls_certificate.h"
#include "params.h"

inline const std::vector<std::string> kDefaultServers = {"127.0.0.3"};
inline const std::vector<std::string> kDefaultSearchDomains = {"example.com"};
inline const std::vector<int> kDefaultParams = {
        300,      // sample validity in seconds
        25,       // success threshod in percent
        8,    8,  // {MIN,MAX}_SAMPLES
        1000,     // BASE_TIMEOUT_MSEC
        2,        // retry count
};

#define SKIP_IF_REMOTE_VERSION_LESS_THAN(service, version)                                         \
    do {                                                                                           \
        if (!DnsResponderClient::isRemoteVersionSupported(service, version)) {                     \
            std::cerr << "    Skip test. Remote version is too old, required version: " << version \
                      << std::endl;                                                                \
            return;                                                                                \
        }                                                                                          \
    } while (0)

// TODO: Remove dns_responder_client_ndk.{h,cpp} after replacing the binder usage of
// dns_responder_client.*
class DnsResponderClient {
  public:
    struct Mapping {
        std::string host;
        std::string entry;
        std::string ip4;
        std::string ip6;
    };

    virtual ~DnsResponderClient() = default;

    static void SetupMappings(unsigned num_hosts, const std::vector<std::string>& domains,
                              std::vector<Mapping>* mappings);

    // This function is deprecated. Please use SetResolversFromParcel() instead.
    bool SetResolversForNetwork(const std::vector<std::string>& servers = kDefaultServers,
                                const std::vector<std::string>& domains = kDefaultSearchDomains,
                                const std::vector<int>& params = kDefaultParams);

    // This function is deprecated. Please use SetResolversFromParcel() instead.
    bool SetResolversWithTls(const std::vector<std::string>& servers,
                             const std::vector<std::string>& searchDomains,
                             const std::vector<int>& params, const std::string& name) {
        // Pass servers as both network-assigned and TLS servers.  Tests can
        // determine on which server and by which protocol queries arrived.
        return SetResolversWithTls(servers, searchDomains, params, servers, name);
    }

    // This function is deprecated. Please use SetResolversFromParcel() instead.
    bool SetResolversWithTls(const std::vector<std::string>& servers,
                             const std::vector<std::string>& searchDomains,
                             const std::vector<int>& params,
                             const std::vector<std::string>& tlsServers, const std::string& name);

    bool SetResolversFromParcel(const aidl::android::net::ResolverParamsParcel& resolverParams);

    static bool isRemoteVersionSupported(aidl::android::net::IDnsResolver* dnsResolverService,
                                         int enabledVersion);

    static bool GetResolverInfo(aidl::android::net::IDnsResolver* dnsResolverService,
                                unsigned netId, std::vector<std::string>* servers,
                                std::vector<std::string>* domains,
                                std::vector<std::string>* tlsServers, res_params* params,
                                std::vector<android::net::ResolverStats>* stats,
                                int* waitForPendingReqTimeoutCount);

    // Return a default resolver configuration for opportunistic mode.
    static aidl::android::net::ResolverParamsParcel GetDefaultResolverParamsParcel();

    static void SetupDNSServers(unsigned numServers, const std::vector<Mapping>& mappings,
                                std::vector<std::unique_ptr<test::DNSResponder>>* dns,
                                std::vector<std::string>* servers);

    static aidl::android::net::ResolverParamsParcel makeResolverParamsParcel(
            int netId, const std::vector<int>& params, const std::vector<std::string>& servers,
            const std::vector<std::string>& domains, const std::string& tlsHostname,
            const std::vector<std::string>& tlsServers, const std::string& caCert = "");

    int SetupOemNetwork();

    void TearDownOemNetwork(int oemNetId);

    virtual void SetUp();
    virtual void TearDown();

    aidl::android::net::IDnsResolver* resolvService() const { return mDnsResolvSrv.get(); }
    aidl::android::net::INetd* netdService() const { return mNetdSrv.get(); }

  private:
    std::shared_ptr<aidl::android::net::INetd> mNetdSrv;
    std::shared_ptr<aidl::android::net::IDnsResolver> mDnsResolvSrv;
    int mOemNetId = -1;
};
