/**
 * Copyright (c) 2020, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <climits>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <android-base/thread_annotations.h>
#include <netdutils/DumpWriter.h>

namespace android::net {

// TODO: Add some way to update the stored experiment flags periodically.
// TODO: Refactor this class and make things easier. (e.g. remove string map.)
class Experiments {
  public:
    using GetExperimentFlagIntFunction = std::function<int(const std::string&, int)>;
    static Experiments* getInstance();
    int getFlag(std::string_view key, int defaultValue) const EXCLUDES(mMutex);
    void update();
    void dump(netdutils::DumpWriter& dw) const EXCLUDES(mMutex);

    Experiments(Experiments const&) = delete;
    void operator=(Experiments const&) = delete;

  private:
    explicit Experiments(GetExperimentFlagIntFunction getExperimentFlagIntFunction);
    Experiments() = delete;
    void updateInternal() EXCLUDES(mMutex);
    mutable std::mutex mMutex;
    std::unordered_map<std::string_view, int> mFlagsMapInt GUARDED_BY(mMutex);
    // TODO: Migrate other experiment flags to here.
    // (retry_count, retransmission_time_interval, dot_connect_timeout_ms)
    static constexpr const char* const kExperimentFlagKeyList[] = {
            "keep_listening_udp", "parallel_lookup", "parallel_lookup_sleep_time"};
    // This value is used in updateInternal as the default value if any flags can't be found.
    static constexpr int kFlagIntDefault = INT_MIN;
    // For testing.
    friend class ExperimentsTest;
    const GetExperimentFlagIntFunction mGetExperimentFlagIntFunction;
};

}  // namespace android::net
