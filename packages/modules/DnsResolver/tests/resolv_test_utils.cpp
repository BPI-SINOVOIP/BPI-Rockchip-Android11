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
 *
 */

#include "resolv_test_utils.h"

#include <arpa/inet.h>

#include <android-base/chrono_utils.h>
#include <netdutils/InternetAddresses.h>

using android::netdutils::ScopedAddrinfo;

std::string ToString(const hostent* he) {
    if (he == nullptr) return "<null>";
    char buffer[INET6_ADDRSTRLEN];
    if (!inet_ntop(he->h_addrtype, he->h_addr_list[0], buffer, sizeof(buffer))) {
        return "<invalid>";
    }
    return buffer;
}

std::string ToString(const addrinfo* ai) {
    if (!ai) return "<null>";

    char host[NI_MAXHOST];
    int rv = getnameinfo(ai->ai_addr, ai->ai_addrlen, host, sizeof(host), nullptr, 0,
                         NI_NUMERICHOST);
    if (rv != 0) return gai_strerror(rv);
    return host;
}

std::string ToString(const ScopedAddrinfo& ai) {
    return ToString(ai.get());
}

std::string ToString(const sockaddr_storage* addr) {
    if (!addr) return "<null>";
    char host[NI_MAXHOST];
    int rv = getnameinfo((const sockaddr*)addr, sizeof(sockaddr_storage), host, sizeof(host),
                         nullptr, 0, NI_NUMERICHOST);
    if (rv != 0) return gai_strerror(rv);
    return host;
}

std::vector<std::string> ToStrings(const hostent* he) {
    std::vector<std::string> hosts;
    if (he == nullptr) {
        hosts.push_back("<null>");
        return hosts;
    }
    uint32_t i = 0;
    while (he->h_addr_list[i] != nullptr) {
        char host[INET6_ADDRSTRLEN];
        if (!inet_ntop(he->h_addrtype, he->h_addr_list[i], host, sizeof(host))) {
            hosts.push_back("<invalid>");
            return hosts;
        } else {
            hosts.push_back(host);
        }
        i++;
    }
    if (hosts.empty()) hosts.push_back("<invalid>");
    return hosts;
}

std::vector<std::string> ToStrings(const addrinfo* ai) {
    std::vector<std::string> hosts;
    if (!ai) {
        hosts.push_back("<null>");
        return hosts;
    }
    for (const auto* aip = ai; aip != nullptr; aip = aip->ai_next) {
        char host[NI_MAXHOST];
        int rv = getnameinfo(aip->ai_addr, aip->ai_addrlen, host, sizeof(host), nullptr, 0,
                             NI_NUMERICHOST);
        if (rv != 0) {
            hosts.clear();
            hosts.push_back(gai_strerror(rv));
            return hosts;
        } else {
            hosts.push_back(host);
        }
    }
    if (hosts.empty()) hosts.push_back("<invalid>");
    return hosts;
}

std::vector<std::string> ToStrings(const ScopedAddrinfo& ai) {
    return ToStrings(ai.get());
}

size_t GetNumQueries(const test::DNSResponder& dns, const char* name) {
    std::vector<test::DNSResponder::QueryInfo> queries = dns.queries();
    size_t found = 0;
    for (const auto& p : queries) {
        if (p.name == name) {
            ++found;
        }
    }
    return found;
}

size_t GetNumQueriesForProtocol(const test::DNSResponder& dns, const int protocol,
                                const char* name) {
    std::vector<test::DNSResponder::QueryInfo> queries = dns.queries();
    size_t found = 0;
    for (const auto& p : queries) {
        if (p.protocol == protocol && p.name == name) {
            ++found;
        }
    }
    return found;
}

size_t GetNumQueriesForType(const test::DNSResponder& dns, ns_type type, const char* name) {
    std::vector<test::DNSResponder::QueryInfo> queries = dns.queries();
    size_t found = 0;
    for (const auto& p : queries) {
        if (p.type == type && p.name == name) {
            ++found;
        }
    }
    return found;
}

bool PollForCondition(const std::function<bool()>& condition, std::chrono::milliseconds timeout) {
    constexpr std::chrono::milliseconds retryIntervalMs{5};
    android::base::Timer t;
    while (t.duration() < timeout) {
        if (condition()) return true;
        std::this_thread::sleep_for(retryIntervalMs);
    }
    return false;
}
