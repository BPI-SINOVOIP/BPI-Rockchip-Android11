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

#ifndef _RESOLVER_CONTROLLER_H_
#define _RESOLVER_CONTROLLER_H_

#include <list>
#include <set>
#include <vector>

#include <aidl/android/net/ResolverParamsParcel.h>
#include "Dns64Configuration.h"
#include "netd_resolv/resolv.h"
#include "netdutils/DumpWriter.h"

struct res_params;

namespace android {
namespace net {

class ResolverController {
  public:
    ResolverController();
    ~ResolverController() = default;

    void destroyNetworkCache(unsigned netid);
    int createNetworkCache(unsigned netid);
    int flushNetworkCache(unsigned netid);

    // Binder specific functions, which convert between the ResolverParamsParcel and the
    // actual data structures.
    int setResolverConfiguration(const aidl::android::net::ResolverParamsParcel& resolverParams);

    int getResolverInfo(int32_t netId, std::vector<std::string>* servers,
                        std::vector<std::string>* domains, std::vector<std::string>* tlsServers,
                        std::vector<int32_t>* params, std::vector<int32_t>* stats,
                        std::vector<int32_t>* wait_for_pending_req_timeout_count);

    // Start or stop NAT64 prefix discovery.
    void startPrefix64Discovery(int32_t netId);
    void stopPrefix64Discovery(int32_t netId);

    // Set or clear a NAT64 prefix discovered by other sources (e.g., RA).
    int setPrefix64(unsigned netId, const netdutils::IPPrefix& prefix) {
        return mDns64Configuration.setPrefix64(netId, prefix);
    }

    int clearPrefix64(unsigned netId) { return mDns64Configuration.clearPrefix64(netId); }

    // Return the current NAT64 prefix network, regardless of how it was discovered.
    int getPrefix64(unsigned netId, netdutils::IPPrefix* prefix);

    void dump(netdutils::DumpWriter& dw, unsigned netId);

  private:
    Dns64Configuration mDns64Configuration;
};
}  // namespace net
}  // namespace android

#endif /* _RESOLVER_CONTROLLER_H_ */
