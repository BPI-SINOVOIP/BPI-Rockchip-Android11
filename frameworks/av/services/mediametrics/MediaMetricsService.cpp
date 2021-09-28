/*
 * Copyright (C) 2017 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "MediaMetricsService"
#include <utils/Log.h>

#include "MediaMetricsService.h"

#include <pwd.h> //getpwuid

#include <android/content/pm/IPackageManagerNative.h>  // package info
#include <audio_utils/clock.h>                 // clock conversions
#include <binder/IPCThreadState.h>             // get calling uid
#include <cutils/properties.h>                 // for property_get
#include <mediautils/MemoryLeakTrackUtil.h>
#include <memunreachable/memunreachable.h>
#include <private/android_filesystem_config.h> // UID

namespace android {

using mediametrics::Item;
using mediametrics::startsWith;

// individual records kept in memory: age or count
// age: <= 28 hours (1 1/6 days)
// count: hard limit of # records
// (0 for either of these disables that threshold)
//
static constexpr nsecs_t kMaxRecordAgeNs = 28 * 3600 * NANOS_PER_SECOND;
// 2019/6: average daily per device is currently 375-ish;
// setting this to 2000 is large enough to catch most devices
// we'll lose some data on very very media-active devices, but only for
// the gms collection; statsd will have already covered those for us.
// This also retains enough information to help with bugreports
static constexpr size_t kMaxRecords = 2000;

// max we expire in a single call, to constrain how long we hold the
// mutex, which also constrains how long a client might wait.
static constexpr size_t kMaxExpiredAtOnce = 50;

// TODO: need to look at tuning kMaxRecords and friends for low-memory devices

/* static */
nsecs_t MediaMetricsService::roundTime(nsecs_t timeNs)
{
    return (timeNs + NANOS_PER_SECOND / 2) / NANOS_PER_SECOND * NANOS_PER_SECOND;
}

/* static */
bool MediaMetricsService::useUidForPackage(
        const std::string& package, const std::string& installer)
{
    if (strchr(package.c_str(), '.') == nullptr) {
        return false;  // not of form 'com.whatever...'; assume internal and ok
    } else if (strncmp(package.c_str(), "android.", 8) == 0) {
        return false;  // android.* packages are assumed fine
    } else if (strncmp(installer.c_str(), "com.android.", 12) == 0) {
        return false;  // from play store
    } else if (strncmp(installer.c_str(), "com.google.", 11) == 0) {
        return false;  // some google source
    } else if (strcmp(installer.c_str(), "preload") == 0) {
        return false;  // preloads
    } else {
        return true;  // we're not sure where it came from, use uid only.
    }
}

/* static */
std::pair<std::string, int64_t>
MediaMetricsService::getSanitizedPackageNameAndVersionCode(uid_t uid) {
    // Meyer's singleton, initialized on first access.
    // mUidInfo is locked internally.
    static mediautils::UidInfo uidInfo;

    // get info.
    mediautils::UidInfo::Info info = uidInfo.getInfo(uid);
    if (useUidForPackage(info.package, info.installer)) {
        return { std::to_string(uid), /* versionCode */ 0 };
    } else {
        return { info.package, info.versionCode };
    }
}

MediaMetricsService::MediaMetricsService()
        : mMaxRecords(kMaxRecords),
          mMaxRecordAgeNs(kMaxRecordAgeNs),
          mMaxRecordsExpiredAtOnce(kMaxExpiredAtOnce)
{
    ALOGD("%s", __func__);
}

MediaMetricsService::~MediaMetricsService()
{
    ALOGD("%s", __func__);
    // the class destructor clears anyhow, but we enforce clearing items first.
    mItemsDiscarded += mItems.size();
    mItems.clear();
}

