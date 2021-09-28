/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <poll.h> /* poll */
#include <resolv.h>
#include <string.h>
#include <sys/socket.h>

#include <android/multinetwork.h>
#include <gtest/gtest.h>

namespace {
constexpr int MAXPACKET = 8 * 1024;
constexpr int PTON_MAX = 16;
constexpr int TIMEOUT_MS = 10000;

int getAsyncResponse(int fd, int timeoutMs, int* rcode, uint8_t* buf, size_t bufLen) {
    struct pollfd wait_fd[1];
    wait_fd[0].fd = fd;
    wait_fd[0].events = POLLIN;
    short revents;
    int ret;
    ret = poll(wait_fd, 1, timeoutMs);
    revents = wait_fd[0].revents;
    if (revents & POLLIN) {
        int n = android_res_nresult(fd, rcode, buf, bufLen);
        // Verify that android_res_nresult() closed the fd
        char dummy;
        EXPECT_EQ(-1, read(fd, &dummy, sizeof dummy));
        EXPECT_EQ(EBADF, errno);
        return n;
    }

    return -1;
}

std::vector<std::string> extractIpAddressAnswers(uint8_t* buf, size_t bufLen, int ipType) {
    ns_msg handle;
    if (ns_initparse((const uint8_t*) buf, bufLen, &handle) < 0) {
        return {};
    }
    const int ancount = ns_msg_count(handle, ns_s_an);
    ns_rr rr;
    std::vector<std::string> answers;
    for (int i = 0; i < ancount; i++) {
        if (ns_parserr(&handle, ns_s_an, i, &rr) < 0) {
            continue;
        }
        const uint8_t* rdata = ns_rr_rdata(rr);
        char buffer[INET6_ADDRSTRLEN];
        if (inet_ntop(ipType, (const char*) rdata, buffer, sizeof(buffer))) {
            answers.push_back(buffer);
        }
    }
    return answers;
}

void expectAnswersValid(int fd, int ipType, int expectedRcode) {
    int rcode = -1;
    uint8_t buf[MAXPACKET] = {};
    int res = getAsyncResponse(fd, TIMEOUT_MS, &rcode, buf, MAXPACKET);
    EXPECT_GE(res, 0);
    EXPECT_EQ(rcode, expectedRcode);

    if (expectedRcode == ns_r_noerror) {
        auto answers = extractIpAddressAnswers(buf, res, ipType);
        EXPECT_GE(answers.size(), 0U);
        for (auto &answer : answers) {
            char pton[PTON_MAX];
            EXPECT_EQ(1, inet_pton(ipType, answer.c_str(), pton));
        }
    }
}

void expectAnswersNotValid(int fd, int expectedErrno) {
    int rcode = -1;
    uint8_t buf[MAXPACKET] = {};
    int res = getAsyncResponse(fd, TIMEOUT_MS, &rcode, buf, MAXPACKET);
    EXPECT_EQ(expectedErrno, res);
}

} // namespace

TEST (NativeDnsAsyncTest, Async_Query) {
    // V4
    int fd1 = android_res_nquery(
            NETWORK_UNSPECIFIED, "www.google.com", ns_c_in, ns_t_a, 0);
    EXPECT_GE(fd1, 0);
    int fd2 = android_res_nquery(
            NETWORK_UNSPECIFIED, "www.youtube.com", ns_c_in, ns_t_a, 0);
    EXPECT_GE(fd2, 0);
    expectAnswersValid(fd2, AF_INET, ns_r_noerror);
    expectAnswersValid(fd1, AF_INET, ns_r_noerror);

    // V6
    fd1 = android_res_nquery(
            NETWORK_UNSPECIFIED, "www.google.com", ns_c_in, ns_t_aaaa, 0);
    EXPECT_GE(fd1, 0);
    fd2 = android_res_nquery(
            NETWORK_UNSPECIFIED, "www.youtube.com", ns_c_in, ns_t_aaaa, 0);
    EXPECT_GE(fd2, 0);
    expectAnswersValid(fd2, AF_INET6, ns_r_noerror);
    expectAnswersValid(fd1, AF_INET6, ns_r_noerror);
}

TEST (NativeDnsAsyncTest, Async_Send) {
    // V4
    uint8_t buf1[MAXPACKET] = {};
    int len1 = res_mkquery(ns_o_query, "www.googleapis.com",
            ns_c_in, ns_t_a, nullptr, 0, nullptr, buf1, sizeof(buf1));
    EXPECT_GT(len1, 0);

    uint8_t buf2[MAXPACKET] = {};
    int len2 = res_mkquery(ns_o_query, "play.googleapis.com",
            ns_c_in, ns_t_a, nullptr, 0, nullptr, buf2, sizeof(buf2));
    EXPECT_GT(len2, 0);

    int fd1 = android_res_nsend(NETWORK_UNSPECIFIED, buf1, len1, 0);
    EXPECT_GE(fd1, 0);
    int fd2 = android_res_nsend(NETWORK_UNSPECIFIED, buf2, len2, 0);
    EXPECT_GE(fd2, 0);

    expectAnswersValid(fd2, AF_INET, ns_r_noerror);
    expectAnswersValid(fd1, AF_INET, ns_r_noerror);

    // V6
    memset(buf1, 0, sizeof(buf1));
    memset(buf2, 0, sizeof(buf2));
    len1 = res_mkquery(ns_o_query, "www.googleapis.com",
            ns_c_in, ns_t_aaaa, nullptr, 0, nullptr, buf1, sizeof(buf1));
    EXPECT_GT(len1, 0);
    len2 = res_mkquery(ns_o_query, "play.googleapis.com",
            ns_c_in, ns_t_aaaa, nullptr, 0, nullptr, buf2, sizeof(buf2));
    EXPECT_GT(len2, 0);

    fd1 = android_res_nsend(NETWORK_UNSPECIFIED, buf1, len1, 0);
    EXPECT_GE(fd1, 0);
    fd2 = android_res_nsend(NETWORK_UNSPECIFIED, buf2, len2, 0);
    EXPECT_GE(fd2, 0);

    expectAnswersValid(fd2, AF_INET6, ns_r_noerror);
    expectAnswersValid(fd1, AF_INET6, ns_r_noerror);
}

