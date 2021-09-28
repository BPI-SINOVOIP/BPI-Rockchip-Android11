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

#include <netdb.h>               // struct hostent
#include "netd_resolv/resolv.h"  // struct android_net_context
#include "stats.pb.h"

/*
 * Error code extending EAI_* codes defined in bionic/libc/include/netdb.h.
 * This error code, including EAI_*, returned from android_getaddrinfofornetcontext()
 * and resolv_gethostbyname() are used for DNS metrics.
 */
#define NETD_RESOLV_TIMEOUT 255  // consistent with RCODE_TIMEOUT

// This is the entry point for the gethostbyname() family of legacy calls.
int resolv_gethostbyname(const char* name, int af, hostent* hp, char* buf, size_t buflen,
                         const android_net_context* netcontext, hostent** result,
                         android::net::NetworkDnsEventReported* event);

// This is the entry point for the gethostbyaddr() family of legacy calls.
int resolv_gethostbyaddr(const void* addr, socklen_t len, int af, hostent* hp, char* buf,
                         size_t buflen, const android_net_context* netcontext, hostent** result,
                         android::net::NetworkDnsEventReported* event);
