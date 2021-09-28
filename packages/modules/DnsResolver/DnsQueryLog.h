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

#include <deque>
#include <string>
#include <vector>

#include <android-base/thread_annotations.h>
#include <netdutils/DumpWriter.h>

namespace android::net {

// A circular buffer based class used for query logging. It's thread-safe for concurrent access.
class DnsQueryLog {
  public:
    static constexpr std::string_view DUMP_KEYWORD = "querylog";

    struct Record {
        Record(uint32_t netId, uid_t uid, pid_t pid, const std::string& hostname,
               const std::vector<std::string>& addrs, int timeTaken)
            : netId(netId),
              uid(uid),
              pid(pid),
              hostname(hostname),
              addrs(addrs),
              timeTaken(timeTaken) {}
        const uint32_t netId;
        const uid_t uid;
        const pid_t pid;
        const std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();
        const std::string hostname;
        const std::vector<std::string> addrs;
        const int timeTaken;
    };

    // Allow the tests to set the capacity and the validaty time in milliseconds.
    DnsQueryLog(size_t size = kDefaultLogSize,
                std::chrono::milliseconds time = kDefaultValidityMinutes)
        : mCapacity(size), mValidityTimeMs(time) {}

    void push(Record&& record) EXCLUDES(mLock);
    void dump(netdutils::DumpWriter& dw) const EXCLUDES(mLock);

  private:
    mutable std::mutex mLock;
    std::deque<Record> mQueue GUARDED_BY(mLock);
    const size_t mCapacity;
    const std::chrono::milliseconds mValidityTimeMs;

    // The capacity of the circular buffer.
    static constexpr size_t kDefaultLogSize = 200;

    // Limit to dump the queries within last |kDefaultValidityMinutes| minutes.
    static constexpr std::chrono::minutes kDefaultValidityMinutes{60};
};

}  // namespace android::net
