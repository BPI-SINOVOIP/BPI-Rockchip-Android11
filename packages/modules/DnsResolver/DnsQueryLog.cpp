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

#include "DnsQueryLog.h"

#include <android-base/stringprintf.h>

namespace android::net {

namespace {

std::string maskHostname(const std::string& hostname) {
    // Boundary issue is handled in substr().
    return hostname.substr(0, 1) + "***";
}

// Return the string of masked addresses of the first v4 address and the first v6 address.
std::string maskIps(const std::vector<std::string>& ips) {
    std::string ret;
    bool v4Found = false, v6Found = false;
    for (const auto& ip : ips) {
        if (auto pos = ip.find_first_of(':'); pos != ip.npos && !v6Found) {
            ret += ip.substr(0, pos + 1) + "***, ";
            v6Found = true;
        } else if (auto pos = ip.find_first_of('.'); pos != ip.npos && !v4Found) {
            ret += ip.substr(0, pos + 1) + "***, ";
            v4Found = true;
        }
        if (v6Found && v4Found) break;
    }
    return ret.empty() ? "" : ret.substr(0, ret.length() - 2);
}

// Return the readable string format "hr:min:sec.ms".
std::string timestampToString(const std::chrono::system_clock::time_point& ts) {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    const auto time_sec = std::chrono::system_clock::to_time_t(ts);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&time_sec));
    int ms = duration_cast<milliseconds>(ts.time_since_epoch()).count() % 1000;
    return android::base::StringPrintf("%s.%03d", buf, ms);
}

}  // namespace

void DnsQueryLog::push(Record&& record) {
    std::lock_guard guard(mLock);
    mQueue.push_back(std::move(record));
    if (mQueue.size() > mCapacity) {
        mQueue.pop_front();
    }
}

void DnsQueryLog::dump(netdutils::DumpWriter& dw) const {
    dw.println("DNS query log (last %lld minutes):", (mValidityTimeMs / 60000).count());
    netdutils::ScopedIndent indentStats(dw);
    const auto now = std::chrono::system_clock::now();

    std::lock_guard guard(mLock);
    for (const auto& record : mQueue) {
        if (now - record.timestamp > mValidityTimeMs) continue;

        const std::string maskedHostname = maskHostname(record.hostname);
        const std::string maskedIpsStr = maskIps(record.addrs);
        const std::string time = timestampToString(record.timestamp);
        dw.println("time=%s netId=%u uid=%u pid=%d hostname=%s answer=[%s] (%dms)", time.c_str(),
                   record.netId, record.uid, record.pid, maskedHostname.c_str(),
                   maskedIpsStr.c_str(), record.timeTaken);
    }
}

}  // namespace android::net
