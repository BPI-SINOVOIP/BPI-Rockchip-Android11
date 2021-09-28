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

#include <chrono>
#include <deque>
#include <map>
#include <vector>

#include <android-base/thread_annotations.h>
#include <netdutils/DumpWriter.h>
#include <netdutils/InternetAddresses.h>

#include "ResolverStats.h"
#include "stats.pb.h"

namespace android::net {

// The overall information of a StatsRecords.
struct StatsData {
    StatsData(const netdutils::IPSockAddr& ipSockAddr) : serverSockAddr(ipSockAddr) {
        lastUpdate = std::chrono::steady_clock::now();
    };

    // Server socket address.
    netdutils::IPSockAddr serverSockAddr;

    // The most recent number of records being accumulated.
    int total = 0;

    // The map used to store the number of each rcode.
    std::map<int, int> rcodeCounts;

    // The aggregated RTT in microseconds.
    // For DNS-over-TCP, it includes TCP handshake.
    // For DNS-over-TLS, it might include TCP handshake plus SSL handshake.
    std::chrono::microseconds latencyUs = {};

    // The last update timestamp.
    std::chrono::time_point<std::chrono::steady_clock> lastUpdate;

    std::string toString() const;

    // For testing.
    bool operator==(const StatsData& o) const;
    friend std::ostream& operator<<(std::ostream& os, const StatsData& data) {
        return os << data.toString();
    }
};

// A circular buffer based class used to store the statistics for a server with a protocol.
class StatsRecords {
  public:
    struct Record {
        int rcode;
        std::chrono::microseconds latencyUs;
    };

    StatsRecords(const netdutils::IPSockAddr& ipSockAddr, size_t size);

    void push(const Record& record);

    const StatsData& getStatsData() const { return mStatsData; }

  private:
    void updateStatsData(const Record& record, const bool add);

    std::deque<Record> mRecords;
    size_t mCapacity;
    StatsData mStatsData;
};

// DnsStats class manages the statistics of DNS servers per netId.
// The class itself is not thread-safe.
class DnsStats {
  public:
    using ServerStatsMap = std::map<netdutils::IPSockAddr, StatsRecords>;

    // Add |servers| to the map, and remove no-longer-used servers.
    // Return true if they are successfully added; otherwise, return false.
    bool setServers(const std::vector<netdutils::IPSockAddr>& servers, Protocol protocol);

    // Return true if |record| is successfully added into |server|'s stats; otherwise, return false.
    bool addStats(const netdutils::IPSockAddr& server, const DnsQueryEvent& record);

    void dump(netdutils::DumpWriter& dw);

    // For testing.
    std::vector<StatsData> getStats(Protocol protocol) const;

    // TODO: Compatible support for getResolverInfo().
    // TODO: Support getSortedServers().

    static constexpr size_t kLogSize = 128;

  private:
    std::map<Protocol, ServerStatsMap> mStats;
};

}  // namespace android::net
