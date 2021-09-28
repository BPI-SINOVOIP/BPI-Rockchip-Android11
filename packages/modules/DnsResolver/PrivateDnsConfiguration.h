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

#pragma once

#include <list>
#include <map>
#include <mutex>
#include <vector>

#include <android-base/thread_annotations.h>

#include "DnsTlsServer.h"

namespace android {
namespace net {

// The DNS over TLS mode on a specific netId.
enum class PrivateDnsMode : uint8_t { OFF, OPPORTUNISTIC, STRICT };

// Validation status of a DNS over TLS server (on a specific netId).
enum class Validation : uint8_t { in_process, success, fail, unknown_server, unknown_netid };

struct PrivateDnsStatus {
    PrivateDnsMode mode;
    std::map<DnsTlsServer, Validation, AddressComparator> serversMap;

    std::list<DnsTlsServer> validatedServers() const {
        std::list<DnsTlsServer> servers;

        for (const auto& pair : serversMap) {
            if (pair.second == Validation::success) {
                servers.push_back(pair.first);
            }
        }
        return servers;
    }
};

class PrivateDnsConfiguration {
  public:
    int set(int32_t netId, uint32_t mark, const std::vector<std::string>& servers,
            const std::string& name, const std::string& caCert) EXCLUDES(mPrivateDnsLock);

    PrivateDnsStatus getStatus(unsigned netId) EXCLUDES(mPrivateDnsLock);

    void clear(unsigned netId) EXCLUDES(mPrivateDnsLock);

  private:
    typedef std::map<DnsTlsServer, Validation, AddressComparator> PrivateDnsTracker;
    typedef std::set<DnsTlsServer, AddressComparator> ThreadTracker;

    void validatePrivateDnsProvider(const DnsTlsServer& server, PrivateDnsTracker& tracker,
                                    unsigned netId, uint32_t mark) REQUIRES(mPrivateDnsLock);

    bool recordPrivateDnsValidation(const DnsTlsServer& server, unsigned netId, bool success);

    bool needValidateThread(const DnsTlsServer& server, unsigned netId) REQUIRES(mPrivateDnsLock);
    void cleanValidateThreadTracker(const DnsTlsServer& server, unsigned netId);

    // Start validation for newly added servers as well as any servers that have
    // landed in Validation::fail state. Note that servers that have failed
    // multiple validation attempts but for which there is still a validating
    // thread running are marked as being Validation::in_process.
    bool needsValidation(const PrivateDnsTracker& tracker, const DnsTlsServer& server);

    std::mutex mPrivateDnsLock;
    std::map<unsigned, PrivateDnsMode> mPrivateDnsModes GUARDED_BY(mPrivateDnsLock);
    // Structure for tracking the validation status of servers on a specific netId.
    // Using the AddressComparator ensures at most one entry per IP address.
    std::map<unsigned, PrivateDnsTracker> mPrivateDnsTransports GUARDED_BY(mPrivateDnsLock);
    std::map<unsigned, ThreadTracker> mPrivateDnsValidateThreads GUARDED_BY(mPrivateDnsLock);
};

extern PrivateDnsConfiguration gPrivateDnsConfiguration;

}  // namespace net
}  // namespace android
