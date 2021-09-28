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

#define LOG_TAG "perfstatsd_io"

#include "io_usage.h"
#include <android-base/parseint.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <cutils/android_filesystem_config.h>
#include <inttypes.h>
#include <pwd.h>

using namespace android::pixel::perfstatsd;
static constexpr const char *UID_IO_STATS_PATH = "/proc/uid_io/stats";
static constexpr char FMT_STR_TOTAL_USAGE[] =
    "[IO_TOTAL: %lld.%03llds] RD:%s WR:%s fsync:%" PRIu64 "\n";
static constexpr char STR_TOP_HEADER[] =
    "[IO_TOP    ]    fg bytes,    bg bytes,fgsyn,bgsyn :  UID   PKG_NAME\n";
static constexpr char FMT_STR_TOP_WRITE_USAGE[] =
    "[W%d:%6.2f%%]%12" PRIu64 ",%12" PRIu64 ",%5" PRIu64 ",%5" PRIu64 " :%6u %s\n";
static constexpr char FMT_STR_TOP_READ_USAGE[] =
    "[R%d:%6.2f%%]%12" PRIu64 ",%12" PRIu64 ",%5" PRIu64 ",%5" PRIu64 " :%6u %s\n";
static constexpr char FMT_STR_SKIP_TOP_READ[] = "(< %" PRIu64 "MB)skip RD";
static constexpr char FMT_STR_SKIP_TOP_WRITE[] = "(< %" PRIu64 "MB)skip WR";

static bool sOptDebug = false;

/* format number with comma
 * Ex: 10000 => 10,000
 */
static bool formatNum(uint64_t x, char *str, int size) {
    int len = snprintf(str, size, "%" PRIu64, x);
    if (len + 1 > size) {
        return false;
    }
    int extr = ((len - 1) / 3);
    int endpos = len + extr;
    if (endpos > size) {
        return false;
    }
    uint32_t d = 0;
    str[endpos] = 0;
    for (int i = 0, j = endpos - 1; i < len; i++) {
        d = x % 10;
        x = x / 10;
        str[j--] = '0' + d;
        if (i % 3 == 2) {
            if (j >= 0)
                str[j--] = ',';
        }
    }
    return true;
}

static bool isAppUid(uint32_t uid) {
    if (uid >= AID_APP_START) {
        return true;
    }
    return false;
}

std::vector<uint32_t> ProcPidIoStats::getNewPids() {
    std::vector<uint32_t> newpids;
    // Not exists in Previous
    for (int i = 0, len = mCurrPids.size(); i < len; i++) {
        if (std::find(mPrevPids.begin(), mPrevPids.end(), mCurrPids[i]) == mPrevPids.end()) {
            newpids.push_back(mCurrPids[i]);
        }
    }
    return newpids;
}

void ProcPidIoStats::update(bool forceAll) {
    ScopeTimer _debugTimer("update: /proc/pid/status for UID/Name mapping");
    _debugTimer.setEnabled(sOptDebug);
    if (forceAll) {
        mPrevPids.clear();
    } else {
        mPrevPids = mCurrPids;
    }
    // Get current pid list
    mCurrPids.clear();
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir("/proc/")) == NULL) {
        LOG(ERROR) << "failed on opendir '/proc/'";
        return;
    }
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_DIR) {
            uint32_t pid;
            if (android::base::ParseUint(ent->d_name, &pid)) {
                mCurrPids.push_back(pid);
            }
        }
    }
    std::vector<uint32_t> newpids = getNewPids();
    // update mUidNameMapping only for new pids
    for (int i = 0, len = newpids.size(); i < len; i++) {
        uint32_t pid = newpids[i];
        if (sOptDebug > 1)
            LOG(INFO) << i << ".";
        std::string buffer;
        if (!android::base::ReadFileToString("/proc/" + std::to_string(pid) + "/status", &buffer)) {
            if (sOptDebug)
                LOG(INFO) << "/proc/" << std::to_string(pid) << "/status"
                          << ": ReadFileToString failed (process died?)";
            continue;
        }
        // --- Find Name ---
        size_t s = buffer.find("Name:");
        if (s == std::string::npos) {
            continue;
        }
        s += std::strlen("Name:");
        // find the pos of next word
        while (buffer[s] && isspace(buffer[s])) s++;
        if (buffer[s] == 0) {
            continue;
        }
        size_t e = s;
        // find the end pos of the word
        while (buffer[e] && !std::isspace(buffer[e])) e++;
        std::string pname(buffer, s, e - s);

        // --- Find Uid ---
        s = buffer.find("\nUid:", e);
        if (s == std::string::npos) {
            continue;
        }
        s += std::strlen("\nUid:");
        // find the pos of next word
        while (buffer[s] && isspace(buffer[s])) s++;
        if (buffer[s] == 0) {
            continue;
        }
        e = s;
        // find the end pos of the word
        while (buffer[e] && !std::isspace(buffer[e])) e++;
        std::string strUid(buffer, s, e - s);

        if (sOptDebug > 1)
            LOG(INFO) << "(pid, name, uid)=(" << pid << ", " << pname << ", " << strUid << ")"
                      << std::endl;
        uint32_t uid = (uint32_t)std::stoi(strUid);
        mUidNameMapping[uid] = pname;
    }
}

