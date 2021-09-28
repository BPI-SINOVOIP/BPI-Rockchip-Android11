//
// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// clang-format off
#include "native_bridge_support/vdso/interceptable_functions.h"

DEFINE_INTERCEPTABLE_STUB_FUNCTION(AMidiDevice_fromJava);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AMidiDevice_getNumInputPorts);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AMidiDevice_getNumOutputPorts);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AMidiDevice_getType);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AMidiDevice_release);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AMidiInputPort_close);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AMidiInputPort_open);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AMidiInputPort_send);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AMidiInputPort_sendFlush);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AMidiInputPort_sendWithTimestamp);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AMidiOutputPort_close);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AMidiOutputPort_open);
DEFINE_INTERCEPTABLE_STUB_FUNCTION(AMidiOutputPort_receive);

static void __attribute__((constructor(0))) init_stub_library() {
  INIT_INTERCEPTABLE_STUB_FUNCTION("libamidi.so", AMidiDevice_fromJava);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libamidi.so", AMidiDevice_getNumInputPorts);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libamidi.so", AMidiDevice_getNumOutputPorts);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libamidi.so", AMidiDevice_getType);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libamidi.so", AMidiDevice_release);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libamidi.so", AMidiInputPort_close);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libamidi.so", AMidiInputPort_open);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libamidi.so", AMidiInputPort_send);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libamidi.so", AMidiInputPort_sendFlush);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libamidi.so", AMidiInputPort_sendWithTimestamp);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libamidi.so", AMidiOutputPort_close);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libamidi.so", AMidiOutputPort_open);
  INIT_INTERCEPTABLE_STUB_FUNCTION("libamidi.so", AMidiOutputPort_receive);
}
// clang-format on
