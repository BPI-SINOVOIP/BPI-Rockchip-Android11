/*
 * Copyright 2018, The Android Open Source Project
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

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ifMonitor;

struct ifAddress {
    int family;
    int prefix;
    unsigned char addr[16];
};

// A callback for when the addresses on an interface changes
typedef void (*ifMonitorCallback)(unsigned int /*interface index*/,
                                  const struct ifAddress* /*addresses*/,
                                  size_t /*number of addresses */);

struct ifMonitor* ifMonitorCreate();
void ifMonitorFree(struct ifMonitor* monitor);

void ifMonitorSetCallback(struct ifMonitor* monitor,
                          ifMonitorCallback callback);
void ifMonitorRunAsync(struct ifMonitor* monitor);
void ifMonitorStop(struct ifMonitor* monitor);

#ifdef __cplusplus
} // extern "C"
#endif

