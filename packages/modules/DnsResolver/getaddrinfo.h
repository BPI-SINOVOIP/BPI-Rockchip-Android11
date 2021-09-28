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

#pragma once

#include "netd_resolv/resolv.h"  // struct android_net_context
#include "stats.pb.h"

struct addrinfo;

int android_getaddrinfofornetcontext(const char* hostname, const char* servname,
                                     const addrinfo* hints, const android_net_context* netcontext,
                                     addrinfo** res, android::net::NetworkDnsEventReported*);

// This is the DNS proxy entry point for getaddrinfo().
int resolv_getaddrinfo(const char* hostname, const char* servname, const addrinfo* hints,
                       const android_net_context* netcontext, addrinfo** res,
                       android::net::NetworkDnsEventReported*);
