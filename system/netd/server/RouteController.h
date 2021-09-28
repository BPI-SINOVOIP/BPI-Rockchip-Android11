/*
 * Copyright (C) 2014 The Android Open Source Project
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

#pragma once

#include "NetdConstants.h"  // IptablesTarget
#include "Permission.h"

#include <android-base/thread_annotations.h>

#include <linux/netlink.h>
#include <sys/types.h>
#include <map>
#include <mutex>

namespace android::net {

class UidRanges;

class RouteController {
public:
    // How the routing table number is determined for route modification requests.
    enum TableType {
        INTERFACE,       // Compute the table number based on the interface index.
        LOCAL_NETWORK,   // A fixed table used for routes to directly-connected clients/peers.
        LEGACY_NETWORK,  // Use a fixed table that's used to override the default network.
        LEGACY_SYSTEM,   // A fixed table, only modifiable by system apps; overrides VPNs too.
    };

    static const int ROUTE_TABLE_OFFSET_FROM_INDEX = 1000;

    static const char* const LOCAL_MANGLE_INPUT;

    [[nodiscard]] static int Init(unsigned localNetId);

    // Returns an ifindex given the interface name, by looking up in sInterfaceToTable.
    // This is currently only used by NetworkController::addInterfaceToNetwork
    // and should probabaly be changed to passing the ifindex into RouteController instead.
    // We do this instead of calling if_nametoindex because the same interface name can
    // correspond to different interface indices over time. This way, even if the interface
    // index has changed, we can still free any map entries indexed by the ifindex that was
    // used to add them.
    static uint32_t getIfIndex(const char* interface) EXCLUDES(sInterfaceToTableLock);

    [[nodiscard]] static int addInterfaceToLocalNetwork(unsigned netId, const char* interface);
    [[nodiscard]] static int removeInterfaceFromLocalNetwork(unsigned netId, const char* interface);

    [[nodiscard]] static int addInterfaceToPhysicalNetwork(unsigned netId, const char* interface,
                                                           Permission permission);
    [[nodiscard]] static int removeInterfaceFromPhysicalNetwork(unsigned netId,
                                                                const char* interface,
                                                                Permission permission);

    [[nodiscard]] static int addInterfaceToVirtualNetwork(unsigned netId, const char* interface,
                                                          bool secure, const UidRanges& uidRanges);
    [[nodiscard]] static int removeInterfaceFromVirtualNetwork(unsigned netId,
                                                               const char* interface, bool secure,
                                                               const UidRanges& uidRanges);

    [[nodiscard]] static int modifyPhysicalNetworkPermission(unsigned netId, const char* interface,
                                                             Permission oldPermission,
                                                             Permission newPermission);

    [[nodiscard]] static int addUsersToVirtualNetwork(unsigned netId, const char* interface,
                                                      bool secure, const UidRanges& uidRanges);
    [[nodiscard]] static int removeUsersFromVirtualNetwork(unsigned netId, const char* interface,
                                                           bool secure, const UidRanges& uidRanges);

    [[nodiscard]] static int addUsersToRejectNonSecureNetworkRule(const UidRanges& uidRanges);
    [[nodiscard]] static int removeUsersFromRejectNonSecureNetworkRule(const UidRanges& uidRanges);

    [[nodiscard]] static int addInterfaceToDefaultNetwork(const char* interface,
                                                          Permission permission);
    [[nodiscard]] static int removeInterfaceFromDefaultNetwork(const char* interface,
                                                               Permission permission);

    // |nexthop| can be NULL (to indicate a directly-connected route), "unreachable" (to indicate a
    // route that's blocked), "throw" (to indicate the lack of a match), or a regular IP address.
    [[nodiscard]] static int addRoute(const char* interface, const char* destination,
                                      const char* nexthop, TableType tableType, int mtu);
    [[nodiscard]] static int removeRoute(const char* interface, const char* destination,
                                         const char* nexthop, TableType tableType);
    [[nodiscard]] static int updateRoute(const char* interface, const char* destination,
                                         const char* nexthop, TableType tableType, int mtu);

    [[nodiscard]] static int enableTethering(const char* inputInterface,
                                             const char* outputInterface);
    [[nodiscard]] static int disableTethering(const char* inputInterface,
                                              const char* outputInterface);

    [[nodiscard]] static int addVirtualNetworkFallthrough(unsigned vpnNetId,
                                                          const char* physicalInterface,
                                                          Permission permission);
    [[nodiscard]] static int removeVirtualNetworkFallthrough(unsigned vpnNetId,
                                                             const char* physicalInterface,
                                                             Permission permission);

    // For testing.
    static int (*iptablesRestoreCommandFunction)(IptablesTarget, const std::string&,
                                                 const std::string&, std::string *);

private:
    friend class RouteControllerTest;

    static std::mutex sInterfaceToTableLock;
    static std::map<std::string, uint32_t> sInterfaceToTable GUARDED_BY(sInterfaceToTableLock);

    static int configureDummyNetwork();
    [[nodiscard]] static int flushRoutes(const char* interface) EXCLUDES(sInterfaceToTableLock);
    [[nodiscard]] static int flushRoutes(uint32_t table);
    static uint32_t getRouteTableForInterfaceLocked(const char* interface)
            REQUIRES(sInterfaceToTableLock);
    static uint32_t getRouteTableForInterface(const char *interface) EXCLUDES(sInterfaceToTableLock);
    static int modifyDefaultNetwork(uint16_t action, const char* interface, Permission permission);
    static int modifyPhysicalNetwork(unsigned netId, const char* interface, Permission permission,
                                     bool add);
    static int modifyRoute(uint16_t action, uint16_t flags, const char* interface,
                           const char* destination, const char* nexthop, TableType tableType,
                           int mtu);
    static int modifyTetheredNetwork(uint16_t action, const char* inputInterface,
                                     const char* outputInterface);
    static int modifyVpnFallthroughRule(uint16_t action, unsigned vpnNetId,
                                        const char* physicalInterface, Permission permission);
    static int modifyVirtualNetwork(unsigned netId, const char* interface,
                                    const UidRanges& uidRanges, bool secure, bool add,
                                    bool modifyNonUidBasedRules);
    static void updateTableNamesFile() EXCLUDES(sInterfaceToTableLock);
};

// Public because they are called by by RouteControllerTest.cpp.
// TODO: come up with a scheme of unit testing this code that does not rely on making all its
// functions public.
[[nodiscard]] int modifyIpRoute(uint16_t action, uint16_t flags, uint32_t table,
                                const char* interface, const char* destination, const char* nexthop,
                                uint32_t mtu);
uint32_t getRulePriority(const nlmsghdr *nlh);
[[nodiscard]] int modifyIncomingPacketMark(unsigned netId, const char* interface,
                                           Permission permission, bool add);

}  // namespace android::net
