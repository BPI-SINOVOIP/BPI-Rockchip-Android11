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

#include "netlink_message.h"

#include <netlink/genl/genl.h>
#include <netlink/msg.h>

NetlinkMessage::NetlinkMessage() {
}

NetlinkMessage::NetlinkMessage(NetlinkMessage&& other) {
    std::swap(mMessage, other.mMessage);
}

NetlinkMessage::~NetlinkMessage() {
    if (mMessage) {
        nlmsg_free(mMessage);
        mMessage = nullptr;
    }
}

NetlinkMessage& NetlinkMessage::operator=(NetlinkMessage&& other) {
    if (mMessage) {
        nlmsg_free(mMessage);
        mMessage = nullptr;
    }
    std::swap(mMessage, other.mMessage);
    return *this;
}

bool NetlinkMessage::initGeneric(int family, uint8_t command, int version) {
    if (mMessage) {
        return false;
    }

    mMessage = nlmsg_alloc();
    if (!mMessage) {
        return false;
    }

    return genlmsg_put(mMessage, NL_AUTO_PORT, NL_AUTO_SEQ, family, 0,
                       NLM_F_REQUEST, command, version) != nullptr;
}

uint32_t NetlinkMessage::getSeqNum() const {
    return nlmsg_hdr(mMessage)->nlmsg_seq;
}
