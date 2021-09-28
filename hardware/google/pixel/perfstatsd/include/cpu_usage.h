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

#ifndef _CPU_USAGE_H_
#define _CPU_USAGE_H_

#include <statstype.h>

#define CPU_USAGE_BUFFER_SIZE (6 * 30)
#define TOP_PROCESS_COUNT (5)
#define CPU_USAGE_PROFILE_THRESHOLD (50)

#define PROCPROF_THRESHOLD "cpu.procprof.threshold"
#define CPU_DISABLED "cpu.disabled"
#define CPU_DEBUG "cpu.debug"
#define CPU_TOPCOUNT "cpu.topcount"

namespace android {
namespace pixel {
namespace perfstatsd {

struct CpuData {
    uint64_t cpuUsage;
    uint64_t cpuTime;
    uint64_t userUsage;
    uint64_t sysUsage;
    uint64_t ioUsage;
};

struct ProcData {
    uint32_t pid;
    std::string name;
    float usageRatio;
    uint64_t usage;
    uint64_t user;
    uint64_t system;
};

class CpuUsage : public StatsType {
  public:
    CpuUsage(void);
    void refresh(void);
    void setOptions(const std::string &key, const std::string &value);

  private:
    std::chrono::system_clock::time_point mLast;
    uint32_t mCores;  // cpu core num
    uint32_t mProfileThreshold;
    uint32_t mTopcount;
    bool mDisabled;
    bool mProfileProcess;
    CpuData mPrevUsage;                                    // cpu usage of last record
    std::vector<CpuData> mPrevCoresUsage;                  // cpu usage per core of last record
    std::unordered_map<uint32_t, ProcData> mPrevProcdata;  // <pid, last_usage>
    uint64_t mDiffCpu;
    float mTotalRatio;
    void getOverallUsage(std::chrono::system_clock::time_point &, std::string *);
    void profileProcess(std::string *);
};

struct ProcdataCompare {
    // sort process by usage percentage in descending order
    bool operator()(const ProcData &a, const ProcData &b) const {
        return a.usageRatio < b.usageRatio;
    }
};

}  // namespace perfstatsd
}  // namespace pixel
}  // namespace android

#endif /*  _CPU_USAGE_H_ */
