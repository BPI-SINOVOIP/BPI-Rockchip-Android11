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

#include "netlink_socket.h"

#include "log.h"
#include "netlink_message.h"

#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <netlink/netlink.h>

NetlinkSocket::NetlinkSocket() {
}

NetlinkSocket::~NetlinkSocket() {
    if (mSocket) {
        nl_socket_free(mSocket);
        mSocket = nullptr;
        mCallback = nullptr;
    }
}

Result NetlinkSocket::init() {
    if (mSocket || mCallback) {
        return Result::error("Netlink socket already initialized");
    }
    mCallback = nl_cb_alloc(NL_CB_CUSTOM);
    if (!mCallback) {
        return Result::error("Netlink socket failed to allocate callbacks");
    }
    mSocket = nl_socket_alloc_cb(mCallback);
    if (!mSocket) {
        return Result::error("Failed to allocate netlink socket");
    }
    return Result::success();
}

Result NetlinkSocket::setBufferSizes(int rxBufferSize, int txBufferSize) {
    int res = nl_socket_set_buffer_size(mSocket, rxBufferSize, txBufferSize);
    if (res != 0) {
        return Result::error("Failed to set buffer sizes: %s",
                             nl_geterror(res));
    }
    return Result::success();
}

Result NetlinkSocket::setOnMsgInCallback(int (*callback)(struct nl_msg*, void*),
                                         void* context) {
    if (nl_cb_set(mCallback, NL_CB_MSG_IN, NL_CB_CUSTOM, callback, context)) {
        return Result::error("Failed to set OnMsgIn callback");
    }
    return Result::success();
}

Result NetlinkSocket::setOnMsgOutCallback(int (*callback)(struct nl_msg*,
                                                          void*),
                                          void* context) {
    if (nl_cb_set(mCallback, NL_CB_MSG_OUT, NL_CB_CUSTOM, callback, context)) {
        return Result::error("Failed to set OnMsgOut callback");
    }
    return Result::success();
}

Result NetlinkSocket::setOnSeqCheckCallback(int (*callback)(struct nl_msg*,
                                                            void*),
                                            void* context) {
    if (nl_cb_set(mCallback, NL_CB_SEQ_CHECK, NL_CB_CUSTOM,
                  callback, context)) {
        return Result::error("Failed to set OnSeqCheck callback");
    }
    return Result::success();
}

Result NetlinkSocket::setOnAckCallback(int (*callback)(struct nl_msg*, void*),
                                       void* context) {
    if (nl_cb_set(mCallback, NL_CB_ACK, NL_CB_CUSTOM, callback, context)) {
        return Result::error("Failed to set OnAck callback");
    }
    return Result::success();
}

Result NetlinkSocket::setOnErrorCallback(int (*callback)(struct sockaddr_nl*,
                                                         struct nlmsgerr*,
                                                         void*),
                                         void* context) {
    if (nl_cb_err(mCallback, NL_CB_CUSTOM, callback, context)) {
        return Result::error("Failed to set OnError callback");
    }
    return Result::success();
}

Result NetlinkSocket::connectGeneric() {
    int status = genl_connect(mSocket);
    if (status < 0) {
        return Result::error("WifiNetlinkForwarder socket connect failed: %d",
                             status);
    }
    return Result::success();
}

int NetlinkSocket::resolveNetlinkFamily(const char* familyName) {
    return genl_ctrl_resolve(mSocket, familyName);
}

bool NetlinkSocket::send(NetlinkMessage& message) {
    int status = nl_send_auto(mSocket, message.get()) >= 0;
    if (status < 0) {
        ALOGE("Failed to send on netlink socket: %s", nl_geterror(status));
        return false;
    }
    return true;
}

bool NetlinkSocket::receive() {
    int res = nl_recvmsgs_default(mSocket);
    if (res != 0) {
        ALOGE("Failed to receive messages on netlink socket: %s",
              nl_geterror(res));
        return false;
    }
    return true;
}

int NetlinkSocket::getFd() const {
    if (!mSocket) {
        return -1;
    }
    return nl_socket_get_fd(mSocket);
}
