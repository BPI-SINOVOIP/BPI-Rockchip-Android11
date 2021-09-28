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

#ifndef _PERFSTATS_BUFFER_H_
#define _PERFSTATS_BUFFER_H_

#include <dirent.h>
#include <inttypes.h>
#include <pthread.h>
#include <time.h>

#include <chrono>
#include <iomanip>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <utils/RefBase.h>

namespace android {
namespace pixel {
namespace perfstatsd {

constexpr auto operator""_KiB(unsigned long long const num) {
    return num * 1024;
}

class StatsData {
  public:
    std::chrono::system_clock::time_point getTime() const { return mTime; }
    std::string getData() const { return mData; }
    void setTime(std::chrono::system_clock::time_point &time) { mTime = time; }
    void setData(std::string &data) { mData = data; }

  private:
    std::chrono::system_clock::time_point mTime;
    std::string mData;
};

class PerfstatsBuffer {
  public:
    size_t size() { return mBufferSize; }
    size_t count() { return mStorage.size(); }

    void setSize(size_t size) { mBufferSize = size; }
    void emplace(StatsData &&data);
    const std::queue<StatsData> &dump(void);

  private:
    size_t mBufferSize = 0U;
    std::queue<StatsData> mStorage;
};

struct StatsdataCompare {
    // sort time in ascending order
    bool operator()(const StatsData &a, const StatsData &b) const {
        return a.getTime() > b.getTime();
    }
};

}  // namespace perfstatsd
}  // namespace pixel
}  // namespace android

#endif /*  _PERFSTATS_BUFFER_H_ */