bool ProcPidIoStats::getNameForUid(uint32_t uid, std::string *name) {
    if (mUidNameMapping.find(uid) != mUidNameMapping.end()) {
        *name = mUidNameMapping[uid];
        return true;
    }
    return false;
}

void IoStats::updateTopRead(UserIo usage) {
    UserIo tmp;
    for (int i = 0, len = IO_TOP_MAX; i < len; i++) {
        if (usage.sumRead() > mReadTop[i].sumRead()) {
            // if new read > old read, then swap values
            tmp = mReadTop[i];
            mReadTop[i] = usage;
            usage = tmp;
        }
    }
}

void IoStats::updateTopWrite(UserIo usage) {
    UserIo tmp;
    for (int i = 0, len = IO_TOP_MAX; i < len; i++) {
        if (usage.sumWrite() > mWriteTop[i].sumWrite()) {
            // if new write > old write, then swap values
            tmp = mWriteTop[i];
            mWriteTop[i] = usage;
            usage = tmp;
        }
    }
}

void IoStats::updateUnknownUidList() {
    if (!mUnknownUidList.size()) {
        return;
    }
    ScopeTimer _debugTimer("update overall UID/Name");
    _debugTimer.setEnabled(sOptDebug);
    mProcIoStats.update(false);
    for (uint32_t i = 0, len = mUnknownUidList.size(); i < len; i++) {
        uint32_t uid = mUnknownUidList[i];
        if (isAppUid(uid)) {
            // Get IO throughput for App processes
            std::string pname;
            if (!mProcIoStats.getNameForUid(uid, &pname)) {
                if (sOptDebug)
                    LOG(WARNING) << "unable to find App uid:" << uid;
                continue;
            }
            mUidNameMap[uid] = pname;
        } else {
            // Get IO throughput for system/native processes
            passwd *usrpwd = getpwuid(uid);
            if (!usrpwd) {
                if (sOptDebug)
                    LOG(WARNING) << "unable to find uid:" << uid << " by getpwuid";
                continue;
            }
            mUidNameMap[uid] = std::string(usrpwd->pw_name);
            if (std::find(mUnknownUidList.begin(), mUnknownUidList.end(), uid) !=
                mUnknownUidList.end()) {
            }
        }
        mUnknownUidList.erase(std::remove(mUnknownUidList.begin(), mUnknownUidList.end(), uid),
                              mUnknownUidList.end());
    }

    if (sOptDebug && mUnknownUidList.size() > 0) {
        std::stringstream msg;
        msg << "Some UID/Name can't be retrieved: ";
        for (const auto &i : mUnknownUidList) {
            msg << i << ", ";
        }
        LOG(WARNING) << msg.str();
    }
    mUnknownUidList.clear();
}

std::unordered_map<uint32_t, UserIo> IoStats::calcIncrement(
    const std::unordered_map<uint32_t, UserIo> &data) {
    std::unordered_map<uint32_t, UserIo> diffs;
    for (const auto &it : data) {
        const UserIo &d = it.second;
        // If data not existed, copy one, else calculate the increment.
        if (mPrevious.find(d.uid) == mPrevious.end()) {
            diffs[d.uid] = d;
        } else {
            diffs[d.uid] = d - mPrevious[d.uid];
        }
        // If uid not existed in UidNameMap, then add into unknown list
        if ((diffs[d.uid].sumRead() || diffs[d.uid].sumWrite()) &&
            mUidNameMap.find(d.uid) == mUidNameMap.end()) {
            mUnknownUidList.push_back(d.uid);
        }
    }
    // update Uid/Name mapping for dump()
    updateUnknownUidList();
    return diffs;
}

