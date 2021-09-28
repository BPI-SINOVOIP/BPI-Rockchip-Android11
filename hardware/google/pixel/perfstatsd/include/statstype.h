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

#ifndef _STATSTYPE_H_
#define _STATSTYPE_H_

#include <perfstats_buffer.h>

namespace android {
namespace pixel {
namespace perfstatsd {

class StatsType : public RefBase {
  public:
    virtual void refresh() = 0;
    virtual void setOptions(const std::string &, const std::string &) = 0;
    void dump(std::priority_queue<StatsData, std::vector<StatsData>, StatsdataCompare> *queue) {
        std::unique_lock<std::mutex> mlock(mMutex);
        std::queue<StatsData> buffer = mBuffer.dump();
        while (!buffer.empty()) {
            queue->push(buffer.front());
            buffer.pop();
        }
    }
    size_t bufferSize() { return mBuffer.size(); }
    void setBufferSize(size_t size) { mBuffer.setSize(size); }
    size_t bufferCount() { return mBuffer.count(); }

  protected:
    void append(StatsData &&data) {
        std::unique_lock<std::mutex> mlock(mMutex);
        mBuffer.emplace(std::forward<StatsData>(data));
    }
    void append(std::chrono::system_clock::time_point &time, std::string &content) {
        StatsData data;
        data.setTime(time);
        data.setData(content);
        append(std::move(data));
    }
    void append(std::string &content) {
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        append(now, content);
    }

  private:
    PerfstatsBuffer mBuffer;
    std::mutex mMutex;
};

}  // namespace perfstatsd
}  // namespace pixel
}  // namespace android

#endif /* _STATSTYPE_H_ */
