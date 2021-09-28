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

#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ipv6Monitor;

// A callback for when the IPv6 configuration changes.
typedef void (*ipv6MonitorCallback)(const struct in6_addr* /*gateway*/,
                                    const struct in6_addr* /*dns servers*/,
                                    size_t /*number of dns servers */);

// Create an IPv6 monitor that will monitor |interfaceName| for IPv6 router
// advertisements. The monitor will trigger a callback if the gateway and/or
// DNS servers provided by router advertisements change at any point.
struct ipv6Monitor* ipv6MonitorCreate(const char* interfaceName);
void ipv6MonitorFree(struct ipv6Monitor* monitor);

void ipv6MonitorSetCallback(struct ipv6Monitor* monitor,
                            ipv6MonitorCallback callback);
void ipv6MonitorRunAsync(struct ipv6Monitor* monitor);
void ipv6MonitorStop(struct ipv6Monitor* monitor);

#ifdef __cplusplus
} // extern "C"
#endif


