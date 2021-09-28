// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_V4L2_CODEC2_PLUGIN_STORE_VENDOR_ALLOCATOR_LOADER_H
#define ANDROID_V4L2_CODEC2_PLUGIN_STORE_VENDOR_ALLOCATOR_LOADER_H

#include <memory>
#include <mutex>

#include <C2Buffer.h>

namespace android {

// This class is for loading the vendor-specific C2Allocator implementations.
// The vendor should implement the shared library "libv4l2_codec2_vendor_allocator.so"
// and expose the function "C2Allocator* CreateAllocator(C2Allocator::id_t allocatorId);".
class VendorAllocatorLoader {
public:
    using CreateAllocatorFunc = ::C2Allocator* (*)(C2Allocator::id_t /* allocatorId */);

    static std::unique_ptr<VendorAllocatorLoader> Create();
    ~VendorAllocatorLoader();

    // Delegate to the vendor's shared library. |allocatorId| should be one of enum listed at
    // V4L2AllocatorId.h.
    C2Allocator* createAllocator(C2Allocator::id_t allocatorId);

private:
    VendorAllocatorLoader(void* libHandle, CreateAllocatorFunc createAllocatorFunc);

    void* mLibHandle;
    CreateAllocatorFunc mCreateAllocatorFunc;
};

}  // namespace android
#endif  // ANDROID_V4L2_CODEC2_PLUGIN_STORE_VENDOR_ALLOCATOR_LOADER_H
