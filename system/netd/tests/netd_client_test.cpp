/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <netdb.h>
#include <netinet/in.h>
#include <poll.h> /* poll */
#include <sys/socket.h>
#include <sys/types.h>

#include <android-base/unique_fd.h>
#include <gtest/gtest.h>

#include "NetdClient.h"

#define SKIP_IF_NO_NETWORK_CONNECTIVITY                                    \
    do {                                                                   \
        if (!checkNetworkConnectivity()) {                                 \
            GTEST_LOG_(INFO) << "Skip. Required Network Connectivity. \n"; \
            return;                                                        \
        }                                                                  \
    } while (0)

namespace {

constexpr char TEST_DOMAIN[] = "www.google.com";

bool checkNetworkConnectivity() {
    android::base::unique_fd sock(socket(AF_INET6, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP));
    if (sock == -1) return false;
    static const sockaddr_in6 server6 = {
            .sin6_family = AF_INET6,
            .sin6_addr.s6_addr = {// 2000::
                                  0x20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
    int ret = connect(sock, reinterpret_cast<const sockaddr*>(&server6), sizeof(server6));
    if (ret == 0) return true;
    sock.reset(socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP));
    if (sock == -1) return false;
    static const sockaddr_in server4 = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = __constant_htonl(0x08080808L)  // 8.8.8.8
    };
    ret = connect(sock, reinterpret_cast<const sockaddr*>(&server4), sizeof(server4));
    return !ret;
}

void expectHasNetworking() {
    // Socket
    android::base::unique_fd ipv4(socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)),
            ipv6(socket(AF_INET6, SOCK_STREAM | SOCK_CLOEXEC, 0));
    EXPECT_LE(3, ipv4);
    EXPECT_LE(3, ipv6);

    // DNS
    addrinfo* result = nullptr;
    errno = 0;
    const addrinfo hints = {
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_DGRAM,
    };
    EXPECT_EQ(0, getaddrinfo(TEST_DOMAIN, nullptr, &hints, &result));
    EXPECT_EQ(0, errno);
    freeaddrinfo(result);
}

void expectNoNetworking() {
    // Socket
    android::base::unique_fd unixSocket(socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
    EXPECT_LE(3, unixSocket);
    android::base::unique_fd ipv4(socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
    EXPECT_EQ(-1, ipv4);
    EXPECT_EQ(EPERM, errno);
    android::base::unique_fd ipv6(socket(AF_INET6, SOCK_STREAM | SOCK_CLOEXEC, 0));
    EXPECT_EQ(-1, ipv6);
    EXPECT_EQ(EPERM, errno);

    // DNS
    addrinfo* result = nullptr;
    errno = 0;
    const addrinfo hints = {
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_DGRAM,
    };
    EXPECT_EQ(EAI_NODATA, getaddrinfo(TEST_DOMAIN, nullptr, &hints, &result));
    EXPECT_EQ(EPERM, errno);
    freeaddrinfo(result);
}

}  // namespace

TEST(NetdClientIntegrationTest, setAllowNetworkingForProcess) {
    SKIP_IF_NO_NETWORK_CONNECTIVITY;
    // At the beginning, we should be able to use socket since the default setting is allowing.
    expectHasNetworking();
    // Disable
    setAllowNetworkingForProcess(false);
    expectNoNetworking();
    // Reset
    setAllowNetworkingForProcess(true);
    expectHasNetworking();
}
