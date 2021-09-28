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

#pragma once

#include <iostream>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <stats.pb.h>

namespace android::net {

android::net::NetworkDnsEventReported fromNetworkDnsEventReportedStr(const std::string& str);

// Used for gmock printing our desired debug messages. (Another approach is defining operator<<())
void PrintTo(const DnsQueryEvents& event, std::ostream* os);
void PrintTo(const DnsQueryEvent& event, std::ostream* os);
void PrintTo(const android::net::NetworkDnsEventReported& event, std::ostream* os);

MATCHER_P(DnsQueryEventEq, other, "") {
    return ::testing::ExplainMatchResult(
            ::testing::AllOf(
                    ::testing::Property("rcode", &DnsQueryEvent::rcode,
                                        ::testing::Eq(other.rcode())),
                    ::testing::Property("ns_type", &DnsQueryEvent::type,
                                        ::testing::Eq(other.type())),
                    ::testing::Property("cache_hit", &DnsQueryEvent::cache_hit,
                                        ::testing::Eq(other.cache_hit())),
                    ::testing::Property("ip_version", &DnsQueryEvent::ip_version,
                                        ::testing::Eq(other.ip_version())),
                    ::testing::Property("protocol", &DnsQueryEvent::protocol,
                                        ::testing::Eq(other.protocol())),
                    ::testing::Property("retry_times", &DnsQueryEvent::retry_times,
                                        ::testing::Eq(other.retry_times())),
                    ::testing::Property("dns_server_index", &DnsQueryEvent::dns_server_index,
                                        ::testing::Eq(other.dns_server_index())),
                    // Removing the latency check, because we can't predict the time.
                    /*  ::testing::Property("latency_micros", &DnsQueryEvent::latency_micros,
                             ::testing::Eq(other.latency_micros())),*/
                    ::testing::Property("linux_errno", &DnsQueryEvent::linux_errno,
                                        ::testing::Eq(other.linux_errno())),
                    ::testing::Property("connected", &DnsQueryEvent::connected,
                                        ::testing::Eq(other.connected()))),
            arg, result_listener);
}

MATCHER_P(DnsQueryEventsEq, other, "") {
    const int eventSize = arg.dns_query_event_size();
    if (eventSize != other.dns_query_event_size()) {
        *result_listener << "Expected dns query event size: " << other.dns_query_event_size()
                         << " \n";
        for (int i = 0; i < other.dns_query_event_size(); ++i)
            PrintTo(other.dns_query_event(i), result_listener->stream());
        *result_listener << "Actual dns query event size: " << eventSize << "\n";
        for (int i = 0; i < eventSize; ++i)
            PrintTo(arg.dns_query_event(i), result_listener->stream());
        return false;
    }

    for (int i = 0; i < eventSize; ++i) {
        bool result =
                ::testing::Value(arg.dns_query_event(i), DnsQueryEventEq(other.dns_query_event(i)));
        if (!result) {
            *result_listener << "Expected event num: " << i << " \n";
            PrintTo(arg.dns_query_event(i), result_listener->stream());
            *result_listener << "Actual event num: " << i << " ";
            PrintTo(other.dns_query_event(i), result_listener->stream());
            return false;
        }
    }
    return true;
}

MATCHER_P(NetworkDnsEventEq, other, "") {
    return ::testing::ExplainMatchResult(
            ::testing::AllOf(
                    // Removing following fields check, because we can't verify those fields in unit
                    // test.
                    /*
                    ::testing::Property("event_type",
                                        &android::net::NetworkDnsEventReported::event_type,
                                        ::testing::Eq(other.event_type())),
                    ::testing::Property("return_code",
                                        &android::net::NetworkDnsEventReported::return_code,
                                        ::testing::Eq(other.return_code())),
                    ::testing::Property("latency_micros",
                       &android::net::NetworkDnsEventReported::latency_micros,
                              ::testing::Eq(other.latency_micros())),
                    ::testing::Property("hints_ai_flags",
                                        &android::net::NetworkDnsEventReported::hints_ai_flags,
                                        ::testing::Eq(other.hints_ai_flags())),
                    ::testing::Property("res_nsend_flags",
                                        &android::net::NetworkDnsEventReported::res_nsend_flags,
                                        ::testing::Eq(other.res_nsend_flags())),
                    ::testing::Property("network_type",
                                        &android::net::NetworkDnsEventReported::network_type,
                                        ::testing::Eq(other.network_type())),
                    ::testing::Property("private_dns_modes",
                                        &android::net::NetworkDnsEventReported::private_dns_modes,
                                        ::testing::Eq(other.private_dns_modes())),
                    */
                    ::testing::Property("dns_query_events",
                                        &android::net::NetworkDnsEventReported::dns_query_events,
                                        DnsQueryEventsEq(other.dns_query_events()))),
            arg, result_listener);
}

}  // namespace android::net
