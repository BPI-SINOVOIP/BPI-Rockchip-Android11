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

#include <android-base/strings.h>
#include <gtest/gtest.h>
#include <resolv_stats_test_utils.h>
#include <stats.pb.h>

namespace android::net {

TEST(ResolvStatsUtils, NetworkDnsEventEq) {
    NetworkDnsEventReported event1;
    // Following fields will not be verified during the test in proto NetworkDnsEventReported.
    // So don't need to config those values: event_type, return_code, latency_micros,
    // hints_ai_flags, res_nsend_flags, network_type, private_dns_modes.
    constexpr char event2[] = R"Event(
             NetworkDnsEventReported {
             dns_query_events:
             {
               dns_query_event:[
                {
                 rcode: 3,
                 type: 28,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 3,
                 retry_times: 28,
                 dns_server_index: 0,
                 connected: 1,
                 latency_micros: 5,
                },
                {
                 rcode: 0,
                 type: 1,
                 cache_hit: 1,
                 ip_version: 1,
                 protocol: 1,
                 retry_times: 56,
                 dns_server_index: 1,
                 connected: 0,
                 latency_micros: 0,
                }
               ]
             }
        })Event";

    // TODO: Add integration test to verify Level 1 fields of NetworkDnsEventReported.
    // Level 1 fields, including event_type, return_code, hints_ai_flags, network_type, etc.
    DnsQueryEvent* dnsQueryEvent1 = event1.mutable_dns_query_events()->add_dns_query_event();
    dnsQueryEvent1->set_rcode(NS_R_NXDOMAIN);
    dnsQueryEvent1->set_type(NS_T_AAAA);
    dnsQueryEvent1->set_cache_hit(CS_NOTFOUND);
    dnsQueryEvent1->set_ip_version(IV_IPV4);
    dnsQueryEvent1->set_protocol(PROTO_DOT);
    dnsQueryEvent1->set_retry_times(28);
    dnsQueryEvent1->set_dns_server_index(0);
    dnsQueryEvent1->set_connected(1);
    dnsQueryEvent1->set_latency_micros(5);
    DnsQueryEvent* dnsQueryEvent2 = event1.mutable_dns_query_events()->add_dns_query_event();
    dnsQueryEvent2->set_rcode(NS_R_NO_ERROR);
    dnsQueryEvent2->set_type(NS_T_A);
    dnsQueryEvent2->set_cache_hit(CS_NOTFOUND);
    dnsQueryEvent2->set_ip_version(IV_IPV4);
    dnsQueryEvent2->set_protocol(PROTO_UDP);
    dnsQueryEvent2->set_retry_times(56);
    dnsQueryEvent2->set_dns_server_index(1);
    dnsQueryEvent2->set_connected(0);
    dnsQueryEvent2->set_latency_micros(5);
    EXPECT_THAT(event1, NetworkDnsEventEq(fromNetworkDnsEventReportedStr(event2)));
}

}  // namespace android::net
