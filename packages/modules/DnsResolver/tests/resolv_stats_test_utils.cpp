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

#include "resolv_stats_test_utils.h"

#include <iostream>
#include <regex>

#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>
#include <gmock/gmock.h>
#include <stats.pb.h>

namespace android::net {

using android::net::NetworkDnsEventReported;

// The fromNetworkDnsEventReportedStr is a function to generate a protocol buffer message
// NetworkDnsEventReported from a string. How to define the sting for NetworkDnsEventReported,
// please refer to test case AlphabeticalHostname.
// There are 3 proto messages(1. NetworkDnsEventReported 2. dns_query_events 3. dns_query_event)
// The names of these 3 messages will not be verified, please do not change.
// Each field will be parsed into corresponding message by those char pairs "{" & "}".
// Example: (Don't need to fill Level 1 fields in NetworkDnsEventReported. Currently,
// no verification is performed.)
// NetworkDnsEventReported { <= construct the NetworkDnsEventReported message (Level 1)
//  dns_query_events:(Level 2)
//  { <= construct the dns_query_events message
//   dns_query_event:(Level 3)
//    {  <= construct the 1st dns_query_event message
//     [field]: value,
//     [field]: value,
//    } <= finish the 1st dns_query_event message
//   dns_query_event:(Level 3)
//    {  <= construct 2nd dns_query_event message
//     [field]: value,
//     [field]: value,
//    } <= finish the 1nd dns_query_event message
//  } <= finish the dns_query_events message
// } <= finish the NetworkDnsEventReported message

NetworkDnsEventReported fromNetworkDnsEventReportedStr(const std::string& str) {
    using android::base::ParseInt;
    using android::base::Split;
    // Remove unnecessary space
    std::regex re(": ");
    std::string s = std::regex_replace(str, re, ":");
    // Using space to separate each parse line
    static const std::regex words_regex("[^\\s]+");
    auto words_begin = std::sregex_iterator(s.begin(), s.end(), words_regex);
    auto words_end = std::sregex_iterator();
    // Using strproto to identify the position of NetworkDnsEventReported proto
    int strproto = 0;
    NetworkDnsEventReported event;
    DnsQueryEvent* dnsQueryEvent = nullptr;
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::string match_str = (*i).str();
        // Using "{" and "}" to identify the Start/End of each proto
        if (match_str == "{") {
            // strproto 1.NetworkDnsEventReported 2.dns_query_events 3.dns_query_event
            if (++strproto == 3) {
                dnsQueryEvent = event.mutable_dns_query_events()->add_dns_query_event();
            }
            continue;
        }
        if (match_str == "}" | match_str == "},") {
            strproto--;
            continue;
        }
        // Parsing each field of the proto and fill it into NetworkDnsEventReported event
        static const std::regex pieces_regex("([a-zA-Z0-9_]+)\\:([0-9]+),");
        std::smatch protoField;
        std::regex_match(match_str, protoField, pieces_regex);
        int value = 0;
        LOG(DEBUG) << "Str:" << match_str << " Name:" << protoField[1]
                   << " Value:" << protoField[2];
        // Parsing each field of the proto NetworkDnsEventReported
        if (strproto == 1) {
            if (protoField[1] == "event_type" && ParseInt(protoField[2], &value)) {
                event.set_event_type(static_cast<EventType>(value));
            } else if (protoField[1] == "return_code" && ParseInt(protoField[2], &value)) {
                event.set_return_code(static_cast<ReturnCode>(value));
            } else if (protoField[1] == "latency_micros" && ParseInt(protoField[2], &value)) {
                event.set_latency_micros(value);
            } else if (protoField[1] == "hints_ai_flags" && ParseInt(protoField[2], &value)) {
                event.set_hints_ai_flags(value);
            } else if (protoField[1] == "res_nsend_flags" && ParseInt(protoField[2], &value)) {
                event.set_res_nsend_flags(value);
            } else if (protoField[1] == "network_type" && ParseInt(protoField[2], &value)) {
                event.set_network_type(static_cast<NetworkType>(value));
            } else if (protoField[1] == "private_dns_modes" && ParseInt(protoField[2], &value)) {
                event.set_private_dns_modes(static_cast<PrivateDnsModes>(value));
            } else if (protoField[1] == "sampling_rate_denom" && ParseInt(protoField[2], &value)) {
                event.set_sampling_rate_denom(value);
            }
        }
        // Parsing each field of the proto DnsQueryEvent
        if (strproto == 3) {
            if (dnsQueryEvent == nullptr) continue;
            if (protoField[1] == "rcode" && ParseInt(protoField[2], &value)) {
                dnsQueryEvent->set_rcode(static_cast<NsRcode>(value));
            } else if (protoField[1] == "type" && ParseInt(protoField[2], &value)) {
                dnsQueryEvent->set_type(static_cast<NsType>(value));
            } else if (protoField[1] == "cache_hit" && ParseInt(protoField[2], &value)) {
                dnsQueryEvent->set_cache_hit(static_cast<CacheStatus>(value));
            } else if (protoField[1] == "ip_version" && ParseInt(protoField[2], &value)) {
                dnsQueryEvent->set_ip_version(static_cast<IpVersion>(value));
            } else if (protoField[1] == "protocol" && ParseInt(protoField[2], &value)) {
                dnsQueryEvent->set_protocol(static_cast<Protocol>(value));
            } else if (protoField[1] == "retry_times" && ParseInt(protoField[2], &value)) {
                dnsQueryEvent->set_retry_times(value);
            } else if (protoField[1] == "dns_server_index" && ParseInt(protoField[2], &value)) {
                dnsQueryEvent->set_dns_server_index(value);
            } else if (protoField[1] == "connected" && ParseInt(protoField[2], &value)) {
                dnsQueryEvent->set_connected(static_cast<bool>(value));
            } else if (protoField[1] == "latency_micros" && ParseInt(protoField[2], &value)) {
                dnsQueryEvent->set_latency_micros(value);
            } else if (protoField[1] == "linux_errno" && ParseInt(protoField[2], &value)) {
                dnsQueryEvent->set_linux_errno(static_cast<LinuxErrno>(value));
            }
        }
    }
    return event;
}