status_t MediaMetricsService::submitInternal(mediametrics::Item *item, bool release)
{
    // calling PID is 0 for one-way calls.
    const pid_t pid = IPCThreadState::self()->getCallingPid();
    const pid_t pid_given = item->getPid();
    const uid_t uid = IPCThreadState::self()->getCallingUid();
    const uid_t uid_given = item->getUid();

    //ALOGD("%s: caller pid=%d uid=%d,  item pid=%d uid=%d", __func__,
    //        (int)pid, (int)uid, (int) pid_given, (int)uid_given);

    bool isTrusted;
    switch (uid) {
    case AID_AUDIOSERVER:
    case AID_BLUETOOTH:
    case AID_CAMERA:
    case AID_DRM:
    case AID_MEDIA:
    case AID_MEDIA_CODEC:
    case AID_MEDIA_EX:
    case AID_MEDIA_DRM:
    // case AID_SHELL: // DEBUG ONLY - used for mediametrics_tests to add new keys
    case AID_SYSTEM:
        // trusted source, only override default values
        isTrusted = true;
        if (uid_given == (uid_t)-1) {
            item->setUid(uid);
        }
        if (pid_given == (pid_t)-1) {
            item->setPid(pid); // if one-way then this is 0.
        }
        break;
    default:
        isTrusted = false;
        item->setPid(pid); // always use calling pid, if one-way then this is 0.
        item->setUid(uid);
        break;
    }

    // Overwrite package name and version if the caller was untrusted or empty
    if (!isTrusted || item->getPkgName().empty()) {
        const uid_t uidItem = item->getUid();
        const auto [ pkgName, version ] =
                MediaMetricsService::getSanitizedPackageNameAndVersionCode(uidItem);
        item->setPkgName(pkgName);
        item->setPkgVersionCode(version);
    }

    ALOGV("%s: isTrusted:%d given uid %d; sanitized uid: %d sanitized pkg: %s "
          "sanitized pkg version: %lld",
          __func__,
          (int)isTrusted,
          uid_given, item->getUid(),
          item->getPkgName().c_str(),
          (long long)item->getPkgVersionCode());

    mItemsSubmitted++;

    // validate the record; we discard if we don't like it
    if (isContentValid(item, isTrusted) == false) {
        if (release) delete item;
        return PERMISSION_DENIED;
    }

    // XXX: if we have a sessionid in the new record, look to make
    // sure it doesn't appear in the finalized list.

    if (item->count() == 0) {
        ALOGV("%s: dropping empty record...", __func__);
        if (release) delete item;
        return BAD_VALUE;
    }

    if (!isTrusted || item->getTimestamp() == 0) {
        // Statsd logs two times for events: ElapsedRealTimeNs (BOOTTIME) and
        // WallClockTimeNs (REALTIME), but currently logs REALTIME to cloud.
        //
        // For consistency and correlation with other logging mechanisms
        // we use REALTIME here.
        const int64_t now = systemTime(SYSTEM_TIME_REALTIME);
        item->setTimestamp(now);
    }

    // now attach either the item or its dup to a const shared pointer
    std::shared_ptr<const mediametrics::Item> sitem(release ? item : item->dup());

    (void)mAudioAnalytics.submit(sitem, isTrusted);

    extern bool dump2Statsd(const std::shared_ptr<const mediametrics::Item>& item);
    (void)dump2Statsd(sitem);  // failure should be logged in function.
    saveItem(sitem);
    return NO_ERROR;
}

