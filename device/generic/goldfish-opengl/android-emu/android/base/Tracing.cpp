// Copyright (C) 2019 The Android Open Source Project
// Copyright (C) 2019 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "android/base/Tracing.h"

#if defined(__ANDROID__) || defined(HOST_BUILD)

#include <cutils/trace.h>

#define VK_TRACE_TAG ATRACE_TAG_GRAPHICS

namespace android {
namespace base {

void ScopedTrace::beginTraceImpl(const char* name) {
    atrace_begin(VK_TRACE_TAG, name);
}

void ScopedTrace::endTraceImpl(const char*) {
    atrace_end(VK_TRACE_TAG);
}

} // namespace base
} // namespace android

#elif __Fuchsia__

#ifndef FUCHSIA_NO_TRACE
#include <lib/trace/event.h>
#endif

#define VK_TRACE_TAG "gfx"

namespace android {
namespace base {

void ScopedTrace::beginTraceImpl(const char* name) {
#ifndef FUCHSIA_NO_TRACE
    TRACE_DURATION_BEGIN(VK_TRACE_TAG, name);
#endif
}

void ScopedTrace::endTraceImpl(const char* name) {
#ifndef FUCHSIA_NO_TRACE
    TRACE_DURATION_END(VK_TRACE_TAG, name);
#endif
}

} // namespace base
} // namespace android

#endif