void PrintTo(const DnsQueryEvents& event, std::ostream* os) {
    *os << "query events: {\n";
    *os << "  dns_query_event_size: " << event.dns_query_event_size() << "\n";
    *os << "}";
}

void PrintTo(const DnsQueryEvent& event, std::ostream* os) {
    *os << "dns query event: {\n";
    *os << "  rcode: " << event.rcode() << "\n";
    *os << "  ns_type: " << event.type() << "\n";
    *os << "  cache_hit: " << event.cache_hit() << "\n";
    *os << "  ip_version: " << event.ip_version() << "\n";
    *os << "  protocol: " << event.protocol() << "\n";
    *os << "  retry_times: " << event.retry_times() << "\n";
    *os << "  dns_server_index: " << event.dns_server_index() << "\n";
    *os << "  connected: " << event.connected() << "\n";
    *os << "  latency_micros: " << event.latency_micros() << "\n";
    *os << "  linux_errno: " << event.linux_errno() << "\n";
    *os << "}";
}

void PrintTo(const NetworkDnsEventReported& event, std::ostream* os) {
    *os << "network dns event: {\n";
    *os << "  event_type: " << event.event_type() << "\n";
    *os << "  return_code: " << event.return_code() << "\n";
    *os << "  latency_micros: " << event.latency_micros() << "\n";
    *os << "  hints_ai_flags: " << event.hints_ai_flags() << "\n";
    *os << "  res_nsend_flags: " << event.res_nsend_flags() << "\n";
    *os << "  network_type: " << event.network_type() << "\n";
    *os << "  private_dns_modes: " << event.private_dns_modes() << "\n";
    *os << "  dns_query_event_size: " << event.dns_query_events().dns_query_event_size() << "\n";
    *os << "}";
}

}  // namespace android::net