status_t MediaMetricsService::dump(int fd, const Vector<String16>& args)
{
    String8 result;

    if (checkCallingPermission(String16("android.permission.DUMP")) == false) {
        result.appendFormat("Permission Denial: "
                "can't dump MediaMetricsService from pid=%d, uid=%d\n",
                IPCThreadState::self()->getCallingPid(),
                IPCThreadState::self()->getCallingUid());
        write(fd, result.string(), result.size());
        return NO_ERROR;
    }

    static const String16 allOption("--all");
    static const String16 clearOption("--clear");
    static const String16 heapOption("--heap");
    static const String16 helpOption("--help");
    static const String16 prefixOption("--prefix");
    static const String16 sinceOption("--since");
    static const String16 unreachableOption("--unreachable");

    bool all = false;
    bool clear = false;
    bool heap = false;
    bool unreachable = false;
    int64_t sinceNs = 0;
    std::string prefix;

    const size_t n = args.size();
    for (size_t i = 0; i < n; i++) {
        if (args[i] == allOption) {
            all = true;
        } else if (args[i] == clearOption) {
            clear = true;
        } else if (args[i] == heapOption) {
            heap = true;
        } else if (args[i] == helpOption) {
            // TODO: consider function area dumping.
            // dumpsys media.metrics audiotrack,codec
            // or dumpsys media.metrics audiotrack codec

            result.append("Recognized parameters:\n");
            result.append("--all         show all records\n");
            result.append("--clear       clear out saved records\n");
            result.append("--heap        show heap usage (top 100)\n");
            result.append("--help        display help\n");
            result.append("--prefix X    process records for component X\n");
            result.append("--since X     X < 0: records from -X seconds in the past\n");
            result.append("              X = 0: ignore\n");
            result.append("              X > 0: records from X seconds since Unix epoch\n");
            result.append("--unreachable show unreachable memory (leaks)\n");
            write(fd, result.string(), result.size());
            return NO_ERROR;
        } else if (args[i] == prefixOption) {
            ++i;
            if (i < n) {
                prefix = String8(args[i]).string();
            }
        } else if (args[i] == sinceOption) {
            ++i;
            if (i < n) {
                String8 value(args[i]);
                char *endp;
                const char *p = value.string();
                const auto sec = (int64_t)strtoll(p, &endp, 10);
                if (endp == p || *endp != '\0' || sec == 0) {
                    sinceNs = 0;
                } else if (sec < 0) {
                    sinceNs = systemTime(SYSTEM_TIME_REALTIME) + sec * NANOS_PER_SECOND;
                } else {
                    sinceNs = sec * NANOS_PER_SECOND;
                }
            }
        } else if (args[i] == unreachableOption) {
            unreachable = true;
        }
    }

    {
        std::lock_guard _l(mLock);

        if (clear) {
            mItemsDiscarded += mItems.size();
            mItems.clear();
            mAudioAnalytics.clear();
        } else {
            result.appendFormat("Dump of the %s process:\n", kServiceName);
            const char *prefixptr = prefix.size() > 0 ? prefix.c_str() : nullptr;
            dumpHeaders(result, sinceNs, prefixptr);
            dumpQueue(result, sinceNs, prefixptr);

            // TODO: maybe consider a better way of dumping audio analytics info.
            const int32_t linesToDump = all ? INT32_MAX : 1000;
            auto [ dumpString, lines ] = mAudioAnalytics.dump(linesToDump, sinceNs, prefixptr);
            result.append(dumpString.c_str());
            if (lines == linesToDump) {
                result.append("-- some lines may be truncated --\n");
            }
        }
    }
    write(fd, result.string(), result.size());

    // Check heap and unreachable memory outside of lock.
    if (heap) {
        dprintf(fd, "\nDumping heap:\n");
        std::string s = dumpMemoryAddresses(100 /* limit */);
        write(fd, s.c_str(), s.size());
    }
    if (unreachable) {
        dprintf(fd, "\nDumping unreachable memory:\n");
        // TODO - should limit be an argument parameter?
        std::string s = GetUnreachableMemoryString(true /* contents */, 100 /* limit */);
        write(fd, s.c_str(), s.size());
    }
    return NO_ERROR;
}

// dump headers
void MediaMetricsService::dumpHeaders(String8 &result, int64_t sinceNs, const char* prefix)
{
    if (mediametrics::Item::isEnabled()) {
        result.append("Metrics gathering: enabled\n");
    } else {
        result.append("Metrics gathering: DISABLED via property\n");
    }
    result.appendFormat(
            "Since Boot: Submissions: %lld Accepted: %lld\n",
            (long long)mItemsSubmitted.load(), (long long)mItemsFinalized);
    result.appendFormat(
            "Records Discarded: %lld (by Count: %lld by Expiration: %lld)\n",
            (long long)mItemsDiscarded, (long long)mItemsDiscardedCount,
            (long long)mItemsDiscardedExpire);
    if (prefix != nullptr) {
        result.appendFormat("Restricting to prefix %s", prefix);
    }
    if (sinceNs != 0) {
        result.appendFormat(
            "Emitting Queue entries more recent than: %lld\n",
            (long long)sinceNs);
    }
}

