// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//#define LOG_NDEBUG 0
#define LOG_TAG "VideoFramePool"

#include <v4l2_codec2/components/VideoFramePool.h>

#include <stdint.h>
#include <memory>

#include <android/hardware/graphics/common/1.0/types.h>
#include <base/bind.h>
#include <base/memory/ptr_util.h>
#include <base/time/time.h>
#include <log/log.h>

#include <v4l2_codec2/common/VideoTypes.h>
#include <v4l2_codec2/plugin_store/C2VdaBqBlockPool.h>
#include <v4l2_codec2/plugin_store/C2VdaPooledBlockPool.h>
#include <v4l2_codec2/plugin_store/V4L2AllocatorId.h>

using android::hardware::graphics::common::V1_0::BufferUsage;

namespace android {

// static
std::optional<uint32_t> VideoFramePool::getBufferIdFromGraphicBlock(const C2BlockPool& blockPool,
                                                                    const C2Block2D& block) {
    ALOGV("%s() blockPool.getAllocatorId() = %u", __func__, blockPool.getAllocatorId());

    if (blockPool.getAllocatorId() == android::V4L2AllocatorId::V4L2_BUFFERPOOL) {
        return C2VdaPooledBlockPool::getBufferIdFromGraphicBlock(block);
    } else if (blockPool.getAllocatorId() == C2PlatformAllocatorStore::BUFFERQUEUE) {
        return C2VdaBqBlockPool::getBufferIdFromGraphicBlock(block);
    }

    ALOGE("%s(): unknown allocator ID: %u", __func__, blockPool.getAllocatorId());
    return std::nullopt;
}

// static
c2_status_t VideoFramePool::requestNewBufferSet(C2BlockPool& blockPool, int32_t bufferCount) {
    ALOGV("%s() blockPool.getAllocatorId() = %u", __func__, blockPool.getAllocatorId());

    if (blockPool.getAllocatorId() == android::V4L2AllocatorId::V4L2_BUFFERPOOL) {
        C2VdaPooledBlockPool* bpPool = static_cast<C2VdaPooledBlockPool*>(&blockPool);
        return bpPool->requestNewBufferSet(bufferCount);
    } else if (blockPool.getAllocatorId() == C2PlatformAllocatorStore::BUFFERQUEUE) {
        C2VdaBqBlockPool* bqPool = static_cast<C2VdaBqBlockPool*>(&blockPool);
        return bqPool->requestNewBufferSet(bufferCount);
    }

    ALOGE("%s(): unknown allocator ID: %u", __func__, blockPool.getAllocatorId());
    return C2_BAD_VALUE;
}

// static
bool VideoFramePool::setNotifyBlockAvailableCb(C2BlockPool& blockPool, ::base::OnceClosure cb) {
    ALOGV("%s() blockPool.getAllocatorId() = %u", __func__, blockPool.getAllocatorId());

    if (blockPool.getAllocatorId() == C2PlatformAllocatorStore::BUFFERQUEUE) {
        C2VdaBqBlockPool* bqPool = static_cast<C2VdaBqBlockPool*>(&blockPool);
        return bqPool->setNotifyBlockAvailableCb(std::move(cb));
    }
    return false;
}

// static
std::unique_ptr<VideoFramePool> VideoFramePool::Create(
        std::shared_ptr<C2BlockPool> blockPool, const size_t numBuffers, const media::Size& size,
        HalPixelFormat pixelFormat, bool isSecure,
        scoped_refptr<::base::SequencedTaskRunner> taskRunner) {
    ALOG_ASSERT(blockPool != nullptr);

    if (requestNewBufferSet(*blockPool, numBuffers) != C2_OK) {
        return nullptr;
    }

    std::unique_ptr<VideoFramePool> pool = ::base::WrapUnique(new VideoFramePool(
            std::move(blockPool), size, pixelFormat, isSecure, std::move(taskRunner)));
    if (!pool->initialize()) return nullptr;
    return pool;
}

VideoFramePool::VideoFramePool(std::shared_ptr<C2BlockPool> blockPool, const media::Size& size,
                               HalPixelFormat pixelFormat, bool isSecure,
                               scoped_refptr<::base::SequencedTaskRunner> taskRunner)
      : mBlockPool(std::move(blockPool)),
        mSize(size),
        mPixelFormat(pixelFormat),
        mMemoryUsage(isSecure ? C2MemoryUsage::READ_PROTECTED : C2MemoryUsage::CPU_READ,
                     static_cast<uint64_t>(BufferUsage::VIDEO_DECODER)),
        mClientTaskRunner(std::move(taskRunner)) {
    ALOGV("%s(size=%dx%d)", __func__, size.width(), size.height());
    ALOG_ASSERT(mClientTaskRunner->RunsTasksInCurrentSequence());
    DCHECK(mBlockPool);
    DCHECK(mClientTaskRunner);
}

bool VideoFramePool::initialize() {
    if (!mFetchThread.Start()) {
        ALOGE("Fetch thread failed to start.");
        return false;
    }
    mFetchTaskRunner = mFetchThread.task_runner();

    mClientWeakThis = mClientWeakThisFactory.GetWeakPtr();
    mFetchWeakThis = mFetchWeakThisFactory.GetWeakPtr();

    return true;
}

VideoFramePool::~VideoFramePool() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mClientTaskRunner->RunsTasksInCurrentSequence());

    mClientWeakThisFactory.InvalidateWeakPtrs();

    if (mFetchThread.IsRunning()) {
        mFetchTaskRunner->PostTask(FROM_HERE,
                                   ::base::BindOnce(&VideoFramePool::destroyTask, mFetchWeakThis));
        mFetchThread.Stop();
    }
}

