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

#pragma once

#include <functional>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "NetdConstants.h"

namespace android::net {

const sockaddr_nl KERNEL_NLADDR = {AF_NETLINK, 0, 0, 0};

const uint16_t NETLINK_REQUEST_FLAGS = NLM_F_REQUEST | NLM_F_ACK;
const uint16_t NETLINK_ROUTE_CREATE_FLAGS = NETLINK_REQUEST_FLAGS | NLM_F_CREATE | NLM_F_EXCL;
const uint16_t NETLINK_ROUTE_REPLACE_FLAGS = NETLINK_REQUEST_FLAGS | NLM_F_REPLACE;
// Don't create rules with NLM_F_EXCL, because operations such as changing network permissions rely
// on make-before-break. The kernel did not complain about duplicate rules until ~4.9, at which
// point it started returning EEXIST. See for example b/69607866 . We can't just ignore the EEXIST
// because if we hit it, the rule was not created, but we will think it was, and we'll then trip up
// trying to delete it.
const uint16_t NETLINK_RULE_CREATE_FLAGS = NETLINK_REQUEST_FLAGS | NLM_F_CREATE;
const uint16_t NETLINK_DUMP_FLAGS = NLM_F_REQUEST | NLM_F_DUMP;

// Generic code for processing netlink dumps.
const int kNetlinkDumpBufferSize = 8192;
typedef std::function<void(nlmsghdr *)> NetlinkDumpCallback;
typedef std::function<bool(nlmsghdr *)> NetlinkDumpFilter;

// Opens an RTNetlink socket and connects it to the kernel.
[[nodiscard]] int openNetlinkSocket(int protocol);

// Receives a netlink ACK. Returns 0 if the command succeeded or negative errno if the command
// failed or receiving the ACK failed.
[[nodiscard]] int recvNetlinkAck(int sock);

// Sends a netlink request and possibly expects an ACK. The first element of iov should be null and
// will be set to the netlink message headerheader. The subsequent elements are the contents of the
// request.
[[nodiscard]] int sendNetlinkRequest(uint16_t action, uint16_t flags, iovec* iov, int iovlen,
                                     const NetlinkDumpCallback* callback);

// Processes a netlink dump, passing every message to the specified |callback|.
[[nodiscard]] int processNetlinkDump(int sock, const NetlinkDumpCallback& callback);

// Flushes netlink objects that take an rtmsg structure (FIB rules, routes...). |getAction| and
// |deleteAction| specify the netlink message types, e.g., RTM_GETRULE and RTM_DELRULE.
// |shouldDelete| specifies whether a given object should be deleted or not. |what| is a
// human-readable name for the objects being flushed, e.g. "rules".
[[nodiscard]] int rtNetlinkFlush(uint16_t getAction, uint16_t deleteAction, const char* what,
                                 const NetlinkDumpFilter& shouldDelete);

// Returns the value of the specific __u32 attribute, or 0 if the attribute was not present.
uint32_t getRtmU32Attribute(const nlmsghdr *nlh, int attribute);

}  // namespace android::net