// TODO: should prefix be a set<string>?
void MediaMetricsService::dumpQueue(String8 &result, int64_t sinceNs, const char* prefix)
{
    if (mItems.empty()) {
        result.append("empty\n");
        return;
    }

    int slot = 0;
    for (const auto &item : mItems) {         // TODO: consider std::lower_bound() on mItems
        if (item->getTimestamp() < sinceNs) { // sinceNs == 0 means all items shown
            continue;
        }
        if (prefix != nullptr && !startsWith(item->getKey(), prefix)) {
            ALOGV("%s: omit '%s', it's not '%s'",
                    __func__, item->getKey().c_str(), prefix);
            continue;
        }
        result.appendFormat("%5d: %s\n", slot, item->toString().c_str());
        slot++;
    }
}

//
// Our Cheap in-core, non-persistent records management.

// if item != NULL, it's the item we just inserted
// true == more items eligible to be recovered
bool MediaMetricsService::expirations(const std::shared_ptr<const mediametrics::Item>& item)
{
    bool more = false;

    // check queue size
    size_t overlimit = 0;
    if (mMaxRecords > 0 && mItems.size() > mMaxRecords) {
        overlimit = mItems.size() - mMaxRecords;
        if (overlimit > mMaxRecordsExpiredAtOnce) {
            more = true;
            overlimit = mMaxRecordsExpiredAtOnce;
        }
    }

    // check queue times
    size_t expired = 0;
    if (!more && mMaxRecordAgeNs > 0) {
        const nsecs_t now = systemTime(SYSTEM_TIME_REALTIME);
        // we check one at a time, skip search would be more efficient.
        size_t i = overlimit;
        for (; i < mItems.size(); ++i) {
            auto &oitem = mItems[i];
            nsecs_t when = oitem->getTimestamp();
            if (oitem.get() == item.get()) {
                break;
            }
            if (now > when && (now - when) <= mMaxRecordAgeNs) {
                break; // Note SYSTEM_TIME_REALTIME may not be monotonic.
            }
            if (i >= mMaxRecordsExpiredAtOnce) {
                // this represents "one too many"; tell caller there are
                // more to be reclaimed.
                more = true;
                break;
            }
        }
        expired = i - overlimit;
    }

    if (const size_t toErase = overlimit + expired;
            toErase > 0) {
        mItemsDiscardedCount += overlimit;
        mItemsDiscardedExpire += expired;
        mItemsDiscarded += toErase;
        mItems.erase(mItems.begin(), mItems.begin() + toErase); // erase from front
    }
    return more;
}

void MediaMetricsService::processExpirations()
{
    bool more;
    do {
        sleep(1);
        std::lock_guard _l(mLock);
        more = expirations(nullptr);
    } while (more);
}

void MediaMetricsService::saveItem(const std::shared_ptr<const mediametrics::Item>& item)
{
    std::lock_guard _l(mLock);
    // we assume the items are roughly in time order.
    mItems.emplace_back(item);
    ++mItemsFinalized;
    if (expirations(item)
            && (!mExpireFuture.valid()
               || mExpireFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)) {
        mExpireFuture = std::async(std::launch::async, [this] { processExpirations(); });
    }
}

/* static */
bool MediaMetricsService::isContentValid(const mediametrics::Item *item, bool isTrusted)
{
    if (isTrusted) return true;
    // untrusted uids can only send us a limited set of keys
    const std::string &key = item->getKey();
    if (startsWith(key, "audio.")) return true;
    if (startsWith(key, "drm.vendor.")) return true;
    // the list of allowedKey uses statsd_handlers
    // in iface_statsd.cpp as reference
    // drmmanager is from a trusted uid, therefore not needed here
    for (const char *allowedKey : {
                                     // legacy audio
                                     "audiopolicy",
                                     "audiorecord",
                                     "audiothread",
                                     "audiotrack",
                                     // other media
                                     "codec",
                                     "extractor",
                                     "mediadrm",
                                     "mediaparser",
                                     "nuplayer",
                                 }) {
        if (key == allowedKey) {
            return true;
        }
    }
    ALOGD("%s: invalid key: %s", __func__, item->toString().c_str());
    return false;
}

// are we rate limited, normally false
bool MediaMetricsService::isRateLimited(mediametrics::Item *) const
{
    return false;
}

} // namespace android