void VideoFramePool::destroyTask() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mFetchTaskRunner->RunsTasksInCurrentSequence());

    mFetchWeakThisFactory.InvalidateWeakPtrs();
}

bool VideoFramePool::getVideoFrame(GetVideoFrameCB cb) {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mClientTaskRunner->RunsTasksInCurrentSequence());

    if (mOutputCb) {
        return false;
    }

    mOutputCb = std::move(cb);
    mFetchTaskRunner->PostTask(
            FROM_HERE, ::base::BindOnce(&VideoFramePool::getVideoFrameTask, mFetchWeakThis));
    return true;
}

// static
void VideoFramePool::getVideoFrameTaskThunk(
        scoped_refptr<::base::SequencedTaskRunner> taskRunner,
        std::optional<::base::WeakPtr<VideoFramePool>> weakPool) {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(weakPool);

    taskRunner->PostTask(FROM_HERE,
                         ::base::BindOnce(&VideoFramePool::getVideoFrameTask, *weakPool));
}

void VideoFramePool::getVideoFrameTask() {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mFetchTaskRunner->RunsTasksInCurrentSequence());

    // Variables used to exponential backoff retry when buffer fetching times out.
    constexpr size_t kFetchRetryDelayInit = 64;    // Initial delay: 64us
    constexpr size_t kFetchRetryDelayMax = 16384;  // Max delay: 16ms (1 frame at 60fps)
    static size_t sNumRetries = 0;
    static size_t sDelay = kFetchRetryDelayInit;

    std::shared_ptr<C2GraphicBlock> block;
    c2_status_t err = mBlockPool->fetchGraphicBlock(mSize.width(), mSize.height(),
                                                    static_cast<uint32_t>(mPixelFormat),
                                                    mMemoryUsage, &block);
    if (err == C2_TIMED_OUT || err == C2_BLOCKING) {
        if (setNotifyBlockAvailableCb(*mBlockPool,
                                      ::base::BindOnce(&VideoFramePool::getVideoFrameTaskThunk,
                                                       mFetchTaskRunner, mFetchWeakThis))) {
            ALOGV("%s(): fetchGraphicBlock() timeout, waiting for block available.", __func__);
        } else {
            ALOGV("%s(): fetchGraphicBlock() timeout, waiting %zuus (%zu retry)", __func__, sDelay,
                  sNumRetries + 1);
            mFetchTaskRunner->PostDelayedTask(
                    FROM_HERE, ::base::BindOnce(&VideoFramePool::getVideoFrameTask, mFetchWeakThis),
                    ::base::TimeDelta::FromMicroseconds(sDelay));

            sDelay = std::min(sDelay * 2, kFetchRetryDelayMax);  // Exponential backoff
            sNumRetries++;
        }

        return;
    }

    // Reset to the default value.
    sNumRetries = 0;
    sDelay = kFetchRetryDelayInit;

    std::optional<FrameWithBlockId> frameWithBlockId;
    if (err == C2_OK) {
        ALOG_ASSERT(block != nullptr);
        std::optional<uint32_t> bufferId = getBufferIdFromGraphicBlock(*mBlockPool, *block);
        std::unique_ptr<VideoFrame> frame = VideoFrame::Create(std::move(block));
        // Only pass the frame + id pair if both have successfully been obtained.
        // Otherwise exit the loop so a nullopt is passed to the client.
        if (bufferId && frame) {
            frameWithBlockId = std::make_pair(std::move(frame), *bufferId);
        } else {
            ALOGE("%s(): Failed to generate VideoFrame or get the buffer id.", __func__);
        }
    } else {
        ALOGE("%s(): Failed to fetch block, err=%d", __func__, err);
    }

    mClientTaskRunner->PostTask(
            FROM_HERE, ::base::BindOnce(&VideoFramePool::onVideoFrameReady, mClientWeakThis,
                                        std::move(frameWithBlockId)));
}

void VideoFramePool::onVideoFrameReady(std::optional<FrameWithBlockId> frameWithBlockId) {
    ALOGV("%s()", __func__);
    ALOG_ASSERT(mClientTaskRunner->RunsTasksInCurrentSequence());

    if (!frameWithBlockId) {
        ALOGE("Failed to get GraphicBlock, abandoning all pending requests.");
        mClientWeakThisFactory.InvalidateWeakPtrs();
        mClientWeakThis = mClientWeakThisFactory.GetWeakPtr();
    }

    ALOG_ASSERT(mOutputCb);
    std::move(mOutputCb).Run(std::move(frameWithBlockId));
}

}  // namespace android
