// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_V4L2_CODEC2_COMPONENTS_VIDEO_FRAME_POOL_H
#define ANDROID_V4L2_CODEC2_COMPONENTS_VIDEO_FRAME_POOL_H

#include <atomic>
#include <memory>
#include <optional>
#include <queue>

#include <C2Buffer.h>
#include <base/callback.h>
#include <base/memory/weak_ptr.h>
#include <base/sequenced_task_runner.h>
#include <base/threading/thread.h>

#include <size.h>
#include <v4l2_codec2/common/VideoTypes.h>
#include <v4l2_codec2/components/VideoFrame.h>

namespace android {

// Fetch C2GraphicBlock from C2BlockPool and wrap to VideoFrame.
// Provide asynchronous call which avoid the caller busy-polling while
// C2BlockPool::fetchGraphicBlock() times out.
class VideoFramePool {
public:
    using FrameWithBlockId = std::pair<std::unique_ptr<VideoFrame>, uint32_t>;
    using GetVideoFrameCB = ::base::OnceCallback<void(std::optional<FrameWithBlockId>)>;

    static std::unique_ptr<VideoFramePool> Create(
            std::shared_ptr<C2BlockPool> blockPool, const size_t numBuffers,
            const media::Size& size, HalPixelFormat pixelFormat, bool isSecure,
            scoped_refptr<::base::SequencedTaskRunner> taskRunner);
    ~VideoFramePool();

    // Get a VideoFrame instance, which will be passed via |cb|.
    // If any error occurs, then nullptr will be passed via |cb|.
    // Return false if the previous callback has not been called, and |cb| will
    // be dropped directly.
    bool getVideoFrame(GetVideoFrameCB cb);

private:
    // |blockPool| is the C2BlockPool that we fetch graphic blocks from.
    // |size| is the resolution size of the required graphic blocks.
    // |pixelFormat| is the pixel format of the required graphic blocks.
    // |isSecure| indicates the video stream is encrypted or not.
    // All public methods and the callbacks should be run on |taskRunner|.
    VideoFramePool(std::shared_ptr<C2BlockPool> blockPool, const media::Size& size,
                   HalPixelFormat pixelFormat, bool isSecure,
                   scoped_refptr<::base::SequencedTaskRunner> taskRunner);
    bool initialize();
    void destroyTask();

    static void getVideoFrameTaskThunk(scoped_refptr<::base::SequencedTaskRunner> taskRunner,
                                       std::optional<::base::WeakPtr<VideoFramePool>> weakPool);
    void getVideoFrameTask();
    void onVideoFrameReady(std::optional<FrameWithBlockId> frameWithBlockId);

    // Extracts buffer ID from graphic block.
    // |block| is the graphic block allocated by |blockPool|.
    static std::optional<uint32_t> getBufferIdFromGraphicBlock(const C2BlockPool& blockPool,
                                                               const C2Block2D& block);

    // Ask |blockPool| to allocate the specified number of buffers.
    // |bufferCount| is the number of requested buffers.
    static c2_status_t requestNewBufferSet(C2BlockPool& blockPool, int32_t bufferCount);

    // Ask |blockPool| to notify when a block is available via |cb|.
    // Return true if |blockPool| supports notifying buffer available.
    static bool setNotifyBlockAvailableCb(C2BlockPool& blockPool, ::base::OnceClosure cb);

    std::shared_ptr<C2BlockPool> mBlockPool;
    const media::Size mSize;
    const HalPixelFormat mPixelFormat;
    const C2MemoryUsage mMemoryUsage;

    GetVideoFrameCB mOutputCb;

    scoped_refptr<::base::SequencedTaskRunner> mClientTaskRunner;
    ::base::Thread mFetchThread{"VideoFramePoolFetchThread"};
    scoped_refptr<::base::SequencedTaskRunner> mFetchTaskRunner;

    ::base::WeakPtr<VideoFramePool> mClientWeakThis;
    ::base::WeakPtr<VideoFramePool> mFetchWeakThis;
    ::base::WeakPtrFactory<VideoFramePool> mClientWeakThisFactory{this};
    ::base::WeakPtrFactory<VideoFramePool> mFetchWeakThisFactory{this};
};

}  // namespace android

#endif  // ANDROID_V4L2_CODEC2_COMPONENTS_VIDEO_FRAME_POOL_H
