/*
 * Copyright 2019, The Android Open Source Project
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

#include "bridge.h"

#include "log.h"

#include <errno.h>
#include <net/if.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

Bridge::Bridge(const std::string& bridgeName) : mBridgeName(bridgeName) {
}

Bridge::~Bridge() {
    if (mSocketFd != -1) {
        ::close(mSocketFd);
        mSocketFd = -1;
    }
}

Result Bridge::init() {
    Result res = createSocket();
    if (!res) {
        return res;
    }
    res = createBridge();
    if (!res) {
        return res;
    }
    return Result::success();
}

Result Bridge::addInterface(const std::string& interfaceName) {
    return doInterfaceOperation(interfaceName, SIOCBRADDIF, "add");
}

Result Bridge::removeInterface(const std::string& interfaceName) {
    return doInterfaceOperation(interfaceName, SIOCBRDELIF, "remove");
}

Result Bridge::createSocket() {
    if (mSocketFd != -1) {
        return Result::error("Bridge already initialized");
    }
    mSocketFd = ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (mSocketFd < 0) {
        return Result::error("Unable to create socket for bridge: %s",
                             strerror(errno));
    }
    return Result::success();
}

Result Bridge::createBridge() {
    int res = ::ioctl(mSocketFd, SIOCBRADDBR, mBridgeName.c_str());
    if (res < 0) {
        // If the bridge already exists we just keep going, that's fine.
        // Otherwise something went wrong.
        if (errno != EEXIST) {
            return Result::error("Cannot create bridge %s: %s",
                                 mBridgeName.c_str(), strerror(errno));
        }
    }

    struct ifreq request;
    memset(&request, 0, sizeof(request));
    // Determine interface index of bridge
    request.ifr_ifindex = if_nametoindex(mBridgeName.c_str());
    if (request.ifr_ifindex == 0) {
        return Result::error("Unable to get bridge %s interface index",
                             mBridgeName.c_str());
    }
    // Get bridge interface flags
    strlcpy(request.ifr_name, mBridgeName.c_str(), sizeof(request.ifr_name));
    res = ::ioctl(mSocketFd, SIOCGIFFLAGS, &request);
    if (res != 0) {
        return Result::error("Unable to get interface flags for bridge %s: %s",
                             mBridgeName.c_str(), strerror(errno));
    }

    if ((request.ifr_flags & IFF_UP) != 0) {
        // Bridge is already up, it's ready to go
        return Result::success();
    }

    // Bridge is not up, it needs to be up to work
    request.ifr_flags |= IFF_UP;
    res = ::ioctl(mSocketFd, SIOCSIFFLAGS, &request);
    if (res != 0) {
        return Result::error("Unable to set interface flags for bridge %s: %s",
                             strerror(errno));
    }

    return Result::success();
}

Result Bridge::doInterfaceOperation(const std::string& interfaceName,
                                    unsigned long operation,
                                    const char* operationName) {
    struct ifreq request;
    memset(&request, 0, sizeof(request));

    request.ifr_ifindex = if_nametoindex(interfaceName.c_str());
    if (request.ifr_ifindex == 0) {
        return Result::error("Bridge unable to %s interface '%s', no such "
                             "interface", operationName, interfaceName.c_str());
    }
    strlcpy(request.ifr_name, mBridgeName.c_str(), sizeof(request.ifr_name));
    int res = ::ioctl(mSocketFd, operation, &request);
    // An errno of EBUSY most likely indicates that the interface is already
    // part of the bridge. Ignore this.
    if (res < 0 && errno != EBUSY) {
        return Result::error("Bridge unable to %s interface '%s': %s",
                             operationName, interfaceName.c_str(),
                             strerror(errno));
    }
    return Result::success();
}
