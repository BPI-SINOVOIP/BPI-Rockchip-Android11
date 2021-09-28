// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_V4L2_CODEC2_PLUGIN_STORE_V4L2_ALLOCATOR_ID_H
#define ANDROID_V4L2_CODEC2_PLUGIN_STORE_V4L2_ALLOCATOR_ID_H

#include <C2PlatformSupport.h>

namespace android {
namespace V4L2AllocatorId {

// The allocator ids used for V4L2DecodeComponent.
enum : C2AllocatorStore::id_t {
    V4L2_BUFFERQUEUE = C2PlatformAllocatorStore::PLATFORM_END,
    V4L2_BUFFERPOOL,
    SECURE_LINEAR,
    SECURE_GRAPHIC,
};

}  // namespace V4L2AllocatorId
}  // namespace android

#endif  // ANDROID_V4L2_CODEC2_PLUGIN_STORE_V4L2_ALLOCATOR_ID_H
