/*
 * Copyright 2018, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "monitor.h"

#include "log.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

Monitor::Monitor() : mSocketFd(-1) {

}

Monitor::~Monitor() {
    closeSocket();
}

Result Monitor::init() {
    Result res = openSocket();
    if (!res) {
        return res;
    }
    return requestInterfaces();
}

void Monitor::setOnInterfaceState(OnInterfaceStateCallback callback) {
    mOnInterfaceStateCallback = callback;
}

bool Monitor::onReadAvailable(int /*fd*/, int* /*status*/) {
    char buffer[32768];
    struct sockaddr_storage storage;

    while (true) {
        socklen_t addrSize = sizeof(storage);
        int status = ::recvfrom(mSocketFd,
                                buffer,
                                sizeof(buffer),
                                MSG_DONTWAIT,
                                reinterpret_cast<struct sockaddr*>(&storage),
                                &addrSize);
        if (status < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Nothing to receive, everything is fine
                return true;
            } else if (errno == EINTR) {
                continue;
            }
            LOGE("Monitor receive failed: %s", strerror(errno));
            // An error occurred but let's keep trying
            return true;
        } else if (addrSize < 0 ||
                   static_cast<size_t>(addrSize) != sizeof(struct sockaddr_nl)) {
            LOGE("Monitor received invalid address size");
            // It's an error but no need to exit, let's keep polling
            return true;
        }

        size_t length = static_cast<size_t>(status);

        auto hdr = reinterpret_cast<struct nlmsghdr*>(buffer);
        while (NLMSG_OK(hdr, length) && hdr->nlmsg_type != NLMSG_DONE) {
            switch (hdr->nlmsg_type) {
                case RTM_NEWLINK:
                    handleNewLink(hdr);
                    break;
                default:
                    break;
            }
            hdr = NLMSG_NEXT(hdr, length);
        }
    }
}

bool Monitor::onClose(int /*fd*/, int* status) {
    // Socket was closed from the other end, close it from our end and re-open
    closeSocket();
    Result res = openSocket();
    if (!res) {
        LOGE("%s", res.c_str());
        *status = 1;
        return false;
    }
    return true;
}

bool Monitor::onTimeout(int* /*status*/) {
    return true;
}

void Monitor::getPollData(std::vector<pollfd>* fds) const {
    if (mSocketFd != -1) {
        fds->push_back(pollfd{mSocketFd, POLLIN, 0});
    }
}

Pollable::Timestamp Monitor::getTimeout() const {
    return Pollable::Timestamp::max();
}

Result Monitor::openSocket() {
    if (mSocketFd != -1) {
        return Result::error("Monitor already initialized");
    }

    mSocketFd = ::socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (mSocketFd == -1) {
        return Result::error("Monitor failed to open socket: %s",
                             strerror(errno));
    }

    struct sockaddr_nl addr;
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTNLGRP_LINK | RTNLGRP_IPV4_IFADDR | RTNLGRP_IPV6_IFADDR;

    struct sockaddr* sa = reinterpret_cast<struct sockaddr*>(&addr);
    if (::bind(mSocketFd, sa, sizeof(addr)) != 0) {
        return Result::error("Monitor failed to bind socket: %s",
                             strerror(errno));
    }

    return Result::success();
}

Result Monitor::requestInterfaces() {
    if (mSocketFd == -1) {
        return Result::error("Monitor not initialized yet");
    }

    struct {
        struct nlmsghdr hdr;
        struct rtgenmsg gen;
    } request;

    memset(&request, 0, sizeof(request));

    request.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(request.gen));
    request.hdr.nlmsg_type = RTM_GETLINK;
    request.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    request.hdr.nlmsg_seq = 1;
    request.hdr.nlmsg_pid = getpid();
    request.gen.rtgen_family = AF_PACKET;

    ssize_t result = TEMP_FAILURE_RETRY(::send(mSocketFd,
                                               &request,
                                               request.hdr.nlmsg_len,
                                               0));
    if (result < 0) {
        return Result::error("Failed to request interfaces: %s",
                             strerror(errno));
    }
    return Result::success();
}

void Monitor::closeSocket() {
    if (mSocketFd != -1) {
        ::close(mSocketFd);
        mSocketFd = -1;
    }
}

void Monitor::handleNewLink(const struct nlmsghdr* hdr) {
    if (!mOnInterfaceStateCallback) {
        return;
    }

    auto msg = reinterpret_cast<const struct ifinfomsg*>(NLMSG_DATA(hdr));

    char name[IF_NAMESIZE + 1] = { 0 };
    const unsigned int ifIndex = msg->ifi_index;
    if (if_indextoname(ifIndex, name) == nullptr) {
        LOGE("Unable to get interface name for interface index %u", ifIndex);
    }

    bool isUp = !!(msg->ifi_flags & IFF_UP);
    auto iterator = mUpInterfaces.find(ifIndex);

    if (iterator == mUpInterfaces.end() && isUp) {
        // The interface was not known to be up but is up, known state changed
        mUpInterfaces.insert(ifIndex);
        mOnInterfaceStateCallback(ifIndex, name, InterfaceState::Up);
    } else if (iterator != mUpInterfaces.end() && !isUp) {
        // The interface was known to be up, now it's not, known state changed
        mUpInterfaces.erase(iterator);
        mOnInterfaceStateCallback(ifIndex, name, InterfaceState::Down);
    }
}