void IoStats::calcAll(std::unordered_map<uint32_t, UserIo> &&data) {
    // if mList == mNow, it's in init state.
    if (mLast == mNow) {
        mPrevious = std::move(data);
        mLast = mNow;
        mNow = std::chrono::system_clock::now();
        mProcIoStats.update(true);
        for (const auto &d : data) {
            mUnknownUidList.push_back(d.first);
        }
        updateUnknownUidList();
        return;
    }
    mLast = mNow;
    mNow = std::chrono::system_clock::now();

    // calculate incremental IO throughput
    std::unordered_map<uint32_t, UserIo> amounts = calcIncrement(data);
    // assign current data to Previous for next calculating
    mPrevious = std::move(data);
    // Reset Total and Tops
    mTotal.reset();
    for (int i = 0, len = IO_TOP_MAX; i < len; i++) {
        mReadTop[i].reset();
        mWriteTop[i].reset();
    }
    for (const auto &it : amounts) {
        const UserIo &d = it.second;
        // Add into total
        mTotal = mTotal + d;
        // Check if it's top
        updateTopRead(d);
        updateTopWrite(d);
    }
}

/* Dump IO usage (Sample Log)
 *
 * [IO_TOTAL: 10.160s] RD:371,703,808 WR:15,929,344 fsync:567
 * [TOP Usage ]    fg bytes,    bg bytes,fgsyn,bgsyn :  UID   NAME
 * [R1: 33.99%]           0,    73240576,    0,  240 : 10016 .android.gms.ui
 * [R2: 28.34%]    16039936,    45027328,    1,   21 : 10082 -
 * [R3: 16.54%]    11243520,    24395776,    0,   25 : 10055 -
 * [R4: 10.93%]    22241280,     1318912,    0,    1 : 10123 oid.apps.photos
 * [R5: 10.19%]    21528576,      421888,   23,   20 : 10061 android.vending
 * [W1: 58.19%]           0,     7655424,    0,  240 : 10016 .android.gms.ui
 * [W2: 17.03%]     1265664,      974848,   38,   45 : 10069 -
 * [W3: 11.30%]     1486848,           0,   58,    0 :  1000 system
 * [W4:  8.13%]      667648,      401408,   23,   20 : 10061 android.vending
 * [W5:  5.35%]           0,      704512,    0,   25 : 10055 -
 *
 */
bool IoStats::dump(std::stringstream *output) {
    std::stringstream &out = (*output);

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(mNow - mLast);
    char readTotal[32];
    char writeTotal[32];
    if (!formatNum(mTotal.sumRead(), readTotal, 32)) {
        LOG(ERROR) << "formatNum buffer size is too small for read: " << mTotal.sumRead();
    }
    if (!formatNum(mTotal.sumWrite(), writeTotal, 32)) {
        LOG(ERROR) << "formatNum buffer size is too small for write: " << mTotal.sumWrite();
    }

    out << android::base::StringPrintf(FMT_STR_TOTAL_USAGE, ms.count() / 1000, ms.count() % 1000,
                                       readTotal, writeTotal, mTotal.fgFsync + mTotal.bgFsync);

    if (mTotal.sumRead() >= mMinSizeOfTotalRead || mTotal.sumWrite() >= mMinSizeOfTotalWrite) {
        out << STR_TOP_HEADER;
    }
    // Dump READ TOP
    if (mTotal.sumRead() < mMinSizeOfTotalRead) {
        out << android::base::StringPrintf(FMT_STR_SKIP_TOP_READ, mMinSizeOfTotalRead / 1000000)
            << std::endl;
    } else {
        for (int i = 0, len = IO_TOP_MAX; i < len; i++) {
            UserIo &target = mReadTop[i];
            if (target.sumRead() == 0) {
                break;
            }
            float percent = 100.0f * target.sumRead() / mTotal.sumRead();
            const char *package = mUidNameMap.find(target.uid) == mUidNameMap.end()
                                      ? "-"
                                      : mUidNameMap[target.uid].c_str();
            out << android::base::StringPrintf(FMT_STR_TOP_READ_USAGE, i + 1, percent,
                                               target.fgRead, target.bgRead, target.fgFsync,
                                               target.bgFsync, target.uid, package);
        }
    }

    // Dump WRITE TOP
    if (mTotal.sumWrite() < mMinSizeOfTotalWrite) {
        out << android::base::StringPrintf(FMT_STR_SKIP_TOP_WRITE, mMinSizeOfTotalWrite / 1000000)
            << std::endl;
    } else {
        for (int i = 0, len = IO_TOP_MAX; i < len; i++) {
            UserIo &target = mWriteTop[i];
            if (target.sumWrite() == 0) {
                break;
            }
            float percent = 100.0f * target.sumWrite() / mTotal.sumWrite();
            const char *package = mUidNameMap.find(target.uid) == mUidNameMap.end()
                                      ? "-"
                                      : mUidNameMap[target.uid].c_str();
            out << android::base::StringPrintf(FMT_STR_TOP_WRITE_USAGE, i + 1, percent,
                                               target.fgWrite, target.bgWrite, target.fgFsync,
                                               target.bgFsync, target.uid, package);
        }
    }
    return true;
}

