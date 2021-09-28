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

#define LOG_TAG "perfstatsd_cpu"

#include "cpu_usage.h"
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

using namespace android::pixel::perfstatsd;

static bool cDebug = false;
static constexpr char FMT_CPU_TOTAL[] =
    "[CPU: %lld.%03llds][T:%.2f%%,U:%.2f%%,S:%.2f%%,IO:%.2f%%]";
static constexpr char TOP_HEADER[] = "[CPU_TOP]  PID, PROCESS_NAME, USR_TIME, SYS_TIME\n";
static constexpr char FMT_TOP_PROFILE[] = "%6.2f%%   %5d %s %" PRIu64 " %" PRIu64 "\n";

CpuUsage::CpuUsage(void) {
    std::string procstat;
    if (android::base::ReadFileToString("/proc/stat", &procstat)) {
        std::istringstream stream(procstat);
        std::string line;
        while (getline(stream, line)) {
            std::vector<std::string> fields = android::base::Split(line, " ");
            if (fields[0].find("cpu") != std::string::npos && fields[0] != "cpu") {
                CpuData data;
                mPrevCoresUsage.push_back(data);
            }
        }
    }
    mCores = mPrevCoresUsage.size();
    mProfileThreshold = CPU_USAGE_PROFILE_THRESHOLD;
    mTopcount = TOP_PROCESS_COUNT;
}

void CpuUsage::setOptions(const std::string &key, const std::string &value) {
    if (key == PROCPROF_THRESHOLD || key == CPU_DISABLED || key == CPU_DEBUG ||
        key == CPU_TOPCOUNT) {
        uint32_t val = 0;
        if (!base::ParseUint(value, &val)) {
            LOG(ERROR) << "Invalid value: " << value;
            return;
        }

        if (key == PROCPROF_THRESHOLD) {
            mProfileThreshold = val;
            LOG(INFO) << "set profile threshold " << mProfileThreshold;
        } else if (key == CPU_DISABLED) {
            mDisabled = (val != 0);
            LOG(INFO) << "set disabled " << mDisabled;
        } else if (key == CPU_DEBUG) {
            cDebug = (val != 0);
            LOG(INFO) << "set debug " << cDebug;
        } else if (key == CPU_TOPCOUNT) {
            mTopcount = val;
            LOG(INFO) << "set top count " << mTopcount;
        }
    }
}

void CpuUsage::profileProcess(std::string *out) {
    // Read cpu usage per process and find the top ones
    DIR *dir;
    struct dirent *ent;
    std::unordered_map<uint32_t, ProcData> procUsage;
    std::priority_queue<ProcData, std::vector<ProcData>, ProcdataCompare> procList;
    if ((dir = opendir("/proc/")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_DIR) {
                std::string pidStr = ent->d_name;
                std::string::const_iterator it = pidStr.begin();
                while (it != pidStr.end() && isdigit(*it)) ++it;
                if (!pidStr.empty() && it == pidStr.end()) {
                    std::string pidStat;
                    if (android::base::ReadFileToString("/proc/" + pidStr + "/stat", &pidStat)) {
                        std::vector<std::string> fields = android::base::Split(pidStat, " ");
                        uint32_t pid = 0;
                        uint64_t utime = 0;
                        uint64_t stime = 0;
                        uint64_t cutime = 0;
                        uint64_t cstime = 0;

                        if (!base::ParseUint(fields[0], &pid) ||
                            !base::ParseUint(fields[13], &utime) ||
                            !base::ParseUint(fields[14], &stime) ||
                            !base::ParseUint(fields[15], &cutime) ||
                            !base::ParseUint(fields[16], &cstime)) {
                            LOG(ERROR) << "Invalid proc data\n" << pidStat;
                            continue;
                        }
                        std::string proc = fields[1];
                        std::string name =
                            proc.length() > 2 ? proc.substr(1, proc.length() - 2) : "";
                        uint64_t user = utime + cutime;
                        uint64_t system = stime + cstime;
                        uint64_t totalUsage = user + system;

                        uint64_t diffUser = user - mPrevProcdata[pid].user;
                        uint64_t diffSystem = system - mPrevProcdata[pid].system;
                        uint64_t diffUsage = totalUsage - mPrevProcdata[pid].usage;

                        ProcData ldata;
                        ldata.user = user;
                        ldata.system = system;
                        ldata.usage = totalUsage;
                        procUsage[pid] = ldata;

                        float usageRatio = (float)(diffUsage * 100.0 / mDiffCpu);
                        if (cDebug && usageRatio > 100) {
                            LOG(INFO) << "pid: " << pid << " , ratio: " << usageRatio
                                      << " , prev usage: " << mPrevProcdata[pid].usage
                                      << " , cur usage: " << totalUsage
                                      << " , total cpu diff: " << mDiffCpu;
                        }

                        ProcData data;
                        data.pid = pid;
                        data.name = name;
                        data.usageRatio = usageRatio;
                        data.user = diffUser;
                        data.system = diffSystem;
                        procList.push(data);
                    }
                }
            }
        }
        mPrevProcdata = std::move(procUsage);
        out->append(TOP_HEADER);
        for (uint32_t count = 0; !procList.empty() && count < mTopcount; count++) {
            ProcData data = procList.top();
            out->append(android::base::StringPrintf(FMT_TOP_PROFILE, data.usageRatio, data.pid,
                                                    data.name.c_str(), data.user, data.system));
            procList.pop();
        }
        closedir(dir);
    } else {
        LOG(ERROR) << "Fail to open /proc/";
    }
}

