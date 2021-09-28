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

#ifndef NATIVE_BRIDGE_SUPPORT_LINKER_STATIC_TLS_CONFIG_H_
#define NATIVE_BRIDGE_SUPPORT_LINKER_STATIC_TLS_CONFIG_H_

#include <stdlib.h>

struct NativeBridgeStaticTlsConfig {
  // The size will be a multiple of the static TLS memory's alignment, which is
  // at most one page.
  size_t size;

  // The offset from the start of static TLS to the thread pointer.
  size_t tpoff;

  // Image to initialize the static TLS with. This image covers the entire area.
  const void* init_img;

  // The guest's value for TLS_SLOT_THREAD_ID.
  int tls_slot_thread_id;

  // The guest's value for TLS_SLOT_BIONIC_TLS.
  int tls_slot_bionic_tls;
};

#endif  // NATIVE_BRIDGE_SUPPORT_LINKER_STATIC_TLS_CONFIG_H_