static bool loadDataFromLine(std::string &&line, UserIo &data) {
    std::vector<std::string> fields = android::base::Split(line, " ");
    if (fields.size() < 11 || !android::base::ParseUint(fields[0], &data.uid) ||
        !android::base::ParseUint(fields[3], &data.fgRead) ||
        !android::base::ParseUint(fields[4], &data.fgWrite) ||
        !android::base::ParseUint(fields[7], &data.bgRead) ||
        !android::base::ParseUint(fields[8], &data.bgWrite) ||
        !android::base::ParseUint(fields[9], &data.fgFsync) ||
        !android::base::ParseUint(fields[10], &data.bgFsync)) {
        LOG(WARNING) << "Invalid uid I/O stats: \"" << line << "\"";
        return false;
    }
    return true;
}

void ScopeTimer::dump(std::string *outAppend) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - mStart);
    outAppend->append("duration (");
    outAppend->append(mName);
    outAppend->append("): ");
    outAppend->append(std::to_string(ms.count()));
    outAppend->append("ms");
}

/*
 * setOptions - IoUsage supports following options
 *     iostats.min : skip dump when R/W amount is lower than the value
 *     iostats.read.min : skip dump when READ amount is lower than the value
 *     iostats.write.min : skip dump when WRITE amount is lower than the value
 *     iostats.debug : 1 - to enable debug log; 0 - disabled
 */
void IoUsage::setOptions(const std::string &key, const std::string &value) {
    std::stringstream out;
    out << "set IO options: " << key << " , " << value;
    if (key == "iostats.min" || key == "iostats.read.min" || key == "iostats.write.min" ||
        key == "iostats.debug") {
        uint64_t val = 0;
        if (!android::base::ParseUint(value, &val)) {
            out << "!!!! unable to parse value to uint64";
            LOG(ERROR) << out.str();
            return;
        }
        if (key == "iostats.min") {
            mStats.setDumpThresholdSizeForRead(val);
            mStats.setDumpThresholdSizeForWrite(val);
        } else if (key == "iostats.disabled") {
            mDisabled = (val != 0);
        } else if (key == "iostats.read.min") {
            mStats.setDumpThresholdSizeForRead(val);
        } else if (key == "iostats.write.min") {
            mStats.setDumpThresholdSizeForWrite(val);
        } else if (key == "iostats.debug") {
            sOptDebug = (val != 0);
        }
        LOG(INFO) << out.str() << ": Success";
    }
}

void IoUsage::refresh(void) {
    if (mDisabled)
        return;
    ScopeTimer _debugTimer("refresh");
    _debugTimer.setEnabled(sOptDebug);
    std::string buffer;
    if (!android::base::ReadFileToString(UID_IO_STATS_PATH, &buffer)) {
        LOG(ERROR) << UID_IO_STATS_PATH << ": ReadFileToString failed";
    }
    if (sOptDebug)
        LOG(INFO) << "read " << UID_IO_STATS_PATH << " OK.";
    std::vector<std::string> lines = android::base::Split(std::move(buffer), "\n");
    std::unordered_map<uint32_t, UserIo> datas;
    for (uint32_t i = 0; i < lines.size(); i++) {
        if (lines[i].empty()) {
            continue;
        }
        UserIo data;
        if (!loadDataFromLine(std::move(lines[i]), data))
            continue;
        datas[data.uid] = data;
    }
    mStats.calcAll(std::move(datas));
    std::stringstream out;
    mStats.dump(&out);
    const std::string &str = out.str();
    if (sOptDebug) {
        LOG(INFO) << str;
        LOG(INFO) << "output append length:" << str.length();
    }
    append((std::string &)str);
}
