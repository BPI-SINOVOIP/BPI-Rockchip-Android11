/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef NETDUTILS_STOPWATCH_H
#define NETDUTILS_STOPWATCH_H

#include <chrono>

namespace android {
namespace netdutils {

class Stopwatch {
  private:
    using clock = std::chrono::steady_clock;
    using time_point = std::chrono::time_point<clock>;

  public:
    Stopwatch() : mStart(clock::now()) {}
    virtual ~Stopwatch() = default;

    int64_t timeTakenUs() const { return getElapsedUs(clock::now()); }
    int64_t getTimeAndResetUs() {
        const auto& now = clock::now();
        int64_t elapsed = getElapsedUs(now);
        mStart = now;
        return elapsed;
    }

  private:
    time_point mStart;

    int64_t getElapsedUs(const time_point& now) const {
        return (std::chrono::duration_cast<std::chrono::microseconds>(now - mStart)).count();
    }
};

}  // namespace netdutils
}  // namespace android

#endif  // NETDUTILS_STOPWATCH_H
