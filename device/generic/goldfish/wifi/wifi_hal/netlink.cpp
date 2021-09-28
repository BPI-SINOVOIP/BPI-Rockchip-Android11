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

#include "netlink.h"

#include "log.h"
#include "netlinkmessage.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <linux/netlink.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static const size_t kControlRead = 0;
static const size_t kControlWrite = 1;

static void closeIfOpen(int* fd) {
    if (*fd != -1) {
        ::close(*fd);
        *fd = -1;
    }
}

Netlink::Netlink()
    : mNextSequenceNumber(1)
    , mSocket(-1) {
    mControlPipe[kControlRead] = -1;
    mControlPipe[kControlWrite] = -1;
}

Netlink::~Netlink() {
    closeIfOpen(&mSocket);
    closeIfOpen(&mControlPipe[kControlRead]);
    closeIfOpen(&mControlPipe[kControlWrite]);
}

bool Netlink::init() {
    if (mSocket != -1) {
        ALOGE("Netlink already initialized");
        return false;
    }

    int status = ::pipe2(mControlPipe, O_CLOEXEC);
    if (status != 0) {
        ALOGE("Failed to create control pipe: %s", strerror(errno));
        return false;
    }

    mSocket = ::socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (mSocket == -1) {
        ALOGE("Failed to create netlink socket: %s", strerror(errno));
        return false;
    }

    struct sockaddr_nl addr;
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    status = ::bind(mSocket,
                    reinterpret_cast<struct sockaddr*>(&addr),
                    sizeof(addr));
    if (status != 0) {
        ALOGE("Failed to bind netlink socket: %s", strerror(errno));
        return false;
    }

    return true;
}

void Netlink::stop(StopHandler handler) {
    char stop = 1;
    // Set the handler before writing so that it's guaranteed to be available
    // when the event loop reads from the control pipe.
    {
        // No need to keep the lock while writing so make it scoped
        std::unique_lock<std::mutex> lock(mStopHandlerMutex);
        mStopHandler = handler;
    }
    ::write(mControlPipe[kControlWrite], &stop, sizeof(stop));
}

bool Netlink::eventLoop() {
    struct pollfd fds[2];
    memset(fds, 0, sizeof(fds));
    fds[0].fd = mSocket;
    fds[0].events = POLLIN;
    fds[1].fd = mControlPipe[kControlRead];
    fds[1].events = POLLIN;

    for (;;) {
        int status = ::poll(fds, 2, -1);
        if (status == 0) {
            // Timeout, not really supposed to happen
            ALOGW("poll encountered a timeout despite infinite timeout");
            continue;
        } else if (status < 0) {
            if (errno == EINTR) {
                continue;
            }
            ALOGE("poll encountered an error: %s", strerror(errno));
            return false;
        }
        for (auto& fd : fds) {
            if ((fd.revents & POLLIN) == 0) {
                continue;
            }
            if (fd.fd == mSocket) {
                readNetlinkMessage(fd.fd);
            } else if (fd.fd == mControlPipe[kControlRead]) {
                if (readControlMessage()) {
                    // Make a copy of the stop handler while holding the lock
                    // and then call it after releasing the lock. This prevents
                    // the potential deadlock of someone calling stop from the
                    // stop callback. The drawback of this is that if someone
                    // calls stop again with a new stop handler that new stop
                    // handler might not be called if the timing is wrong.
                    // Both of these scenarios indicate highly questionable
                    // behavior on the callers part but at least this way the
                    // event loop will terminate which seems better than a
                    // total deadlock.
                    StopHandler handler;
                    {
                        std::unique_lock<std::mutex> lock(mStopHandlerMutex);
                        handler = mStopHandler;
                    }
                    if (handler) {
                        handler();
                    }
                    return true;
                }
            }
        }
    }
}

uint32_t Netlink::getSequenceNumber() {
    return mNextSequenceNumber++;
}

bool Netlink::sendMessage(const NetlinkMessage& message,
                          ReplyHandler handler) {
    // Keep lock the entire time so that we can safely erase the handler
    // without worrying about another call to sendAsync adding a handler that
    // shouldn't be deleted.
    std::unique_lock<std::mutex> lock(mHandlersMutex);
    // Register handler before sending in case the read thread picks up the
    // response between the send thread sending and registering the handler.
    mHandlers[message.sequence()] = handler;
    for (;;) {
        int bytesSent = ::send(mSocket, message.data(), message.size(), 0);
        if (bytesSent > 0 && static_cast<size_t>(bytesSent) == message.size()) {
            return true;
        }
        if (bytesSent < 0 && errno == EINTR) {
            // We need to try again, keep the mutex locked
            continue;
        }
        // It's a failure, remove the handler and unlock the mutex
        mHandlers.erase(message.sequence());
        lock.unlock();

        if (bytesSent < 0) {
            ALOGE("Failed to send netlink message: %s", strerror(errno));
        }
        return false;
    }
}

bool Netlink::readNetlinkMessage(int fd) {
    char buffer[8 * 1024];
    for (;;) {
        int bytesReceived = ::recv(fd, buffer, sizeof(buffer), 0);
        if (bytesReceived < 0) {
            if (errno == EINTR) {
                continue;
            }
            ALOGE("recv failed to receive on netlink socket: %s",
                  strerror(errno));
            return false;
        }
        char* data = buffer;
        char* end = data + bytesReceived;
        while (data < end) {
            if (data + sizeof(nlmsghdr) > end) {
                ALOGE("received invalid netlink message, too small for header");
                return false;
            }
            auto header = reinterpret_cast<nlmsghdr*>(data);
            if (data + header->nlmsg_len > end) {
                ALOGE("received invalid netlink message, too small for data");
                return false;
            }

            if (header->nlmsg_type == NLMSG_ERROR) {
                if (data + NLMSG_HDRLEN + sizeof(nlmsgerr) <= end) {
                    auto err = reinterpret_cast<nlmsgerr*>(NLMSG_DATA(header));
                    ALOGE("Receive netlink error message: %s, sequence %u",
                          strerror(-err->error), header->nlmsg_seq);
                } else {
                    ALOGE("Received netlink error code but no error message");
                }
                return false;
            }

            notifyHandler(data, header->nlmsg_len);

            data += header->nlmsg_len;
        }
        return true;
    }
}

bool Netlink::readControlMessage() {
    char buffer[32];

    for (;;) {
        int bytesReceived = ::recv(mControlPipe[kControlRead],
                                   buffer,
                                   sizeof(buffer),
                                   0);
        if (bytesReceived < 0) {
            if (errno == EINTR) {
                continue;
            }
        } else if (bytesReceived == 0) {
            return false;
        }
        return true;
    }
}


void Netlink::notifyHandler(const char* data, size_t size) {
    NetlinkMessage message(data, size);

    ReplyHandler replyHandler;
    {
        std::unique_lock<std::mutex> lock(mHandlersMutex);
        auto handler = mHandlers.find(message.sequence());
        if (handler == mHandlers.end()) {
            // No handler found, ignore message
            return;
        }
        replyHandler = handler->second;
        mHandlers.erase(handler);
    }

    replyHandler(message);
}