void CpuUsage::getOverallUsage(std::chrono::system_clock::time_point &now, std::string *out) {
    mDiffCpu = 0;
    mTotalRatio = 0.0f;
    std::string procStat;

    // Get overall cpu usage
    if (android::base::ReadFileToString("/proc/stat", &procStat)) {
        std::istringstream stream(procStat);
        std::string line;
        while (getline(stream, line)) {
            std::vector<std::string> fields = android::base::Split(line, " ");
            if (fields[0].find("cpu") != std::string::npos) {
                std::string cpuStr = fields[0];
                std::string core = cpuStr.length() > 3 ? cpuStr.substr(3, cpuStr.length() - 3) : "";
                uint64_t user = 0;
                uint64_t nice = 0;
                uint64_t system = 0;
                uint64_t idle = 0;
                uint64_t iowait = 0;
                uint64_t irq = 0;
                uint64_t softirq = 0;
                uint64_t steal = 0;

                // cpu  6013 3243 6311 92390 517 693 319 0 0 0  <-- (fields[1] = "")
                // cpu0 558 139 568 12135 67 121 50 0 0 0
                uint32_t base = core.compare("") ? 1 : 2;

                if (!base::ParseUint(fields[base], &user) ||
                    !base::ParseUint(fields[base + 1], &nice) ||
                    !base::ParseUint(fields[base + 2], &system) ||
                    !base::ParseUint(fields[base + 3], &idle) ||
                    !base::ParseUint(fields[base + 4], &iowait) ||
                    !base::ParseUint(fields[base + 5], &irq) ||
                    !base::ParseUint(fields[base + 6], &softirq) ||
                    !base::ParseUint(fields[base + 7], &steal)) {
                    LOG(ERROR) << "Invalid /proc/stat data\n" << line;
                    continue;
                }

                uint64_t cpuTime = user + nice + system + idle + iowait + irq + softirq + steal;
                uint64_t cpuUsage = cpuTime - idle - iowait;
                uint64_t userUsage = user + nice;

                if (!core.compare("")) {
                    uint64_t diffUsage = cpuUsage - mPrevUsage.cpuUsage;
                    mDiffCpu = cpuTime - mPrevUsage.cpuTime;
                    uint64_t diffUser = userUsage - mPrevUsage.userUsage;
                    uint64_t diffSys = system - mPrevUsage.sysUsage;
                    uint64_t diffIo = iowait - mPrevUsage.ioUsage;

                    mTotalRatio = (float)(diffUsage * 100.0 / mDiffCpu);
                    float userRatio = (float)(diffUser * 100.0 / mDiffCpu);
                    float sysRatio = (float)(diffSys * 100.0 / mDiffCpu);
                    float ioRatio = (float)(diffIo * 100.0 / mDiffCpu);

                    if (cDebug) {
                        LOG(INFO) << "prev total: " << mPrevUsage.cpuUsage
                                  << " , cur total: " << cpuUsage << " , diffusage: " << diffUsage
                                  << " , diffcpu: " << mDiffCpu << " , ratio: " << mTotalRatio;
                    }

                    mPrevUsage.cpuUsage = cpuUsage;
                    mPrevUsage.cpuTime = cpuTime;
                    mPrevUsage.userUsage = userUsage;
                    mPrevUsage.sysUsage = system;
                    mPrevUsage.ioUsage = iowait;

                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - mLast);
                    out->append(android::base::StringPrintf(FMT_CPU_TOTAL, ms.count() / 1000,
                                                            ms.count() % 1000, mTotalRatio,
                                                            userRatio, sysRatio, ioRatio));
                } else {
                    // calculate total cpu usage of each core
                    uint32_t c = 0;
                    if (!base::ParseUint(core, &c)) {
                        LOG(ERROR) << "Invalid core: " << core;
                        continue;
                    }
                    uint64_t diffUsage = cpuUsage - mPrevCoresUsage[c].cpuUsage;
                    float coreTotalRatio = (float)(diffUsage * 100.0 / mDiffCpu);
                    if (cDebug) {
                        LOG(INFO) << "core " << c
                                  << " , prev cpu usage: " << mPrevCoresUsage[c].cpuUsage
                                  << " , cur cpu usage: " << cpuUsage
                                  << " , diffusage: " << diffUsage
                                  << " , difftotalcpu: " << mDiffCpu
                                  << " , ratio: " << coreTotalRatio;
                    }
                    mPrevCoresUsage[c].cpuUsage = cpuUsage;

                    char buf[64];
                    sprintf(buf, "%.2f%%]", coreTotalRatio);
                    out->append("[" + core + ":" + std::string(buf));
                }
            }
        }
        out->append("\n");
    } else {
        LOG(ERROR) << "Fail to read /proc/stat";
    }
}

void CpuUsage::refresh(void) {
    if (mDisabled)
        return;

    std::string out;
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

    getOverallUsage(now, &out);

    if (mTotalRatio >= mProfileThreshold) {
        if (cDebug)
            LOG(INFO) << "Total CPU usage over " << mProfileThreshold << "%";
        std::string profileResult;
        profileProcess(&profileResult);
        if (mProfileProcess) {
            // Dump top processes once met threshold continuously at least twice.
            out.append(profileResult);
        } else
            mProfileProcess = true;
    } else
        mProfileProcess = false;

    append(now, out);
    mLast = now;
    if (cDebug) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - now);
        LOG(INFO) << "Took " << ms.count() << " ms, data bytes: " << out.length();
    }
}
