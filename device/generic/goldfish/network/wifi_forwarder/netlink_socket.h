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

#pragma once

#include "result.h"

struct nl_cb;
struct nl_msg;
struct nl_sock;
struct nlmsgerr;
struct sockaddr_nl;

class NetlinkMessage;

class NetlinkSocket {
public:
    NetlinkSocket();
    ~NetlinkSocket();

    Result init();

    /* Set the size of the receive buffer to |rxBufferSize| bytes and the
     * transmit buffer to |txBufferSize| bytes.
     */
    Result setBufferSizes(int rxBufferSize, int txBufferSize);

    Result setOnMsgInCallback(int (*callback)(struct nl_msg*, void*),
                              void* context);
    Result setOnMsgOutCallback(int (*callback)(struct nl_msg*, void*),
                              void* context);
    Result setOnSeqCheckCallback(int (*callback)(struct nl_msg*, void*),
                                 void* context);
    Result setOnAckCallback(int (*callback)(struct nl_msg*, void*),
                            void* context);
    Result setOnErrorCallback(int (*callback)(struct sockaddr_nl*,
                                              struct nlmsgerr*,
                                              void*),
                              void* context);

    /* Connect socket to generic netlink. This needs to be done before generic
     * netlink messages can be sent. After this is done the caller should use
     * @resolveGenericNetlinkFamily to determine the generic family id to use.
     * Then create NetlinkMessage's with that family id.
     */
    Result connectGeneric();

    /* Resolve a generic family name to a family identifier. This is used when
     * sending generic netlink messages to indicate where the message should go.
     * Examples of family names are "mac80211_hwsim" or "nl80211".
     */
    int resolveNetlinkFamily(const char* familyName);

    /* Send a netlink message on this socket. */
    bool send(NetlinkMessage& message);

    /* Receive all pending message. This method does not return any messages,
     * instead they will be provided through the callback set with
     * setOnMsgInCallback. This callback will be called on the same thread as
     * this method while this method is running. */
    bool receive();

    int getFd() const;

private:
    struct nl_cb* mCallback = nullptr;
    struct nl_sock* mSocket = nullptr;
};

