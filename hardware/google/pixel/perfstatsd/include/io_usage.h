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
#ifndef _IO_USAGE_H_
#define _IO_USAGE_H_

#include <statstype.h>
#include <chrono>
#include <sstream>
#include <string>

#include <unordered_map>

#define IO_USAGE_BUFFER_SIZE (6 * 30)
#define IO_TOP_MAX 5

namespace android {
namespace pixel {
namespace perfstatsd {

class ProcPidIoStats {
  private:
    std::chrono::system_clock::time_point mCheckTime;
    std::vector<uint32_t> mPrevPids;
    std::vector<uint32_t> mCurrPids;
    std::unordered_map<uint32_t, std::string> mUidNameMapping;
    // functions
    std::vector<uint32_t> getNewPids();

  public:
    void update(bool forceAll);
    bool getNameForUid(uint32_t uid, std::string *name);
};

struct UserIo {
    uint32_t uid;
    uint64_t fgRead;
    uint64_t bgRead;
    uint64_t fgWrite;
    uint64_t bgWrite;
    uint64_t fgFsync;
    uint64_t bgFsync;

    UserIo &operator=(const UserIo &other) {
        uid = other.uid;
        fgRead = other.fgRead;
        bgRead = other.bgRead;
        fgWrite = other.fgWrite;
        bgWrite = other.bgWrite;
        fgFsync = other.fgFsync;
        bgFsync = other.bgFsync;
        return *this;
    }

    UserIo operator-(const UserIo &other) const {
        UserIo r;
        r.uid = uid;
        r.fgRead = fgRead - other.fgRead;
        r.bgRead = bgRead - other.bgRead;
        r.fgWrite = fgWrite - other.fgWrite;
        r.bgWrite = bgWrite - other.bgWrite;
        r.fgFsync = fgFsync - other.fgFsync;
        r.bgFsync = bgFsync - other.bgFsync;
        return r;
    }

    UserIo operator+(const UserIo &other) const {
        UserIo r;
        r.uid = uid;
        r.fgRead = fgRead + other.fgRead;
        r.bgRead = bgRead + other.bgRead;
        r.fgWrite = fgWrite + other.fgWrite;
        r.bgWrite = bgWrite + other.bgWrite;
        r.fgFsync = fgFsync + other.fgFsync;
        r.bgFsync = bgFsync + other.bgFsync;
        return r;
    }

    uint64_t sumWrite() { return fgWrite + bgWrite; }

    uint64_t sumRead() { return fgRead + bgRead; }

    void reset() {
        uid = 0;
        fgRead = 0;
        bgRead = 0;
        fgWrite = 0;
        bgWrite = 0;
        fgFsync = 0;
        bgFsync = 0;
    }
};

class ScopeTimer {
  private:
    bool mDisabled;
    std::string mName;
    std::chrono::system_clock::time_point mStart;

  public:
    ScopeTimer() : ScopeTimer("") {}
    ScopeTimer(std::string name) : mDisabled(false), mName(name) {
        mStart = std::chrono::system_clock::now();
    }
    ~ScopeTimer() {
        if (!mDisabled) {
            std::string msg;
            dump(&msg);
            LOG(INFO) << msg;
        }
    }
    void setEnabled(bool enabled) { mDisabled = !enabled; }
    void dump(std::string *outAppend);
};

constexpr uint64_t IO_USAGE_DUMP_THRESHOLD = 50L * 1000L * 1000L;  // 50MB
class IoStats {
  private:
    uint64_t mMinSizeOfTotalRead = IO_USAGE_DUMP_THRESHOLD;
    uint64_t mMinSizeOfTotalWrite = IO_USAGE_DUMP_THRESHOLD;
    std::chrono::system_clock::time_point mLast;
    std::chrono::system_clock::time_point mNow;
    std::unordered_map<uint32_t, UserIo> mPrevious;
    UserIo mTotal;
    UserIo mWriteTop[IO_TOP_MAX];
    UserIo mReadTop[IO_TOP_MAX];
    std::vector<uint32_t> mUnknownUidList;
    std::unordered_map<uint32_t, std::string> mUidNameMap;
    ProcPidIoStats mProcIoStats;
    // Functions
    std::unordered_map<uint32_t, UserIo> calcIncrement(
        const std::unordered_map<uint32_t, UserIo> &data);
    void updateTopWrite(UserIo usage);
    void updateTopRead(UserIo usage);
    void updateUnknownUidList();

  public:
    IoStats() {
        mNow = std::chrono::system_clock::now();
        mLast = mNow;
    }
    void calcAll(std::unordered_map<uint32_t, UserIo> &&data);
    void setDumpThresholdSizeForRead(uint64_t size) { mMinSizeOfTotalRead = size; }
    void setDumpThresholdSizeForWrite(uint64_t size) { mMinSizeOfTotalWrite = size; }
    bool dump(std::stringstream *output);
};

class IoUsage : public StatsType {
  private:
    bool mDisabled;
    IoStats mStats;

  public:
    IoUsage() : mDisabled(false) {}
    void refresh(void);
    void setOptions(const std::string &key, const std::string &value);
};

}  // namespace perfstatsd
}  // namespace pixel
}  // namespace android

#endif /*  _IO_USAGE_H_ */
