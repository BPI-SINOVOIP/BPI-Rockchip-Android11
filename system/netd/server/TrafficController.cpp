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

#define LOG_TAG "TrafficController"
#include <inttypes.h>
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include <linux/sock_diag.h>
#include <linux/unistd.h>
#include <net/if.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <mutex>
#include <unordered_set>
#include <vector>

#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>
#include <netdutils/StatusOr.h>

#include <netdutils/Misc.h>
#include <netdutils/Syscalls.h>
#include <processgroup/processgroup.h>
#include "TrafficController.h"
#include "bpf/BpfMap.h"

#include "FirewallController.h"
#include "InterfaceController.h"
#include "NetlinkListener.h"
#include "netdutils/DumpWriter.h"
#include "qtaguid/qtaguid.h"

using namespace android::bpf;  // NOLINT(google-build-using-namespace): grandfathered

namespace android {
namespace net {

using base::StringPrintf;
using base::unique_fd;
using netdutils::DumpWriter;
using netdutils::extract;
using netdutils::ScopedIndent;
using netdutils::Slice;
using netdutils::sSyscalls;
using netdutils::Status;
using netdutils::statusFromErrno;
using netdutils::StatusOr;
using netdutils::status::ok;

constexpr int kSockDiagMsgType = SOCK_DIAG_BY_FAMILY;
constexpr int kSockDiagDoneMsgType = NLMSG_DONE;
constexpr int PER_UID_STATS_ENTRIES_LIMIT = 500;
// At most 90% of the stats map may be used by tagged traffic entries. This ensures
// that 10% of the map is always available to count untagged traffic, one entry per UID.
// Otherwise, apps would be able to avoid data usage accounting entirely by filling up the
// map with tagged traffic entries.
constexpr int TOTAL_UID_STATS_ENTRIES_LIMIT = STATS_MAP_SIZE * 0.9;

static_assert(BPF_PERMISSION_INTERNET == INetd::PERMISSION_INTERNET,
              "Mismatch between BPF and AIDL permissions: PERMISSION_INTERNET");
static_assert(BPF_PERMISSION_UPDATE_DEVICE_STATS == INetd::PERMISSION_UPDATE_DEVICE_STATS,
              "Mismatch between BPF and AIDL permissions: PERMISSION_UPDATE_DEVICE_STATS");
static_assert(STATS_MAP_SIZE - TOTAL_UID_STATS_ENTRIES_LIMIT > 100,
              "The limit for stats map is to high, stats data may be lost due to overflow");

#define FLAG_MSG_TRANS(result, flag, value) \
    do {                                    \
        if ((value) & (flag)) {             \
            (result).append(" " #flag);     \
            (value) &= ~(flag);             \
        }                                   \
    } while (0)

const std::string uidMatchTypeToString(uint8_t match) {
    std::string matchType;
    FLAG_MSG_TRANS(matchType, HAPPY_BOX_MATCH, match);
    FLAG_MSG_TRANS(matchType, PENALTY_BOX_MATCH, match);
    FLAG_MSG_TRANS(matchType, DOZABLE_MATCH, match);
    FLAG_MSG_TRANS(matchType, STANDBY_MATCH, match);
    FLAG_MSG_TRANS(matchType, POWERSAVE_MATCH, match);
    FLAG_MSG_TRANS(matchType, IIF_MATCH, match);
    if (match) {
        return StringPrintf("Unknown match: %u", match);
    }
    return matchType;
}

bool TrafficController::hasUpdateDeviceStatsPermission(uid_t uid) {
    // This implementation is the same logic as method ActivityManager#checkComponentPermission.
    // It implies that the calling uid can never be the same as PER_USER_RANGE.
    uint32_t appId = uid % PER_USER_RANGE;
    return ((appId == AID_ROOT) || (appId == AID_SYSTEM) ||
            mPrivilegedUser.find(appId) != mPrivilegedUser.end());
}

const std::string UidPermissionTypeToString(int permission) {
    if (permission == INetd::PERMISSION_NONE) {
        return "PERMISSION_NONE";
    }
    if (permission == INetd::PERMISSION_UNINSTALLED) {
        // This should never appear in the map, complain loudly if it does.
        return "PERMISSION_UNINSTALLED error!";
    }
    std::string permissionType;
    FLAG_MSG_TRANS(permissionType, BPF_PERMISSION_INTERNET, permission);
    FLAG_MSG_TRANS(permissionType, BPF_PERMISSION_UPDATE_DEVICE_STATS, permission);
    if (permission) {
        return StringPrintf("Unknown permission: %u", permission);
    }
    return permissionType;
}

StatusOr<std::unique_ptr<NetlinkListenerInterface>> TrafficController::makeSkDestroyListener() {
    const auto& sys = sSyscalls.get();
    ASSIGN_OR_RETURN(auto event, sys.eventfd(0, EFD_CLOEXEC));
    const int domain = AF_NETLINK;
    const int type = SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK;
    const int protocol = NETLINK_INET_DIAG;
    ASSIGN_OR_RETURN(auto sock, sys.socket(domain, type, protocol));

    // TODO: if too many sockets are closed too quickly, we can overflow the socket buffer, and
    // some entries in mCookieTagMap will not be freed. In order to fix this we would need to
    // periodically dump all sockets and remove the tag entries for sockets that have been closed.
    // For now, set a large-enough buffer that we can close hundreds of sockets without getting
    // ENOBUFS and leaking mCookieTagMap entries.
    int rcvbuf = 512 * 1024;
    auto ret = sys.setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    if (!ret.ok()) {
        ALOGW("Failed to set SkDestroyListener buffer size to %d: %s", rcvbuf, ret.msg().c_str());
    }

    sockaddr_nl addr = {
        .nl_family = AF_NETLINK,
        .nl_groups = 1 << (SKNLGRP_INET_TCP_DESTROY - 1) | 1 << (SKNLGRP_INET_UDP_DESTROY - 1) |
                     1 << (SKNLGRP_INET6_TCP_DESTROY - 1) | 1 << (SKNLGRP_INET6_UDP_DESTROY - 1)};
    RETURN_IF_NOT_OK(sys.bind(sock, addr));

    const sockaddr_nl kernel = {.nl_family = AF_NETLINK};
    RETURN_IF_NOT_OK(sys.connect(sock, kernel));

    std::unique_ptr<NetlinkListenerInterface> listener =
            std::make_unique<NetlinkListener>(std::move(event), std::move(sock), "SkDestroyListen");

    return listener;
}

TrafficController::TrafficController()
    : mBpfEnabled(isBpfSupported()),
      mPerUidStatsEntriesLimit(PER_UID_STATS_ENTRIES_LIMIT),
      mTotalUidStatsEntriesLimit(TOTAL_UID_STATS_ENTRIES_LIMIT) {}

TrafficController::TrafficController(uint32_t perUidLimit, uint32_t totalLimit)
    : mBpfEnabled(isBpfSupported()),
      mPerUidStatsEntriesLimit(perUidLimit),
      mTotalUidStatsEntriesLimit(totalLimit) {}

Status TrafficController::initMaps() {
    std::lock_guard guard(mMutex);

    RETURN_IF_NOT_OK(mCookieTagMap.init(COOKIE_TAG_MAP_PATH));
    RETURN_IF_NOT_OK(mUidCounterSetMap.init(UID_COUNTERSET_MAP_PATH));
    RETURN_IF_NOT_OK(mAppUidStatsMap.init(APP_UID_STATS_MAP_PATH));
    RETURN_IF_NOT_OK(mStatsMapA.init(STATS_MAP_A_PATH));
    RETURN_IF_NOT_OK(mStatsMapB.init(STATS_MAP_B_PATH));
    RETURN_IF_NOT_OK(mIfaceIndexNameMap.init(IFACE_INDEX_NAME_MAP_PATH));
    RETURN_IF_NOT_OK(mIfaceStatsMap.init(IFACE_STATS_MAP_PATH));

    RETURN_IF_NOT_OK(mConfigurationMap.init(CONFIGURATION_MAP_PATH));
    RETURN_IF_NOT_OK(
            mConfigurationMap.writeValue(UID_RULES_CONFIGURATION_KEY, DEFAULT_CONFIG, BPF_ANY));
    RETURN_IF_NOT_OK(mConfigurationMap.writeValue(CURRENT_STATS_MAP_CONFIGURATION_KEY, SELECT_MAP_A,
                                                  BPF_ANY));

    RETURN_IF_NOT_OK(mUidOwnerMap.init(UID_OWNER_MAP_PATH));
    RETURN_IF_NOT_OK(mUidOwnerMap.clear());
    RETURN_IF_NOT_OK(mUidPermissionMap.init(UID_PERMISSION_MAP_PATH));

    return netdutils::status::ok;
}

static Status attachProgramToCgroup(const char* programPath, const unique_fd& cgroupFd,
                                    bpf_attach_type type) {
    unique_fd cgroupProg(retrieveProgram(programPath));
    if (cgroupProg == -1) {
        int ret = errno;
        ALOGE("Failed to get program from %s: %s", programPath, strerror(ret));
        return statusFromErrno(ret, "cgroup program get failed");
    }
    if (android::bpf::attachProgram(type, cgroupProg, cgroupFd)) {
        int ret = errno;
        ALOGE("Program from %s attach failed: %s", programPath, strerror(ret));
        return statusFromErrno(ret, "program attach failed");
    }
    return netdutils::status::ok;
}

static Status initPrograms() {
    std::string cg2_path;

    if (!CgroupGetControllerPath(CGROUPV2_CONTROLLER_NAME, &cg2_path)) {
         int ret = errno;
         ALOGE("Failed to find cgroup v2 root");
         return statusFromErrno(ret, "Failed to find cgroup v2 root");
    }

    unique_fd cg_fd(open(cg2_path.c_str(), O_DIRECTORY | O_RDONLY | O_CLOEXEC));
    if (cg_fd == -1) {
        int ret = errno;
        ALOGE("Failed to open the cgroup directory: %s", strerror(ret));
        return statusFromErrno(ret, "Open the cgroup directory failed");
    }
    RETURN_IF_NOT_OK(attachProgramToCgroup(BPF_EGRESS_PROG_PATH, cg_fd, BPF_CGROUP_INET_EGRESS));
    RETURN_IF_NOT_OK(attachProgramToCgroup(BPF_INGRESS_PROG_PATH, cg_fd, BPF_CGROUP_INET_INGRESS));

    // For the devices that support cgroup socket filter, the socket filter
    // should be loaded successfully by bpfloader. So we attach the filter to
    // cgroup if the program is pinned properly.
    // TODO: delete the if statement once all devices should support cgroup
    // socket filter (ie. the minimum kernel version required is 4.14).
    if (!access(CGROUP_SOCKET_PROG_PATH, F_OK)) {
        RETURN_IF_NOT_OK(
                attachProgramToCgroup(CGROUP_SOCKET_PROG_PATH, cg_fd, BPF_CGROUP_INET_SOCK_CREATE));
    }
    return netdutils::status::ok;
}

Status TrafficController::start() {
    if (!mBpfEnabled) {
        return netdutils::status::ok;
    }

    /* When netd restarts from a crash without total system reboot, the program
     * is still attached to the cgroup, detach it so the program can be freed
     * and we can load and attach new program into the target cgroup.
     *
     * TODO: Scrape existing socket when run-time restart and clean up the map
     * if the socket no longer exist
     */

    RETURN_IF_NOT_OK(initMaps());

    RETURN_IF_NOT_OK(initPrograms());

    // Fetch the list of currently-existing interfaces. At this point NetlinkHandler is
    // already running, so it will call addInterface() when any new interface appears.
    std::map<std::string, uint32_t> ifacePairs;
    ASSIGN_OR_RETURN(ifacePairs, InterfaceController::getIfaceList());
    for (const auto& ifacePair:ifacePairs) {
        addInterface(ifacePair.first.c_str(), ifacePair.second);
    }

    auto result = makeSkDestroyListener();
    if (!isOk(result)) {
        ALOGE("Unable to create SkDestroyListener: %s", toString(result).c_str());
    } else {
        mSkDestroyListener = std::move(result.value());
    }
    // Rx handler extracts nfgenmsg looks up and invokes registered dispatch function.
    const auto rxHandler = [this](const nlmsghdr&, const Slice msg) {
        std::lock_guard guard(mMutex);
        inet_diag_msg diagmsg = {};
        if (extract(msg, diagmsg) < sizeof(inet_diag_msg)) {
            ALOGE("Unrecognized netlink message: %s", toString(msg).c_str());
            return;
        }
        uint64_t sock_cookie = static_cast<uint64_t>(diagmsg.id.idiag_cookie[0]) |
                               (static_cast<uint64_t>(diagmsg.id.idiag_cookie[1]) << 32);

        Status s = mCookieTagMap.deleteValue(sock_cookie);
        if (!isOk(s) && s.code() != ENOENT) {
            ALOGE("Failed to delete cookie %" PRIx64 ": %s", sock_cookie, toString(s).c_str());
            return;
        }
    };
    expectOk(mSkDestroyListener->subscribe(kSockDiagMsgType, rxHandler));

    // In case multiple netlink message comes in as a stream, we need to handle the rxDone message
    // properly.
    const auto rxDoneHandler = [](const nlmsghdr&, const Slice msg) {
        // Ignore NLMSG_DONE  messages
        inet_diag_msg diagmsg = {};
        extract(msg, diagmsg);
    };
    expectOk(mSkDestroyListener->subscribe(kSockDiagDoneMsgType, rxDoneHandler));

    return netdutils::status::ok;
}

int TrafficController::tagSocket(int sockFd, uint32_t tag, uid_t uid, uid_t callingUid) {
    std::lock_guard guard(mMutex);
    if (uid != callingUid && !hasUpdateDeviceStatsPermission(callingUid)) {
        return -EPERM;
    }

    if (!mBpfEnabled) {
        if (legacy_tagSocket(sockFd, tag, uid)) return -errno;
        return 0;
    }

    uint64_t sock_cookie = getSocketCookie(sockFd);
    if (sock_cookie == NONEXISTENT_COOKIE) return -errno;
    UidTagValue newKey = {.uid = (uint32_t)uid, .tag = tag};

    uint32_t totalEntryCount = 0;
    uint32_t perUidEntryCount = 0;
    // Now we go through the stats map and count how many entries are associated
    // with target uid. If the uid entry hit the limit for each uid, we block
    // the request to prevent the map from overflow. It is safe here to iterate
    // over the map since when mMutex is hold, system server cannot toggle
    // the live stats map and clean it. So nobody can delete entries from the map.
    const auto countUidStatsEntries = [uid, &totalEntryCount, &perUidEntryCount](
                                              const StatsKey& key,
                                              const BpfMap<StatsKey, StatsValue>&) {
        if (key.uid == uid) {
            perUidEntryCount++;
        }
        totalEntryCount++;
        return base::Result<void>();
    };
    auto configuration = mConfigurationMap.readValue(CURRENT_STATS_MAP_CONFIGURATION_KEY);
    if (!configuration.ok()) {
        ALOGE("Failed to get current configuration: %s, fd: %d",
              strerror(configuration.error().code()), mConfigurationMap.getMap().get());
        return -configuration.error().code();
    }
    if (configuration.value() != SELECT_MAP_A && configuration.value() != SELECT_MAP_B) {
        ALOGE("unknown configuration value: %d", configuration.value());
        return -EINVAL;
    }

    BpfMap<StatsKey, StatsValue>& currentMap =
            (configuration.value() == SELECT_MAP_A) ? mStatsMapA : mStatsMapB;
    base::Result<void> res = currentMap.iterate(countUidStatsEntries);
    if (!res.ok()) {
        ALOGE("Failed to count the stats entry in map %d: %s", currentMap.getMap().get(),
              strerror(res.error().code()));
        return -res.error().code();
    }

    if (totalEntryCount > mTotalUidStatsEntriesLimit ||
        perUidEntryCount > mPerUidStatsEntriesLimit) {
        ALOGE("Too many stats entries in the map, total count: %u, uid(%u) count: %u, blocking tag"
              " request to prevent map overflow",
              totalEntryCount, uid, perUidEntryCount);
        return -EMFILE;
    }
    // Update the tag information of a socket to the cookieUidMap. Use BPF_ANY
    // flag so it will insert a new entry to the map if that value doesn't exist
    // yet. And update the tag if there is already a tag stored. Since the eBPF
    // program in kernel only read this map, and is protected by rcu read lock. It
    // should be fine to cocurrently update the map while eBPF program is running.
    res = mCookieTagMap.writeValue(sock_cookie, newKey, BPF_ANY);
    if (!res.ok()) {
        ALOGE("Failed to tag the socket: %s, fd: %d", strerror(res.error().code()),
              mCookieTagMap.getMap().get());
        return -res.error().code();
    }
    return 0;
}

int TrafficController::untagSocket(int sockFd) {
    std::lock_guard guard(mMutex);
    if (!mBpfEnabled) {
        if (legacy_untagSocket(sockFd)) return -errno;
        return 0;
    }
    uint64_t sock_cookie = getSocketCookie(sockFd);

    if (sock_cookie == NONEXISTENT_COOKIE) return -errno;
    base::Result<void> res = mCookieTagMap.deleteValue(sock_cookie);
    if (!res.ok()) {
        ALOGE("Failed to untag socket: %s\n", strerror(res.error().code()));
        return -res.error().code();
    }
    return 0;
}

int TrafficController::setCounterSet(int counterSetNum, uid_t uid, uid_t callingUid) {
    if (counterSetNum < 0 || counterSetNum >= OVERFLOW_COUNTERSET) return -EINVAL;

    std::lock_guard guard(mMutex);
    if (!hasUpdateDeviceStatsPermission(callingUid)) return -EPERM;

    if (!mBpfEnabled) {
        if (legacy_setCounterSet(counterSetNum, uid)) return -errno;
        return 0;
    }

    // The default counter set for all uid is 0, so deleting the current counterset for that uid
    // will automatically set it to 0.
    if (counterSetNum == 0) {
        Status res = mUidCounterSetMap.deleteValue(uid);
        if (isOk(res) || (!isOk(res) && res.code() == ENOENT)) {
            return 0;
        } else {
            ALOGE("Failed to delete the counterSet: %s\n", strerror(res.code()));
            return -res.code();
        }
    }
    uint8_t tmpCounterSetNum = (uint8_t)counterSetNum;
    Status res = mUidCounterSetMap.writeValue(uid, tmpCounterSetNum, BPF_ANY);
    if (!isOk(res)) {
        ALOGE("Failed to set the counterSet: %s, fd: %d", strerror(res.code()),
              mUidCounterSetMap.getMap().get());
        return -res.code();
    }
    return 0;
}

// This method only get called by system_server when an app get uinstalled, it
// is called inside removeUidsLocked() while holding mStatsLock. So it is safe
// to iterate and modify the stats maps.
int TrafficController::deleteTagData(uint32_t tag, uid_t uid, uid_t callingUid) {
    std::lock_guard guard(mMutex);
    if (!hasUpdateDeviceStatsPermission(callingUid)) return -EPERM;

    if (!mBpfEnabled) {
        if (legacy_deleteTagData(tag, uid)) return -errno;
        return 0;
    }

    // First we go through the cookieTagMap to delete the target uid tag combination. Or delete all
    // the tags related to the uid if the tag is 0.
    const auto deleteMatchedCookieEntries = [uid, tag](const uint64_t& key,
                                                       const UidTagValue& value,
                                                       BpfMap<uint64_t, UidTagValue>& map) {
        if (value.uid == uid && (value.tag == tag || tag == 0)) {
            auto res = map.deleteValue(key);
            if (res.ok() || (res.error().code() == ENOENT)) {
                return base::Result<void>();
            }
            ALOGE("Failed to delete data(cookie = %" PRIu64 "): %s\n", key,
                  strerror(res.error().code()));
        }
        // Move forward to next cookie in the map.
        return base::Result<void>();
    };
    mCookieTagMap.iterateWithValue(deleteMatchedCookieEntries);
    // Now we go through the Tag stats map and delete the data entry with correct uid and tag
    // combination. Or all tag stats under that uid if the target tag is 0.
    const auto deleteMatchedUidTagEntries = [uid, tag](const StatsKey& key,
                                                       BpfMap<StatsKey, StatsValue>& map) {
        if (key.uid == uid && (key.tag == tag || tag == 0)) {
            auto res = map.deleteValue(key);
            if (res.ok() || (res.error().code() == ENOENT)) {
                //Entry is deleted, use the current key to get a new nextKey;
                return base::Result<void>();
            }
            ALOGE("Failed to delete data(uid=%u, tag=%u): %s\n", key.uid, key.tag,
                  strerror(res.error().code()));
        }
        return base::Result<void>();
    };
    mStatsMapB.iterate(deleteMatchedUidTagEntries);
    mStatsMapA.iterate(deleteMatchedUidTagEntries);
    // If the tag is not zero, we already deleted all the data entry required. If tag is 0, we also
    // need to delete the stats stored in uidStatsMap and counterSet map.
    if (tag != 0) return 0;

    auto res = mUidCounterSetMap.deleteValue(uid);
    if (!res.ok() && res.error().code() != ENOENT) {
        ALOGE("Failed to delete counterSet data(uid=%u, tag=%u): %s\n", uid, tag,
              strerror(res.error().code()));
    }

    auto deleteAppUidStatsEntry = [uid](const uint32_t& key,
                                        BpfMap<uint32_t, StatsValue>& map) -> base::Result<void> {
        if (key == uid) {
            auto res = map.deleteValue(key);
            if (res.ok() || (res.error().code() == ENOENT)) {
                return {};
            }
            ALOGE("Failed to delete data(uid=%u): %s", key, strerror(res.error().code()));
        }
        return {};
    };
    mAppUidStatsMap.iterate(deleteAppUidStatsEntry);
    return 0;
}

int TrafficController::addInterface(const char* name, uint32_t ifaceIndex) {
    if (!mBpfEnabled) return 0;

    IfaceValue iface;
    if (ifaceIndex == 0) {
        ALOGE("Unknown interface %s(%d)", name, ifaceIndex);
        return -1;
    }

    strlcpy(iface.name, name, sizeof(IfaceValue));
    Status res = mIfaceIndexNameMap.writeValue(ifaceIndex, iface, BPF_ANY);
    if (!isOk(res)) {
        ALOGE("Failed to add iface %s(%d): %s", name, ifaceIndex, strerror(res.code()));
        return -res.code();
    }
    return 0;
}

Status TrafficController::updateOwnerMapEntry(UidOwnerMatchType match, uid_t uid, FirewallRule rule,
                                              FirewallType type) {
    std::lock_guard guard(mMutex);
    if ((rule == ALLOW && type == WHITELIST) || (rule == DENY && type == BLACKLIST)) {
        RETURN_IF_NOT_OK(addRule(mUidOwnerMap, uid, match));
    } else if ((rule == ALLOW && type == BLACKLIST) || (rule == DENY && type == WHITELIST)) {
        RETURN_IF_NOT_OK(removeRule(mUidOwnerMap, uid, match));
    } else {
        //Cannot happen.
        return statusFromErrno(EINVAL, "");
    }
    return netdutils::status::ok;
}

UidOwnerMatchType TrafficController::jumpOpToMatch(BandwidthController::IptJumpOp jumpHandling) {
    switch (jumpHandling) {
        case BandwidthController::IptJumpReject:
            return PENALTY_BOX_MATCH;
        case BandwidthController::IptJumpReturn:
            return HAPPY_BOX_MATCH;
        case BandwidthController::IptJumpNoAdd:
            return NO_MATCH;
    }
}

Status TrafficController::removeRule(BpfMap<uint32_t, UidOwnerValue>& map, uint32_t uid,
                                     UidOwnerMatchType match) {
    auto oldMatch = map.readValue(uid);
    if (oldMatch.ok()) {
        UidOwnerValue newMatch = {
                .iif = (match == IIF_MATCH) ? 0 : oldMatch.value().iif,
                .rule = static_cast<uint8_t>(oldMatch.value().rule & ~match),
        };
        if (newMatch.rule == 0) {
            RETURN_IF_NOT_OK(map.deleteValue(uid));
        } else {
            RETURN_IF_NOT_OK(map.writeValue(uid, newMatch, BPF_ANY));
        }
    } else {
        return statusFromErrno(ENOENT, StringPrintf("uid: %u does not exist in map", uid));
    }
    return netdutils::status::ok;
}

Status TrafficController::addRule(BpfMap<uint32_t, UidOwnerValue>& map, uint32_t uid,
                                  UidOwnerMatchType match, uint32_t iif) {
    // iif should be non-zero if and only if match == MATCH_IIF
    if (match == IIF_MATCH && iif == 0) {
        return statusFromErrno(EINVAL, "Interface match must have nonzero interface index");
    } else if (match != IIF_MATCH && iif != 0) {
        return statusFromErrno(EINVAL, "Non-interface match must have zero interface index");
    }
    auto oldMatch = map.readValue(uid);
    if (oldMatch.ok()) {
        UidOwnerValue newMatch = {
                .iif = iif ? iif : oldMatch.value().iif,
                .rule = static_cast<uint8_t>(oldMatch.value().rule | match),
        };
        RETURN_IF_NOT_OK(map.writeValue(uid, newMatch, BPF_ANY));
    } else {
        UidOwnerValue newMatch = {
                .iif = iif,
                .rule = static_cast<uint8_t>(match),
        };
        RETURN_IF_NOT_OK(map.writeValue(uid, newMatch, BPF_ANY));
    }
    return netdutils::status::ok;
}

Status TrafficController::updateUidOwnerMap(const std::vector<std::string>& appStrUids,
                                            BandwidthController::IptJumpOp jumpHandling,
                                            BandwidthController::IptOp op) {
    std::lock_guard guard(mMutex);
    UidOwnerMatchType match = jumpOpToMatch(jumpHandling);
    if (match == NO_MATCH) {
        return statusFromErrno(
                EINVAL, StringPrintf("invalid IptJumpOp: %d, command: %d", jumpHandling, match));
    }
    for (const auto& appStrUid : appStrUids) {
        char* endPtr;
        long uid = strtol(appStrUid.c_str(), &endPtr, 10);
        if ((errno == ERANGE && (uid == LONG_MAX || uid == LONG_MIN)) ||
            (endPtr == appStrUid.c_str()) || (*endPtr != '\0')) {
               return statusFromErrno(errno, "invalid uid string:" + appStrUid);
        }

        if (op == BandwidthController::IptOpDelete) {
            RETURN_IF_NOT_OK(removeRule(mUidOwnerMap, uid, match));
        } else if (op == BandwidthController::IptOpInsert) {
            RETURN_IF_NOT_OK(addRule(mUidOwnerMap, uid, match));
        } else {
            // Cannot happen.
            return statusFromErrno(EINVAL, StringPrintf("invalid IptOp: %d, %d", op, match));
        }
    }
    return netdutils::status::ok;
}

int TrafficController::changeUidOwnerRule(ChildChain chain, uid_t uid, FirewallRule rule,
                                          FirewallType type) {
    if (!mBpfEnabled) {
        ALOGE("bpf is not set up, should use iptables rule");
        return -ENOSYS;
    }
    Status res;
    switch (chain) {
        case DOZABLE:
            res = updateOwnerMapEntry(DOZABLE_MATCH, uid, rule, type);
            break;
        case STANDBY:
            res = updateOwnerMapEntry(STANDBY_MATCH, uid, rule, type);
            break;
        case POWERSAVE:
            res = updateOwnerMapEntry(POWERSAVE_MATCH, uid, rule, type);
            break;
        case NONE:
        default:
            return -EINVAL;
    }
    if (!isOk(res)) {
        ALOGE("change uid(%u) rule of %d failed: %s, rule: %d, type: %d", uid, chain,
              res.msg().c_str(), rule, type);
        return -res.code();
    }
    return 0;
}

Status TrafficController::replaceRulesInMap(const UidOwnerMatchType match,
                                            const std::vector<int32_t>& uids) {
    std::lock_guard guard(mMutex);
    std::set<int32_t> uidSet(uids.begin(), uids.end());
    std::vector<uint32_t> uidsToDelete;
    auto getUidsToDelete = [&uidsToDelete, &uidSet](const uint32_t& key,
                                                    const BpfMap<uint32_t, UidOwnerValue>&) {
        if (uidSet.find((int32_t) key) == uidSet.end()) {
            uidsToDelete.push_back(key);
        }
        return base::Result<void>();
    };
    RETURN_IF_NOT_OK(mUidOwnerMap.iterate(getUidsToDelete));

    for(auto uid : uidsToDelete) {
        RETURN_IF_NOT_OK(removeRule(mUidOwnerMap, uid, match));
    }

    for (auto uid : uids) {
        RETURN_IF_NOT_OK(addRule(mUidOwnerMap, uid, match));
    }
    return netdutils::status::ok;
}

Status TrafficController::addUidInterfaceRules(const int iif,
                                               const std::vector<int32_t>& uidsToAdd) {
    if (!mBpfEnabled) {
        ALOGW("UID ingress interface filtering not possible without BPF owner match");
        return statusFromErrno(EOPNOTSUPP, "eBPF not supported");
    }
    if (!iif) {
        return statusFromErrno(EINVAL, "Interface rule must specify interface");
    }
    std::lock_guard guard(mMutex);

    for (auto uid : uidsToAdd) {
        netdutils::Status result = addRule(mUidOwnerMap, uid, IIF_MATCH, iif);
        if (!isOk(result)) {
            ALOGW("addRule failed(%d): uid=%d iif=%d", result.code(), uid, iif);
        }
    }
    return netdutils::status::ok;
}

Status TrafficController::removeUidInterfaceRules(const std::vector<int32_t>& uidsToDelete) {
    if (!mBpfEnabled) {
        ALOGW("UID ingress interface filtering not possible without BPF owner match");
        return statusFromErrno(EOPNOTSUPP, "eBPF not supported");
    }
    std::lock_guard guard(mMutex);

    for (auto uid : uidsToDelete) {
        netdutils::Status result = removeRule(mUidOwnerMap, uid, IIF_MATCH);
        if (!isOk(result)) {
            ALOGW("removeRule failed(%d): uid=%d", result.code(), uid);
        }
    }
    return netdutils::status::ok;
}

int TrafficController::replaceUidOwnerMap(const std::string& name, bool isWhitelist __unused,
                                          const std::vector<int32_t>& uids) {
    // FirewallRule rule = isWhitelist ? ALLOW : DENY;
    // FirewallType type = isWhitelist ? WHITELIST : BLACKLIST;
    Status res;
    if (!name.compare(FirewallController::LOCAL_DOZABLE)) {
        res = replaceRulesInMap(DOZABLE_MATCH, uids);
    } else if (!name.compare(FirewallController::LOCAL_STANDBY)) {
        res = replaceRulesInMap(STANDBY_MATCH, uids);
    } else if (!name.compare(FirewallController::LOCAL_POWERSAVE)) {
        res = replaceRulesInMap(POWERSAVE_MATCH, uids);
    } else {
        ALOGE("unknown chain name: %s", name.c_str());
        return -EINVAL;
    }
    if (!isOk(res)) {
        ALOGE("Failed to clean up chain: %s: %s", name.c_str(), res.msg().c_str());
        return -res.code();
    }
    return 0;
}

int TrafficController::toggleUidOwnerMap(ChildChain chain, bool enable) {
    std::lock_guard guard(mMutex);
    uint32_t key = UID_RULES_CONFIGURATION_KEY;
    auto oldConfiguration = mConfigurationMap.readValue(key);
    if (!oldConfiguration.ok()) {
        ALOGE("Cannot read the old configuration from map: %s",
              oldConfiguration.error().message().c_str());
        return -oldConfiguration.error().code();
    }
    Status res;
    BpfConfig newConfiguration;
    uint8_t match;
    switch (chain) {
        case DOZABLE:
            match = DOZABLE_MATCH;
            break;
        case STANDBY:
            match = STANDBY_MATCH;
            break;
        case POWERSAVE:
            match = POWERSAVE_MATCH;
            break;
        default:
            return -EINVAL;
    }
    newConfiguration =
            enable ? (oldConfiguration.value() | match) : (oldConfiguration.value() & (~match));
    res = mConfigurationMap.writeValue(key, newConfiguration, BPF_EXIST);
    if (!isOk(res)) {
        ALOGE("Failed to toggleUidOwnerMap(%d): %s", chain, res.msg().c_str());
    }
    return -res.code();
}

bool TrafficController::getBpfEnabled() {
    return mBpfEnabled;
}

Status TrafficController::swapActiveStatsMap() {
    std::lock_guard guard(mMutex);

    if (!mBpfEnabled) {
        return statusFromErrno(EOPNOTSUPP, "This device doesn't have eBPF support");
    }

    uint32_t key = CURRENT_STATS_MAP_CONFIGURATION_KEY;
    auto oldConfiguration = mConfigurationMap.readValue(key);
    if (!oldConfiguration.ok()) {
        ALOGE("Cannot read the old configuration from map: %s",
              oldConfiguration.error().message().c_str());
        return Status(oldConfiguration.error().code(), oldConfiguration.error().message());
    }

    // Write to the configuration map to inform the kernel eBPF program to switch
    // from using one map to the other. Use flag BPF_EXIST here since the map should
    // be already populated in initMaps.
    uint8_t newConfigure = (oldConfiguration.value() == SELECT_MAP_A) ? SELECT_MAP_B : SELECT_MAP_A;
    auto res = mConfigurationMap.writeValue(CURRENT_STATS_MAP_CONFIGURATION_KEY, newConfigure,
                                            BPF_EXIST);
    if (!res.ok()) {
        ALOGE("Failed to toggle the stats map: %s", strerror(res.error().code()));
        return res;
    }
    // After changing the config, we need to make sure all the current running
    // eBPF programs are finished and all the CPUs are aware of this config change
    // before we modify the old map. So we do a special hack here to wait for
    // the kernel to do a synchronize_rcu(). Once the kernel called
    // synchronize_rcu(), the config we just updated will be available to all cores
    // and the next eBPF programs triggered inside the kernel will use the new
    // map configuration. So once this function returns we can safely modify the
    // old stats map without concerning about race between the kernel and
    // userspace.
    int ret = synchronizeKernelRCU();
    if (ret) {
        ALOGE("map swap synchronize_rcu() ended with failure: %s", strerror(-ret));
        return statusFromErrno(-ret, "map swap synchronize_rcu() failed");
    }
    return netdutils::status::ok;
}

void TrafficController::setPermissionForUids(int permission, const std::vector<uid_t>& uids) {
    std::lock_guard guard(mMutex);
    if (permission == INetd::PERMISSION_UNINSTALLED) {
        for (uid_t uid : uids) {
            // Clean up all permission information for the related uid if all the
            // packages related to it are uninstalled.
            mPrivilegedUser.erase(uid);
            if (mBpfEnabled) {
                Status ret = mUidPermissionMap.deleteValue(uid);
                if (!isOk(ret) && ret.code() != ENOENT) {
                    ALOGE("Failed to clean up the permission for %u: %s", uid,
                          strerror(ret.code()));
                }
            }
        }
        return;
    }

    bool privileged = (permission & INetd::PERMISSION_UPDATE_DEVICE_STATS);

    for (uid_t uid : uids) {
        if (privileged) {
            mPrivilegedUser.insert(uid);
        } else {
            mPrivilegedUser.erase(uid);
        }

        // Skip the bpf map operation if not supported.
        if (!mBpfEnabled) {
            continue;
        }
        // The map stores all the permissions that the UID has, except if the only permission
        // the UID has is the INTERNET permission, then the UID should not appear in the map.
        if (permission != INetd::PERMISSION_INTERNET) {
            Status ret = mUidPermissionMap.writeValue(uid, permission, BPF_ANY);
            if (!isOk(ret)) {
                ALOGE("Failed to set permission: %s of uid(%u) to permission map: %s",
                      UidPermissionTypeToString(permission).c_str(), uid, strerror(ret.code()));
            }
        } else {
            Status ret = mUidPermissionMap.deleteValue(uid);
            if (!isOk(ret) && ret.code() != ENOENT) {
                ALOGE("Failed to remove uid %u from permission map: %s", uid, strerror(ret.code()));
            }
        }
    }
}

std::string getProgramStatus(const char *path) {
    int ret = access(path, R_OK);
    if (ret == 0) {
        return StringPrintf("OK");
    }
    if (ret != 0 && errno == ENOENT) {
        return StringPrintf("program is missing at: %s", path);
    }
    return StringPrintf("check Program %s error: %s", path, strerror(errno));
}

std::string getMapStatus(const base::unique_fd& map_fd, const char* path) {
    if (map_fd.get() < 0) {
        return StringPrintf("map fd lost");
    }
    if (access(path, F_OK) != 0) {
        return StringPrintf("map not pinned to location: %s", path);
    }
    return StringPrintf("OK");
}

// NOLINTNEXTLINE(google-runtime-references): grandfathered pass by non-const reference
void dumpBpfMap(const std::string& mapName, DumpWriter& dw, const std::string& header) {
    dw.blankline();
    dw.println("%s:", mapName.c_str());
    if (!header.empty()) {
        dw.println(header);
    }
}

const String16 TrafficController::DUMP_KEYWORD = String16("trafficcontroller");

void TrafficController::dump(DumpWriter& dw, bool verbose) {
    std::lock_guard guard(mMutex);
    ScopedIndent indentTop(dw);
    dw.println("TrafficController");

    ScopedIndent indentPreBpfModule(dw);
    dw.println("BPF module status: %s", mBpfEnabled ? "enabled" : "disabled");
    dw.println("BPF support level: %s", BpfLevelToString(getBpfSupportLevel()).c_str());

    if (!mBpfEnabled) {
        return;
    }

    dw.blankline();
    dw.println("mCookieTagMap status: %s",
               getMapStatus(mCookieTagMap.getMap(), COOKIE_TAG_MAP_PATH).c_str());
    dw.println("mUidCounterSetMap status: %s",
               getMapStatus(mUidCounterSetMap.getMap(), UID_COUNTERSET_MAP_PATH).c_str());
    dw.println("mAppUidStatsMap status: %s",
               getMapStatus(mAppUidStatsMap.getMap(), APP_UID_STATS_MAP_PATH).c_str());
    dw.println("mStatsMapA status: %s",
               getMapStatus(mStatsMapA.getMap(), STATS_MAP_A_PATH).c_str());
    dw.println("mStatsMapB status: %s",
               getMapStatus(mStatsMapB.getMap(), STATS_MAP_B_PATH).c_str());
    dw.println("mIfaceIndexNameMap status: %s",
               getMapStatus(mIfaceIndexNameMap.getMap(), IFACE_INDEX_NAME_MAP_PATH).c_str());
    dw.println("mIfaceStatsMap status: %s",
               getMapStatus(mIfaceStatsMap.getMap(), IFACE_STATS_MAP_PATH).c_str());
    dw.println("mConfigurationMap status: %s",
               getMapStatus(mConfigurationMap.getMap(), CONFIGURATION_MAP_PATH).c_str());
    dw.println("mUidOwnerMap status: %s",
               getMapStatus(mUidOwnerMap.getMap(), UID_OWNER_MAP_PATH).c_str());

    dw.blankline();
    dw.println("Cgroup ingress program status: %s",
               getProgramStatus(BPF_INGRESS_PROG_PATH).c_str());
    dw.println("Cgroup egress program status: %s", getProgramStatus(BPF_EGRESS_PROG_PATH).c_str());
    dw.println("xt_bpf ingress program status: %s",
               getProgramStatus(XT_BPF_INGRESS_PROG_PATH).c_str());
    dw.println("xt_bpf egress program status: %s",
               getProgramStatus(XT_BPF_EGRESS_PROG_PATH).c_str());
    dw.println("xt_bpf bandwidth whitelist program status: %s",
               getProgramStatus(XT_BPF_WHITELIST_PROG_PATH).c_str());
    dw.println("xt_bpf bandwidth blacklist program status: %s",
               getProgramStatus(XT_BPF_BLACKLIST_PROG_PATH).c_str());

    if (!verbose) {
        return;
    }

    dw.blankline();
    dw.println("BPF map content:");

    ScopedIndent indentForMapContent(dw);

    // Print CookieTagMap content.
    dumpBpfMap("mCookieTagMap", dw, "");
    const auto printCookieTagInfo = [&dw](const uint64_t& key, const UidTagValue& value,
                                          const BpfMap<uint64_t, UidTagValue>&) {
        dw.println("cookie=%" PRIu64 " tag=0x%x uid=%u", key, value.tag, value.uid);
        return base::Result<void>();
    };
    base::Result<void> res = mCookieTagMap.iterateWithValue(printCookieTagInfo);
    if (!res.ok()) {
        dw.println("mCookieTagMap print end with error: %s", res.error().message().c_str());
    }

    // Print UidCounterSetMap Content
    dumpBpfMap("mUidCounterSetMap", dw, "");
    const auto printUidInfo = [&dw](const uint32_t& key, const uint8_t& value,
                                    const BpfMap<uint32_t, uint8_t>&) {
        dw.println("%u %u", key, value);
        return base::Result<void>();
    };
    res = mUidCounterSetMap.iterateWithValue(printUidInfo);
    if (!res.ok()) {
        dw.println("mUidCounterSetMap print end with error: %s", res.error().message().c_str());
    }

    // Print AppUidStatsMap content
    std::string appUidStatsHeader = StringPrintf("uid rxBytes rxPackets txBytes txPackets");
    dumpBpfMap("mAppUidStatsMap:", dw, appUidStatsHeader);
    auto printAppUidStatsInfo = [&dw](const uint32_t& key, const StatsValue& value,
                                      const BpfMap<uint32_t, StatsValue>&) {
        dw.println("%u %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64, key, value.rxBytes,
                   value.rxPackets, value.txBytes, value.txPackets);
        return base::Result<void>();
    };
    res = mAppUidStatsMap.iterateWithValue(printAppUidStatsInfo);
    if (!res.ok()) {
        dw.println("mAppUidStatsMap print end with error: %s", res.error().message().c_str());
    }

    // Print uidStatsMap content
    std::string statsHeader = StringPrintf("ifaceIndex ifaceName tag_hex uid_int cnt_set rxBytes"
                                           " rxPackets txBytes txPackets");
    dumpBpfMap("mStatsMapA", dw, statsHeader);
    const auto printStatsInfo = [&dw, this](const StatsKey& key, const StatsValue& value,
                                            const BpfMap<StatsKey, StatsValue>&) {
        uint32_t ifIndex = key.ifaceIndex;
        auto ifname = mIfaceIndexNameMap.readValue(ifIndex);
        if (!ifname.ok()) {
            ifname = IfaceValue{"unknown"};
        }
        dw.println("%u %s 0x%x %u %u %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64, ifIndex,
                   ifname.value().name, key.tag, key.uid, key.counterSet, value.rxBytes,
                   value.rxPackets, value.txBytes, value.txPackets);
        return base::Result<void>();
    };
    res = mStatsMapA.iterateWithValue(printStatsInfo);
    if (!res.ok()) {
        dw.println("mStatsMapA print end with error: %s", res.error().message().c_str());
    }

    // Print TagStatsMap content.
    dumpBpfMap("mStatsMapB", dw, statsHeader);
    res = mStatsMapB.iterateWithValue(printStatsInfo);
    if (!res.ok()) {
        dw.println("mStatsMapB print end with error: %s", res.error().message().c_str());
    }

    // Print ifaceIndexToNameMap content.
    dumpBpfMap("mIfaceIndexNameMap", dw, "");
    const auto printIfaceNameInfo = [&dw](const uint32_t& key, const IfaceValue& value,
                                          const BpfMap<uint32_t, IfaceValue>&) {
        const char* ifname = value.name;
        dw.println("ifaceIndex=%u ifaceName=%s", key, ifname);
        return base::Result<void>();
    };
    res = mIfaceIndexNameMap.iterateWithValue(printIfaceNameInfo);
    if (!res.ok()) {
        dw.println("mIfaceIndexNameMap print end with error: %s", res.error().message().c_str());
    }

    // Print ifaceStatsMap content
    std::string ifaceStatsHeader = StringPrintf("ifaceIndex ifaceName rxBytes rxPackets txBytes"
                                                " txPackets");
    dumpBpfMap("mIfaceStatsMap:", dw, ifaceStatsHeader);
    const auto printIfaceStatsInfo = [&dw, this](const uint32_t& key, const StatsValue& value,
                                                 const BpfMap<uint32_t, StatsValue>&) {
        auto ifname = mIfaceIndexNameMap.readValue(key);
        if (!ifname.ok()) {
            ifname = IfaceValue{"unknown"};
        }
        dw.println("%u %s %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64, key, ifname.value().name,
                   value.rxBytes, value.rxPackets, value.txBytes, value.txPackets);
        return base::Result<void>();
    };
    res = mIfaceStatsMap.iterateWithValue(printIfaceStatsInfo);
    if (!res.ok()) {
        dw.println("mIfaceStatsMap print end with error: %s", res.error().message().c_str());
    }

    dw.blankline();

    uint32_t key = UID_RULES_CONFIGURATION_KEY;
    auto configuration = mConfigurationMap.readValue(key);
    if (configuration.ok()) {
        dw.println("current ownerMatch configuration: %d%s", configuration.value(),
                   uidMatchTypeToString(configuration.value()).c_str());
    } else {
        dw.println("mConfigurationMap read ownerMatch configure failed with error: %s",
                   configuration.error().message().c_str());
    }

    key = CURRENT_STATS_MAP_CONFIGURATION_KEY;
    configuration = mConfigurationMap.readValue(key);
    if (configuration.ok()) {
        const char* statsMapDescription = "???";
        switch (configuration.value()) {
            case SELECT_MAP_A:
                statsMapDescription = "SELECT_MAP_A";
                break;
            case SELECT_MAP_B:
                statsMapDescription = "SELECT_MAP_B";
                break;
                // No default clause, so if we ever add a third map, this code will fail to build.
        }
        dw.println("current statsMap configuration: %d %s", configuration.value(),
                   statsMapDescription);
    } else {
        dw.println("mConfigurationMap read stats map configure failed with error: %s",
                   configuration.error().message().c_str());
    }
    dumpBpfMap("mUidOwnerMap", dw, "");
    const auto printUidMatchInfo = [&dw, this](const uint32_t& key, const UidOwnerValue& value,
                                               const BpfMap<uint32_t, UidOwnerValue>&) {
        if (value.rule & IIF_MATCH) {
            auto ifname = mIfaceIndexNameMap.readValue(value.iif);
            if (ifname.ok()) {
                dw.println("%u %s %s", key, uidMatchTypeToString(value.rule).c_str(),
                           ifname.value().name);
            } else {
                dw.println("%u %s %u", key, uidMatchTypeToString(value.rule).c_str(), value.iif);
            }
        } else {
            dw.println("%u %s", key, uidMatchTypeToString(value.rule).c_str());
        }
        return base::Result<void>();
    };
    res = mUidOwnerMap.iterateWithValue(printUidMatchInfo);
    if (!res.ok()) {
        dw.println("mUidOwnerMap print end with error: %s", res.error().message().c_str());
    }
    dumpBpfMap("mUidPermissionMap", dw, "");
    const auto printUidPermissionInfo = [&dw](const uint32_t& key, const int& value,
                                              const BpfMap<uint32_t, uint8_t>&) {
        dw.println("%u %s", key, UidPermissionTypeToString(value).c_str());
        return base::Result<void>();
    };
    res = mUidPermissionMap.iterateWithValue(printUidPermissionInfo);
    if (!res.ok()) {
        dw.println("mUidPermissionMap print end with error: %s", res.error().message().c_str());
    }

    dumpBpfMap("mPrivilegedUser", dw, "");
    for (uid_t uid : mPrivilegedUser) {
        dw.println("%u ALLOW_UPDATE_DEVICE_STATS", (uint32_t)uid);
    }
}

}  // namespace net
}  // namespace android
