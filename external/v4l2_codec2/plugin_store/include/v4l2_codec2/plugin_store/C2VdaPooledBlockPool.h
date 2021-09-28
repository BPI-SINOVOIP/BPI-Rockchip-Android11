// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_V4L2_CODEC2_PLUGIN_STORE_C2_VDA_POOLED_BLOCK_POOL_H
#define ANDROID_V4L2_CODEC2_PLUGIN_STORE_C2_VDA_POOLED_BLOCK_POOL_H

#include <memory>
#include <mutex>
#include <optional>
#include <set>

#include <C2Buffer.h>
#include <C2BufferPriv.h>
#include <C2PlatformSupport.h>
#include <android-base/thread_annotations.h>

namespace android {

class C2VdaPooledBlockPool : public C2PooledBlockPool {
public:
    using C2PooledBlockPool::C2PooledBlockPool;
    ~C2VdaPooledBlockPool() override = default;

    // Extracts the buffer ID from BufferPoolData of the graphic block.
    // |block| is the graphic block allocated by bufferpool block pool.
    static std::optional<uint32_t> getBufferIdFromGraphicBlock(const C2Block2D& block);

    // Allocate the specified number of buffers.
    // |bufferCount| is the number of requested buffers.
    c2_status_t requestNewBufferSet(int32_t bufferCount);

    // Return C2_OK and store a buffer in |block| if a buffer is successfully fetched.
    // Return C2_TIMED_OUT if the pool already allocated |mBufferCount| buffers but they are all in
    // use.
    // Return C2_NO_MEMORY if the pool fails to allocate a new buffer.
    c2_status_t fetchGraphicBlock(uint32_t width, uint32_t height, uint32_t format,
                                  C2MemoryUsage usage,
                                  std::shared_ptr<C2GraphicBlock>* block /* nonnull */) override;

private:
    // Function mutex to lock at the start of each API function call for protecting the
    // synchronization of all member variables.
    std::mutex mMutex;

    // The ids of all allocated buffers.
    std::set<uint32_t> mBufferIds GUARDED_BY(mMutex);
    // The maximum count of allocated buffers.
    size_t mBufferCount GUARDED_BY(mMutex){0};
    // The timestamp for the next fetchGraphicBlock() call.
    // Set when the previous fetchGraphicBlock() call timed out.
    int64_t mNextFetchTimeUs GUARDED_BY(mMutex){0};
};

}  // namespace android
#endif  // ANDROID_V4L2_CODEC2_PLUGIN_STORE_C2_VDA_POOLED_BLOCK_POOL_H
