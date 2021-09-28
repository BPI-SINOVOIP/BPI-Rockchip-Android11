/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <poll.h> /* poll */
#include <sys/socket.h>

#include <thread>

#include <android-base/parseint.h>
#include <android-base/unique_fd.h>
#include <gtest/gtest.h>

#include "NetdClient.h"
#include "netdclient_priv.h"

namespace {

// Keep in sync with FrameworkListener.cpp (500, "Command not recognized")
constexpr char NOT_SUPPORT_MSG[] = "500 Command not recognized";

int openDnsProxyFuncStub() {
    return -1;
};

typedef int (*DnsOpenProxyType)();
typedef int (*SocketFunctionType)(int, int, int);

DnsOpenProxyType openDnsProxyFuncPtr = openDnsProxyFuncStub;
SocketFunctionType socketFuncPtr = socket;

void serverLoop(int dnsProxyFd) {
    while (true) {
        pollfd fds[1] = {{.fd = dnsProxyFd, .events = POLLIN}};
        enum { SERVERFD = 0 };

        int poll_result = TEMP_FAILURE_RETRY(poll(fds, std::size(fds), -1));
        ASSERT_GT(poll_result, 0);

        if (fds[SERVERFD].revents & POLLERR) return;
        if (fds[SERVERFD].revents & POLLIN) {
            char buf[4096];
            TEMP_FAILURE_RETRY(read(fds[SERVERFD].fd, &buf, sizeof(buf)));
            // TODO: verify command
            TEMP_FAILURE_RETRY(write(fds[SERVERFD].fd, NOT_SUPPORT_MSG, sizeof(NOT_SUPPORT_MSG)));
        }
    }
}

void expectAllowNetworkingForProcess() {
    // netdClientSocket
    android::base::unique_fd ipv4(socketFuncPtr(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0)),
            ipv6(socketFuncPtr(AF_INET6, SOCK_STREAM | SOCK_CLOEXEC, 0));
    EXPECT_LE(3, ipv4);
    EXPECT_LE(3, ipv6);

    // dns_open_proxy
    android::base::unique_fd dnsproxydSocket(openDnsProxyFuncPtr());
    EXPECT_LE(3, dnsproxydSocket);
}

void expectNotAllowNetworkingForProcess() {
    // netdClientSocket
    android::base::unique_fd unixSocket(socketFuncPtr(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
    EXPECT_LE(3, unixSocket);
    android::base::unique_fd ipv4(socketFuncPtr(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
    EXPECT_EQ(-1, ipv4);
    EXPECT_EQ(errno, EPERM);
    android::base::unique_fd ipv6(socketFuncPtr(AF_INET6, SOCK_STREAM | SOCK_CLOEXEC, 0));
    EXPECT_EQ(-1, ipv6);
    EXPECT_EQ(errno, EPERM);

    // dns_open_proxy
    android::base::unique_fd dnsproxydSocket(openDnsProxyFuncPtr());
    EXPECT_EQ(-1, dnsproxydSocket);
    EXPECT_EQ(errno, EPERM);
}

}  // namespace

TEST(NetdClientTest, getNetworkForDnsInternal) {
    // Test invalid fd
    unsigned dnsNetId = 0;
    const int invalidFd = -1;
    EXPECT_EQ(-EBADF, getNetworkForDnsInternal(invalidFd, &dnsNetId));

    // Test what the client does if the resolver does not support the "getdnsnetid" command.
    android::base::unique_fd clientFd, serverFd;
    ASSERT_TRUE(android::base::Socketpair(AF_UNIX, &clientFd, &serverFd));

    std::thread serverThread = std::thread(serverLoop, serverFd.get());

    EXPECT_EQ(-EOPNOTSUPP, getNetworkForDnsInternal(clientFd.get(), &dnsNetId));

    clientFd.reset();  // Causes serverLoop() to exit
    serverThread.join();
}

TEST(NetdClientTest, getNetworkForDns) {
    // Test null input
    unsigned* testNull = nullptr;
    EXPECT_EQ(-EFAULT, getNetworkForDns(testNull));
}

TEST(NetdClientTest, protectFromVpnBadFd) {
    EXPECT_EQ(-EBADF, protectFromVpn(-1));
}

TEST(NetdClientTest, protectFromVpnUnixStream) {
    int s = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    ASSERT_GE(s, 3);
    EXPECT_EQ(-EAFNOSUPPORT, protectFromVpn(s));
    close(s);
}

TEST(NetdClientTest, protectFromVpnTcp6) {
    int s = socket(AF_INET6, SOCK_STREAM | SOCK_CLOEXEC, 0);
    ASSERT_GE(s, 3);
    EXPECT_EQ(0, protectFromVpn(s));
    close(s);
}

TEST(NetdClientTest, setAllowNetworkingForProcess) {
    netdClientInitDnsOpenProxy(&openDnsProxyFuncPtr);
    netdClientInitSocket(&socketFuncPtr);
    // At the beginning, we should be able to use socket since the default setting is allowing.
    expectAllowNetworkingForProcess();
    // Disable
    setAllowNetworkingForProcess(false);
    expectNotAllowNetworkingForProcess();
    // Reset
    setAllowNetworkingForProcess(true);
    expectAllowNetworkingForProcess();
}
