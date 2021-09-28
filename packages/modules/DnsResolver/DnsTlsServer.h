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

#pragma once

#include <chrono>
#include <set>
#include <string>
#include <vector>

#include <netinet/in.h>

#include <params.h>

namespace android {
namespace net {

// DnsTlsServer represents a recursive resolver that supports, or may support, a
// secure protocol.
struct DnsTlsServer {
    // Default constructor.
    DnsTlsServer() {}

    // Allow sockaddr_storage to be promoted to DnsTlsServer automatically.
    DnsTlsServer(const sockaddr_storage& ss) : ss(ss) {}

    // The server location, including IP and port.
    sockaddr_storage ss = {};

    // The server's hostname.  If this string is nonempty, the server must present a
    // certificate that indicates this name and has a valid chain to a trusted root CA.
    std::string name;

    // The certificate of the CA that signed the server's certificate.
    // It is used to store temporary test CA certificate for internal tests.
    std::string certificate;

    // Placeholder.  More protocols might be defined in the future.
    int protocol = IPPROTO_TCP;

    // The time to wait for the attempt on connecting to the server.
    // Set the default value 127 seconds to be consistent with TCP connect timeout.
    // (presume net.ipv4.tcp_syn_retries = 6)
    static constexpr std::chrono::milliseconds kDotConnectTimeoutMs =
            std::chrono::milliseconds(127 * 1000);
    std::chrono::milliseconds connectTimeout = kDotConnectTimeoutMs;

    // Exact comparison of DnsTlsServer objects
    bool operator<(const DnsTlsServer& other) const;
    bool operator==(const DnsTlsServer& other) const;

    bool wasExplicitlyConfigured() const;
};

// This comparison only checks the IP address.  It ignores ports, names, and fingerprints.
struct AddressComparator {
    bool operator()(const DnsTlsServer& x, const DnsTlsServer& y) const;
};

}  // namespace net
}  // namespace android