TEST (NativeDnsAsyncTest, Async_NXDOMAIN) {
    uint8_t buf[MAXPACKET] = {};
    int len = res_mkquery(ns_o_query, "test1-nx.metric.gstatic.com",
            ns_c_in, ns_t_a, nullptr, 0, nullptr, buf, sizeof(buf));
    EXPECT_GT(len, 0);
    int fd1 = android_res_nsend(NETWORK_UNSPECIFIED, buf, len, ANDROID_RESOLV_NO_CACHE_LOOKUP);
    EXPECT_GE(fd1, 0);

    len = res_mkquery(ns_o_query, "test2-nx.metric.gstatic.com",
            ns_c_in, ns_t_a, nullptr, 0, nullptr, buf, sizeof(buf));
    EXPECT_GT(len, 0);
    int fd2 = android_res_nsend(NETWORK_UNSPECIFIED, buf, len, ANDROID_RESOLV_NO_CACHE_LOOKUP);
    EXPECT_GE(fd2, 0);

    expectAnswersValid(fd2, AF_INET, ns_r_nxdomain);
    expectAnswersValid(fd1, AF_INET, ns_r_nxdomain);

    fd1 = android_res_nquery(
            NETWORK_UNSPECIFIED, "test3-nx.metric.gstatic.com",
            ns_c_in, ns_t_aaaa, ANDROID_RESOLV_NO_CACHE_LOOKUP);
    EXPECT_GE(fd1, 0);
    fd2 = android_res_nquery(
            NETWORK_UNSPECIFIED, "test4-nx.metric.gstatic.com",
            ns_c_in, ns_t_aaaa, ANDROID_RESOLV_NO_CACHE_LOOKUP);
    EXPECT_GE(fd2, 0);
    expectAnswersValid(fd2, AF_INET6, ns_r_nxdomain);
    expectAnswersValid(fd1, AF_INET6, ns_r_nxdomain);
}

TEST (NativeDnsAsyncTest, Async_Cancel) {
    int fd = android_res_nquery(
            NETWORK_UNSPECIFIED, "www.google.com", ns_c_in, ns_t_a, 0);
    errno = 0;
    android_res_cancel(fd);
    int err = errno;
    EXPECT_EQ(err, 0);
    // DO NOT call cancel or result with the same fd more than once,
    // otherwise it will hit fdsan double-close fd.
}

TEST (NativeDnsAsyncTest, Async_Query_MALFORMED) {
    // Empty string to create BLOB and query, we will get empty result and rcode = 0
    // on DNSTLS.
    int fd = android_res_nquery(
            NETWORK_UNSPECIFIED, "", ns_c_in, ns_t_a, 0);
    EXPECT_GE(fd, 0);
    expectAnswersValid(fd, AF_INET, ns_r_noerror);

    std::string exceedingLabelQuery = "www." + std::string(70, 'g') + ".com";
    std::string exceedingDomainQuery = "www." + std::string(255, 'g') + ".com";

    fd = android_res_nquery(NETWORK_UNSPECIFIED,
            exceedingLabelQuery.c_str(), ns_c_in, ns_t_a, 0);
    EXPECT_EQ(-EMSGSIZE, fd);
    fd = android_res_nquery(NETWORK_UNSPECIFIED,
            exceedingDomainQuery.c_str(), ns_c_in, ns_t_a, 0);
    EXPECT_EQ(-EMSGSIZE, fd);
}

TEST (NativeDnsAsyncTest, Async_Send_MALFORMED) {
    uint8_t buf[10] = {};
    // empty BLOB
    int fd = android_res_nsend(NETWORK_UNSPECIFIED, buf, 10, 0);
    EXPECT_GE(fd, 0);
    expectAnswersNotValid(fd, -EINVAL);

    std::vector<uint8_t> largeBuf(2 * MAXPACKET, 0);
    // A buffer larger than 8KB
    fd = android_res_nsend(
            NETWORK_UNSPECIFIED, largeBuf.data(), largeBuf.size(), 0);
    EXPECT_EQ(-EMSGSIZE, fd);

    // 5000 bytes filled with 0. This returns EMSGSIZE because FrameworkListener limits the size of
    // commands to 4096 bytes.
    fd = android_res_nsend(NETWORK_UNSPECIFIED, largeBuf.data(), 5000, 0);
    EXPECT_EQ(-EMSGSIZE, fd);

    // 500 bytes filled with 0
    fd = android_res_nsend(NETWORK_UNSPECIFIED, largeBuf.data(), 500, 0);
    EXPECT_GE(fd, 0);
    expectAnswersNotValid(fd, -EINVAL);

    // 5000 bytes filled with 0xFF
    std::vector<uint8_t> ffBuf(5000, 0xFF);
    fd = android_res_nsend(
            NETWORK_UNSPECIFIED, ffBuf.data(), ffBuf.size(), 0);
    EXPECT_EQ(-EMSGSIZE, fd);

    // 500 bytes filled with 0xFF
    fd = android_res_nsend(NETWORK_UNSPECIFIED, ffBuf.data(), 500, 0);
    EXPECT_GE(fd, 0);
    expectAnswersNotValid(fd, -EINVAL);
}
