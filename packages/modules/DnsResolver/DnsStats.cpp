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

#define LOG_TAG "resolv"

#include "DnsStats.h"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

namespace android::net {

using base::StringPrintf;
using netdutils::DumpWriter;
using netdutils::IPAddress;
using netdutils::IPSockAddr;
using netdutils::ScopedIndent;
using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::seconds;

namespace {

static constexpr IPAddress INVALID_IPADDRESS = IPAddress();

std::string rcodeToName(int rcode) {
    // clang-format off
    switch (rcode) {
        case NS_R_NO_ERROR: return "NOERROR";
        case NS_R_FORMERR: return "FORMERR";
        case NS_R_SERVFAIL: return "SERVFAIL";
        case NS_R_NXDOMAIN: return "NXDOMAIN";
        case NS_R_NOTIMPL: return "NOTIMP";
        case NS_R_REFUSED: return "REFUSED";
        case NS_R_YXDOMAIN: return "YXDOMAIN";
        case NS_R_YXRRSET: return "YXRRSET";
        case NS_R_NXRRSET: return "NXRRSET";
        case NS_R_NOTAUTH: return "NOTAUTH";
        case NS_R_NOTZONE: return "NOTZONE";
        case NS_R_INTERNAL_ERROR: return "INTERNAL_ERROR";
        case NS_R_TIMEOUT: return "TIMEOUT";
        default: return StringPrintf("UNKNOWN(%d)", rcode);
    }
    // clang-format on
}

bool ensureNoInvalidIp(const std::vector<IPSockAddr>& servers) {
    for (const auto& server : servers) {
        if (server.ip() == INVALID_IPADDRESS || server.port() == 0) {
            LOG(WARNING) << "Invalid server: " << server;
            return false;
        }
    }
    return true;
}

}  // namespace

// The comparison ignores the last update time.
bool StatsData::operator==(const StatsData& o) const {
    return std::tie(serverSockAddr, total, rcodeCounts, latencyUs) ==
           std::tie(o.serverSockAddr, o.total, o.rcodeCounts, o.latencyUs);
}

std::string StatsData::toString() const {
    if (total == 0) return StringPrintf("%s <no data>", serverSockAddr.ip().toString().c_str());

    const auto now = std::chrono::steady_clock::now();
    const int meanLatencyMs = duration_cast<milliseconds>(latencyUs).count() / total;
    const int lastUpdateSec = duration_cast<seconds>(now - lastUpdate).count();
    std::string buf;
    for (const auto& [rcode, counts] : rcodeCounts) {
        if (counts != 0) {
            buf += StringPrintf("%s:%d ", rcodeToName(rcode).c_str(), counts);
        }
    }
    return StringPrintf("%s (%d, %dms, [%s], %ds)", serverSockAddr.ip().toString().c_str(), total,
                        meanLatencyMs, buf.c_str(), lastUpdateSec);
}

StatsRecords::StatsRecords(const IPSockAddr& ipSockAddr, size_t size)
    : mCapacity(size), mStatsData(ipSockAddr) {}

void StatsRecords::push(const Record& record) {
    updateStatsData(record, true);
    mRecords.push_back(record);

    if (mRecords.size() > mCapacity) {
        updateStatsData(mRecords.front(), false);
        mRecords.pop_front();
    }
}

void StatsRecords::updateStatsData(const Record& record, const bool add) {
    const int rcode = record.rcode;
    if (add) {
        mStatsData.total += 1;
        mStatsData.rcodeCounts[rcode] += 1;
        mStatsData.latencyUs += record.latencyUs;
    } else {
        mStatsData.total -= 1;
        mStatsData.rcodeCounts[rcode] -= 1;
        mStatsData.latencyUs -= record.latencyUs;
    }
    mStatsData.lastUpdate = std::chrono::steady_clock::now();
}

bool DnsStats::setServers(const std::vector<netdutils::IPSockAddr>& servers, Protocol protocol) {
    if (!ensureNoInvalidIp(servers)) return false;

    ServerStatsMap& statsMap = mStats[protocol];
    for (const auto& server : servers) {
        statsMap.try_emplace(server, StatsRecords(server, kLogSize));
    }

    // Clean up the map to eliminate the nodes not belonging to the given list of servers.
    const auto cleanup = [&](ServerStatsMap* statsMap) {
        ServerStatsMap tmp;
        for (const auto& server : servers) {
            if (statsMap->find(server) != statsMap->end()) {
                tmp.insert(statsMap->extract(server));
            }
        }
        statsMap->swap(tmp);
    };

    cleanup(&statsMap);

    return true;
}

bool DnsStats::addStats(const IPSockAddr& ipSockAddr, const DnsQueryEvent& record) {
    if (ipSockAddr.ip() == INVALID_IPADDRESS) return false;

    for (auto& [serverSockAddr, statsRecords] : mStats[record.protocol()]) {
        if (serverSockAddr == ipSockAddr) {
            const StatsRecords::Record rec = {
                    .rcode = record.rcode(),
                    .latencyUs = microseconds(record.latency_micros()),
            };
            statsRecords.push(rec);
            return true;
        }
    }
    return false;
}

std::vector<StatsData> DnsStats::getStats(Protocol protocol) const {
    std::vector<StatsData> ret;

    if (mStats.find(protocol) != mStats.end()) {
        for (const auto& [_, statsRecords] : mStats.at(protocol)) {
            ret.push_back(statsRecords.getStatsData());
        }
    }
    return ret;
}

void DnsStats::dump(DumpWriter& dw) {
    const auto dumpStatsMap = [&](ServerStatsMap& statsMap) {
        ScopedIndent indentLog(dw);
        if (statsMap.size() == 0) {
            dw.println("<no server>");
            return;
        }
        for (const auto& [_, statsRecords] : statsMap) {
            dw.println("%s", statsRecords.getStatsData().toString().c_str());
        }
    };

    dw.println("Server statistics: (total, RTT avg, {rcode:counts}, last update)");
    ScopedIndent indentStats(dw);

    dw.println("over UDP");
    dumpStatsMap(mStats[PROTO_UDP]);

    dw.println("over TLS");
    dumpStatsMap(mStats[PROTO_DOT]);

    dw.println("over TCP");
    dumpStatsMap(mStats[PROTO_TCP]);
}

}  // namespace android::net
