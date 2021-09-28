// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//#define LOG_NDEBUG 0
#define LOG_TAG "C2VdaBqBlockPool"

#include <v4l2_codec2/plugin_store/C2VdaBqBlockPool.h>

#include <errno.h>

#include <chrono>
#include <mutex>
#include <thread>

#include <C2AllocatorGralloc.h>
#include <C2BlockInternal.h>
#include <android/hardware/graphics/bufferqueue/2.0/IGraphicBufferProducer.h>
#include <android/hardware/graphics/bufferqueue/2.0/IProducerListener.h>
#include <base/callback.h>
#include <log/log.h>
#include <system/window.h>
#include <types.h>
#include <ui/BufferQueueDefs.h>

#include <v4l2_codec2/plugin_store/V4L2AllocatorId.h>

namespace android {
namespace {

// The wait time for acquire fence in milliseconds.
constexpr int kFenceWaitTimeMs = 10;
// The timeout limit of acquiring lock of timed_mutex in milliseconds.
constexpr std::chrono::milliseconds kTimedMutexTimeoutMs = std::chrono::milliseconds(500);

}  // namespace

using namespace std::chrono_literals;

using ::android::C2AndroidMemoryUsage;
using ::android::Fence;
using ::android::GraphicBuffer;
using ::android::sp;
using ::android::status_t;
using ::android::BufferQueueDefs::BUFFER_NEEDS_REALLOCATION;
using ::android::BufferQueueDefs::NUM_BUFFER_SLOTS;
using ::android::BufferQueueDefs::RELEASE_ALL_BUFFERS;
using ::android::hardware::hidl_handle;
using ::android::hardware::Return;

using HBuffer = ::android::hardware::graphics::common::V1_2::HardwareBuffer;
using HStatus = ::android::hardware::graphics::bufferqueue::V2_0::Status;
using HGraphicBufferProducer =
        ::android::hardware::graphics::bufferqueue::V2_0::IGraphicBufferProducer;
using HProducerListener = ::android::hardware::graphics::bufferqueue::V2_0::IProducerListener;
using HConnectionType = hardware::graphics::bufferqueue::V2_0::ConnectionType;
using HQueueBufferOutput =
        ::android::hardware::graphics::bufferqueue::V2_0::IGraphicBufferProducer::QueueBufferOutput;

using ::android::hardware::graphics::bufferqueue::V2_0::utils::b2h;
using ::android::hardware::graphics::bufferqueue::V2_0::utils::h2b;
using ::android::hardware::graphics::bufferqueue::V2_0::utils::HFenceWrapper;

static c2_status_t asC2Error(int32_t err) {
    switch (err) {
    case android::NO_ERROR:
        return C2_OK;
    case android::NO_INIT:
        return C2_NO_INIT;
    case android::BAD_VALUE:
        return C2_BAD_VALUE;
    case android::TIMED_OUT:
        return C2_TIMED_OUT;
    case android::WOULD_BLOCK:
        return C2_BLOCKING;
    case android::NO_MEMORY:
        return C2_NO_MEMORY;
    }
    return C2_CORRUPTED;
}

class H2BGraphicBufferProducer {
public:
    explicit H2BGraphicBufferProducer(sp<HGraphicBufferProducer> base) : mBase(base) {}
    ~H2BGraphicBufferProducer() = default;

    status_t requestBuffer(int slot, sp<GraphicBuffer>* buf) {
        bool converted = false;
        status_t status = UNKNOWN_ERROR;
        Return<void> transResult = mBase->requestBuffer(
                slot, [&converted, &status, buf](HStatus hStatus, HBuffer const& hBuffer,
                                                 uint32_t generationNumber) {
                    converted = h2b(hStatus, &status) && h2b(hBuffer, buf);
                    if (*buf) {
                        (*buf)->setGenerationNumber(generationNumber);
                    }
                });

        if (!transResult.isOk()) {
            ALOGE("%s(): transaction failed: %s", __func__, transResult.description().c_str());
            return FAILED_TRANSACTION;
        }
        if (!converted) {
            ALOGE("%s(): corrupted transaction.", __func__);
            return FAILED_TRANSACTION;
        }
        if (status != android::NO_ERROR) {
            ALOGE("%s() failed: %d", __func__, status);
        }
        return status;
    }

    status_t setMaxDequeuedBufferCount(int maxDequeuedBuffers) {
        status_t status = UNKNOWN_ERROR;
        Return<HStatus> transResult =
                mBase->setMaxDequeuedBufferCount(static_cast<int32_t>(maxDequeuedBuffers));

        if (!transResult.isOk()) {
            ALOGE("%s(): transaction failed: %s", __func__, transResult.description().c_str());
            return FAILED_TRANSACTION;
        }
        if (!h2b(static_cast<HStatus>(transResult), &status)) {
            ALOGE("%s(): corrupted transaction.", __func__);
            return FAILED_TRANSACTION;
        }
        if (status != android::NO_ERROR) {
            ALOGE("%s() failed: %d", __func__, status);
        }
        return status;
    }

    status_t dequeueBuffer(uint32_t width, uint32_t height, uint32_t pixelFormat,
                           C2AndroidMemoryUsage androidUsage, int* slot, sp<Fence>* fence) {
        using Input = HGraphicBufferProducer::DequeueBufferInput;
        using Output = HGraphicBufferProducer::DequeueBufferOutput;
        Input input{width, height, pixelFormat, androidUsage.asGrallocUsage()};

        bool converted = false;
        status_t status = UNKNOWN_ERROR;
        Return<void> transResult = mBase->dequeueBuffer(
                input, [&converted, &status, &slot, &fence](HStatus hStatus, int32_t hSlot,
                                                            Output const& hOutput) {
                    converted = h2b(hStatus, &status);
                    if (!converted || status != android::NO_ERROR) {
                        return;
                    }

                    *slot = hSlot;
                    if (hOutput.bufferNeedsReallocation) {
                        status = BUFFER_NEEDS_REALLOCATION;
                    }
                    converted = h2b(hOutput.fence, fence);
                });

        if (!transResult.isOk()) {
            ALOGE("%s(): transaction failed: %s", __func__, transResult.description().c_str());
            return FAILED_TRANSACTION;
        }
        if (!converted) {
            ALOGE("%s(): corrupted transaction.", __func__);
            return FAILED_TRANSACTION;
        }
        if (status != android::NO_ERROR && status != BUFFER_NEEDS_REALLOCATION &&
            status != android::TIMED_OUT) {
            ALOGE("%s() failed: %d", __func__, status);
        }
        return status;
    }

    status_t detachBuffer(int slot) {
        status_t status = UNKNOWN_ERROR;
        Return<HStatus> transResult = mBase->detachBuffer(static_cast<int32_t>(slot));

        if (!transResult.isOk()) {
            ALOGE("%s(): transaction failed: %s", __func__, transResult.description().c_str());
            return FAILED_TRANSACTION;
        }
        if (!h2b(static_cast<HStatus>(transResult), &status)) {
            ALOGE("%s(): corrupted transaction.", __func__);
            return FAILED_TRANSACTION;
        }
        if (status != android::NO_ERROR) {
            ALOGE("%s() failed: %d", __func__, status);
        }
        return status;
    }

    status_t attachBuffer(const sp<GraphicBuffer>& buffer, int* outSlot) {
        HBuffer hBuffer;
        uint32_t hGenerationNumber;
        if (!b2h(buffer, &hBuffer, &hGenerationNumber)) {
            ALOGE("%s: invalid input buffer.", __func__);
            return BAD_VALUE;
        }

        bool converted = false;
        status_t status = UNKNOWN_ERROR;
        Return<void> transResult = mBase->attachBuffer(
                hBuffer, hGenerationNumber,
                [&converted, &status, outSlot](HStatus hStatus, int32_t hSlot,
                                               bool releaseAllBuffers) {
                    converted = h2b(hStatus, &status);
                    *outSlot = static_cast<int>(hSlot);
                    if (converted && releaseAllBuffers && status == android::NO_ERROR) {
                        status = RELEASE_ALL_BUFFERS;
                    }
                });

        if (!transResult.isOk()) {
            ALOGE("%s(): transaction failed: %s", __func__, transResult.description().c_str());
            return FAILED_TRANSACTION;
        }
        if (!converted) {
            ALOGE("%s(): corrupted transaction.", __func__);
            return FAILED_TRANSACTION;
        }
        if (status != android::NO_ERROR) {
            ALOGE("%s() failed: %d", __func__, status);
        }
        return status;
    }

    status_t cancelBuffer(int slot, const sp<Fence>& fence) {
        HFenceWrapper hFenceWrapper;
        if (!b2h(fence, &hFenceWrapper)) {
            ALOGE("%s(): corrupted input fence.", __func__);
            return UNKNOWN_ERROR;
        }

        status_t status = UNKNOWN_ERROR;
        Return<HStatus> transResult =
                mBase->cancelBuffer(static_cast<int32_t>(slot), hFenceWrapper.getHandle());

        if (!transResult.isOk()) {
            ALOGE("%s(): transaction failed: %s", __func__, transResult.description().c_str());
            return FAILED_TRANSACTION;
        }
        if (!h2b(static_cast<HStatus>(transResult), &status)) {
            ALOGE("%s(): corrupted transaction.", __func__);
            return FAILED_TRANSACTION;
        }
        if (status != android::NO_ERROR) {
            ALOGE("%s() failed: %d", __func__, status);
        }
        return status;
    }

    int query(int what, int* value) {
        int result = 0;
        Return<void> transResult =
                mBase->query(static_cast<int32_t>(what), [&result, value](int32_t r, int32_t v) {
                    result = static_cast<int>(r);
                    *value = static_cast<int>(v);
                });

        if (!transResult.isOk()) {
            ALOGE("%s(): transaction failed: %s", __func__, transResult.description().c_str());
            return FAILED_TRANSACTION;
        }
        return result;
    }

    status_t allowAllocation(bool allow) {
        status_t status = UNKNOWN_ERROR;
        Return<HStatus> transResult = mBase->allowAllocation(allow);

        if (!transResult.isOk()) {
            ALOGE("%s(): transaction failed: %s", __func__, transResult.description().c_str());
            return FAILED_TRANSACTION;
        }
        if (!h2b(static_cast<HStatus>(transResult), &status)) {
            ALOGE("%s(): corrupted transaction.", __func__);
            return FAILED_TRANSACTION;
        }
        if (status != android::NO_ERROR) {
            ALOGW("%s() failed: %d", __func__, status);
        }
        return status;
    }

    status_t getUniqueId(uint64_t* outId) const {
        Return<uint64_t> transResult = mBase->getUniqueId();

        if (!transResult.isOk()) {
            ALOGE("%s(): transaction failed: %s", __func__, transResult.description().c_str());
            return FAILED_TRANSACTION;
        }

        *outId = static_cast<uint64_t>(transResult);
        return android::NO_ERROR;
    }

    // android::IProducerListener cannot be depended by vendor library, so we use HProducerListener
    // directly.
    status_t connect(sp<HProducerListener> const& hListener, int32_t api,
                     bool producerControlledByApp) {
        bool converted = false;
        status_t status = UNKNOWN_ERROR;
        // hack(b/146409777): We pass self-defined api, so we don't use b2h() here.
        Return<void> transResult = mBase->connect(
                hListener, static_cast<HConnectionType>(api), producerControlledByApp,
                [&converted, &status](HStatus hStatus, HQueueBufferOutput const& /* hOutput */) {
                    converted = h2b(hStatus, &status);
                });

        if (!transResult.isOk()) {
            ALOGE("%s(): transaction failed: %s", __func__, transResult.description().c_str());
            return FAILED_TRANSACTION;
        }
        if (!converted) {
            ALOGE("%s(): corrupted transaction.", __func__);
            return FAILED_TRANSACTION;
        }
        return status;
    }

    status_t setDequeueTimeout(nsecs_t timeout) {
        status_t status = UNKNOWN_ERROR;
        Return<HStatus> transResult = mBase->setDequeueTimeout(static_cast<int64_t>(timeout));

        if (!transResult.isOk()) {
            ALOGE("%s(): transaction failed: %s", __func__, transResult.description().c_str());
            return FAILED_TRANSACTION;
        }
        if (!h2b(static_cast<HStatus>(transResult), &status)) {
            ALOGE("%s(): corrupted transaction.", __func__);
            return FAILED_TRANSACTION;
        }
        return status;
    }

private:
    const sp<HGraphicBufferProducer> mBase;
};

// This class is used to notify the listener when a certain event happens.
class EventNotifier : public virtual android::RefBase {
public:
    class Listener {
    public:
        virtual ~Listener() = default;

        // Called by EventNotifier when a certain event happens.
        virtual void onEventNotified() = 0;
    };

    explicit EventNotifier(const std::shared_ptr<Listener>& listener) : mListener(listener) {}
    virtual ~EventNotifier() = default;

protected:
    void notify() {
        ALOGV("%s()", __func__);
        std::shared_ptr<Listener> listener = mListener.lock();
        if (listener) {
            listener->onEventNotified();
        }
    }

    std::weak_ptr<Listener> mListener;
};

// Notifies the listener when the connected IGBP releases buffers.
class BufferReleasedNotifier : public EventNotifier, public HProducerListener {
public:
    using EventNotifier::EventNotifier;
    ~BufferReleasedNotifier() override = default;

    // HProducerListener implementation
    Return<void> onBuffersReleased(uint32_t count) override {
        ALOGV("%s(%u)", __func__, count);
        if (count > 0) {
            notify();
        }
        return {};
    }
};

/**
 * BlockPoolData implementation for C2VdaBqBlockPool. The life cycle of this object should be as
 * long as its accompanied C2GraphicBlock.
 *
 * When C2VdaBqBlockPoolData is created, |mShared| is false, and the owner of the accompanied
 * C2GraphicBlock is the component that called fetchGraphicBlock(). If this is released before
 * sharing, the destructor will call detachBuffer() to BufferQueue to free the slot.
 *
 * When the accompanied C2GraphicBlock is going to share to client from component, component should
 * call MarkBlockPoolDataAsShared() to set |mShared| to true, and then this will be released after
 * the transition of C2GraphicBlock across HIDL interface. At this time, the destructor will not
 * call detachBuffer().
 */
struct C2VdaBqBlockPoolData : public _C2BlockPoolData {
    // This type should be a different value than what _C2BlockPoolData::type_t has defined.
    static constexpr int kTypeVdaBufferQueue = TYPE_BUFFERQUEUE + 256;

    C2VdaBqBlockPoolData(uint64_t producerId, int32_t slotId,
                         const std::shared_ptr<C2VdaBqBlockPool::Impl>& pool);
    C2VdaBqBlockPoolData() = delete;

    // If |mShared| is false, call detach buffer to BufferQueue via |mPool|
    virtual ~C2VdaBqBlockPoolData() override;

    type_t getType() const override { return static_cast<type_t>(kTypeVdaBufferQueue); }

    bool mShared = false;  // whether is shared from component to client.
    const uint64_t mProducerId;
    const int32_t mSlotId;
    const std::shared_ptr<C2VdaBqBlockPool::Impl> mPool;
};

c2_status_t MarkBlockPoolDataAsShared(const C2ConstGraphicBlock& sharedBlock) {
    std::shared_ptr<_C2BlockPoolData> data = _C2BlockFactory::GetGraphicBlockPoolData(sharedBlock);
    if (!data || data->getType() != C2VdaBqBlockPoolData::kTypeVdaBufferQueue) {
        // Skip this functtion if |sharedBlock| is not fetched from C2VdaBqBlockPool.
        return C2_OMITTED;
    }
    const std::shared_ptr<C2VdaBqBlockPoolData> poolData =
            std::static_pointer_cast<C2VdaBqBlockPoolData>(data);
    if (poolData->mShared) {
        ALOGE("C2VdaBqBlockPoolData(id=%" PRIu64 ", slot=%d) is already marked as shared...",
              poolData->mProducerId, poolData->mSlotId);
        return C2_BAD_STATE;
    }
    poolData->mShared = true;
    return C2_OK;
}

// static
std::optional<uint32_t> C2VdaBqBlockPool::getBufferIdFromGraphicBlock(const C2Block2D& block) {
    uint32_t width, height, format, stride, igbp_slot, generation;
    uint64_t usage, igbp_id;
    android::_UnwrapNativeCodec2GrallocMetadata(block.handle(), &width, &height, &format, &usage,
                                                &stride, &generation, &igbp_id, &igbp_slot);
    ALOGV("Unwrap Metadata: igbp[%" PRIu64 ", %u] (%u*%u, fmt %#x, usage %" PRIx64 ", stride %u)",
          igbp_id, igbp_slot, width, height, format, usage, stride);
    return igbp_slot;
}

class C2VdaBqBlockPool::Impl : public std::enable_shared_from_this<C2VdaBqBlockPool::Impl>,
                               public EventNotifier::Listener {
public:
    using HGraphicBufferProducer = C2VdaBqBlockPool::HGraphicBufferProducer;

    explicit Impl(const std::shared_ptr<C2Allocator>& allocator);
    // TODO: should we detach buffers on producer if any on destructor?
    ~Impl() = default;

    // EventNotifier::Listener implementation.
    void onEventNotified() override;

    c2_status_t fetchGraphicBlock(uint32_t width, uint32_t height, uint32_t format,
                                  C2MemoryUsage usage,
                                  std::shared_ptr<C2GraphicBlock>* block /* nonnull */);
    void setRenderCallback(const C2BufferQueueBlockPool::OnRenderCallback& renderCallback);
    void configureProducer(const sp<HGraphicBufferProducer>& producer);
    c2_status_t requestNewBufferSet(int32_t bufferCount);
    c2_status_t updateGraphicBlock(bool willCancel, uint32_t oldSlot, uint32_t* newSlot,
                                   std::shared_ptr<C2GraphicBlock>* block /* nonnull */);
    c2_status_t getMinBuffersForDisplay(size_t* bufferCount);
    bool setNotifyBlockAvailableCb(::base::OnceClosure cb);

private:
    friend struct C2VdaBqBlockPoolData;

    // Requested buffer formats.
    struct BufferFormat {
        BufferFormat(uint32_t width, uint32_t height, uint32_t pixelFormat,
                     C2AndroidMemoryUsage androidUsage)
              : mWidth(width), mHeight(height), mPixelFormat(pixelFormat), mUsage(androidUsage) {}
        BufferFormat() = default;

        uint32_t mWidth = 0;
        uint32_t mHeight = 0;
        uint32_t mPixelFormat = 0;
        C2AndroidMemoryUsage mUsage = C2MemoryUsage(0);
    };

    // For C2VdaBqBlockPoolData to detach corresponding slot buffer from BufferQueue.
    void detachBuffer(uint64_t producerId, int32_t slotId);

    // Queries the generation and usage flags from the given producer by dequeuing and requesting a
    // buffer (the buffer is then detached and freed).
    c2_status_t queryGenerationAndUsage(H2BGraphicBufferProducer* const producer, uint32_t width,
                                        uint32_t height, uint32_t pixelFormat,
                                        C2AndroidMemoryUsage androidUsage, uint32_t* generation,
                                        uint64_t* usage);

    // Switches producer and transfers allocated buffers from old producer to the new one.
    bool switchProducer(H2BGraphicBufferProducer* const newProducer, uint64_t newProducerId);

    const std::shared_ptr<C2Allocator> mAllocator;

    std::unique_ptr<H2BGraphicBufferProducer> mProducer;
    uint64_t mProducerId;
    C2BufferQueueBlockPool::OnRenderCallback mRenderCallback;

    // Function mutex to lock at the start of each API function call for protecting the
    // synchronization of all member variables.
    std::mutex mMutex;
    // The mutex of excluding the procedures of configuring producer and allocating buffers. They
    // should be blocked mutually. Set the timeout for acquiring lock in case of any deadlock.
    // Configuring producer: configureProducer() called by CCodec.
    // Allocating buffers: requestNewBufferSet(), then a loop of fetchGraphicBlock() called by
    //                     compoenent until |mSlotAllocations|.size() equals |mBuffersRequested|.
    std::timed_mutex mConfigureProducerAndAllocateBuffersMutex;
    // The unique lock of the procedure of allocating buffers. It should be locked in the beginning
    // of requestNewBufferSet() and unlock in the end of the loop of fetchGraphicBlock(). Note that
    // all calls should be in the same thread.
    std::unique_lock<std::timed_mutex> mAllocateBuffersLock;

    // The map restored C2GraphicAllocation from corresponding slot index.
    std::map<int32_t, std::shared_ptr<C2GraphicAllocation>> mSlotAllocations;
    // Number of buffers requested on requestNewBufferSet() call.
    size_t mBuffersRequested;
    // Currently requested buffer formats.
    BufferFormat mBufferFormat;
    // The map recorded the slot indices from old producer to new producer.
    std::map<int32_t, int32_t> mProducerChangeSlotMap;
    // The counter for representing the buffer count in client. Only used in producer switching
    // case. It will be reset in switchProducer(), and accumulated in updateGraphicBlock() routine.
    uint32_t mBuffersInClient = 0u;
    // The indicator to record if producer has been switched. Set to true when producer is switched.
    // Toggle off when requestNewBufferSet() is called. We forcedly detach all slots to make sure
    // all slots are available, except the ones owned by client.
    bool mProducerSwitched = false;

    // Listener for buffer release events.
    sp<EventNotifier> mFetchBufferNotifier;

    std::mutex mBufferReleaseMutex;
    // Set to true when the buffer release event is triggered after dequeueing
    // buffer from IGBP times out.
    bool mBufferReleasedAfterTimedOut GUARDED_BY(mBufferReleaseMutex) = false;
    // The callback to notify the caller the buffer is available.
    ::base::OnceClosure mNotifyBlockAvailableCb GUARDED_BY(mBufferReleaseMutex);
};

C2VdaBqBlockPool::Impl::Impl(const std::shared_ptr<C2Allocator>& allocator)
      : mAllocator(allocator),
        mAllocateBuffersLock(mConfigureProducerAndAllocateBuffersMutex, std::defer_lock),
        mBuffersRequested(0u) {}

c2_status_t C2VdaBqBlockPool::Impl::fetchGraphicBlock(
        uint32_t width, uint32_t height, uint32_t format, C2MemoryUsage usage,
        std::shared_ptr<C2GraphicBlock>* block /* nonnull */) {
    ALOGV("%s()", __func__);
    std::lock_guard<std::mutex> lock(mMutex);

    if (!mProducer) {
        // Producer will not be configured in byte-buffer mode. Allocate buffers from allocator
        // directly as a basic graphic block pool.
        std::shared_ptr<C2GraphicAllocation> alloc;
        c2_status_t err = mAllocator->newGraphicAllocation(width, height, format, usage, &alloc);
        if (err != C2_OK) {
            return err;
        }
        *block = _C2BlockFactory::CreateGraphicBlock(alloc);
        return C2_OK;
    }

    // The existence of |mProducerChangeSlotMap| indicates producer is just switched. Use return
    // code C2_BAD_STATE to inform the component to handle the procedure of producer change.
    // TODO(johnylin): consider to inform producer change to component in an active way.
    if (!mProducerChangeSlotMap.empty()) {
        return C2_BAD_STATE;
    }

    C2AndroidMemoryUsage androidUsage = usage;
    uint32_t pixelFormat = format;
    int32_t slot;
    sp<Fence> fence = new Fence();
    status_t status =
            mProducer->dequeueBuffer(width, height, pixelFormat, androidUsage, &slot, &fence);
    // The C2VdaBqBlockPool does not fully own the bufferqueue. After buffers are dequeued here,
    // they are passed into the codec2 framework, processed, and eventually queued into the
    // bufferqueue. The C2VdaBqBlockPool cannot determine exactly when a buffer gets queued.
    // However, if every buffer is being processed by the codec2 framework, then dequeueBuffer()
    // will return INVALID_OPERATION because of an attempt to dequeue too many buffers.
    // The C2VdaBqBlockPool cannot prevent this from happening, so just map it to TIMED_OUT
    // and let the C2VdaBqBlockPool's caller's timeout retry logic handle the failure.
    if (status == android::INVALID_OPERATION) {
        status = android::TIMED_OUT;
    }
    if (status == android::TIMED_OUT) {
        std::lock_guard<std::mutex> lock(mBufferReleaseMutex);
        mBufferReleasedAfterTimedOut = false;
    }
    if (status != android::NO_ERROR && status != BUFFER_NEEDS_REALLOCATION) {
        return asC2Error(status);
    }

    // Wait for acquire fence if we get one.
    if (fence) {
        status_t fenceStatus = fence->wait(kFenceWaitTimeMs);
        if (fenceStatus != android::NO_ERROR) {
            if (mProducer->cancelBuffer(slot, fence) != android::NO_ERROR) {
                return C2_CORRUPTED;
            }

            if (fenceStatus == -ETIME) {  // fence wait timed out
                ALOGV("%s(): buffer (slot=%d) fence wait timed out", __func__, slot);
                return C2_TIMED_OUT;
            }
            ALOGE("buffer fence wait error: %d", fenceStatus);
            return asC2Error(fenceStatus);
        }

        if (mRenderCallback) {
            nsecs_t signalTime = fence->getSignalTime();
            if (signalTime >= 0 && signalTime < INT64_MAX) {
                mRenderCallback(mProducerId, slot, signalTime);
            } else {
                ALOGV("got fence signal time of %" PRId64 " nsec", signalTime);
            }
        }
    }

    auto iter = mSlotAllocations.find(slot);
    if (iter == mSlotAllocations.end()) {
        if (mSlotAllocations.size() >= mBuffersRequested) {
            // The dequeued slot has a pre-allocated buffer whose size and format is as same as
            // currently requested (but was not dequeued during allocation cycle). Just detach it to
            // free this slot. And try dequeueBuffer again.
            ALOGD("dequeued a new slot index but already allocated enough buffers. Detach it.");

            if (mProducer->detachBuffer(slot) != android::NO_ERROR) {
                return C2_CORRUPTED;
            }
            return C2_TIMED_OUT;
        }
        if (status != BUFFER_NEEDS_REALLOCATION) {
            // The dequeued slot has a pre-allocated buffer whose size and format is as same as
            // currently requested, so there is no BUFFER_NEEDS_REALLOCATION flag. However since the
            // buffer reference is already dropped, still call requestBuffer to re-allocate then.
            // Add a debug note here for tracking.
            ALOGD("dequeued a new slot index without BUFFER_NEEDS_REALLOCATION flag.");
        }

        // Call requestBuffer to allocate buffer for the slot and obtain the reference.
        sp<GraphicBuffer> slotBuffer = new GraphicBuffer();
        status = mProducer->requestBuffer(slot, &slotBuffer);
        if (status != android::NO_ERROR) {
            if (mProducer->cancelBuffer(slot, fence) != android::NO_ERROR) {
                return C2_CORRUPTED;
            }
            return asC2Error(status);
        }

        // Convert GraphicBuffer to C2GraphicAllocation and wrap producer id and slot index
        ALOGV("buffer wraps { producer id: %" PRIu64 ", slot: %d }", mProducerId, slot);
        C2Handle* c2Handle = android::WrapNativeCodec2GrallocHandle(
                slotBuffer->handle, slotBuffer->width, slotBuffer->height, slotBuffer->format,
                slotBuffer->usage, slotBuffer->stride, slotBuffer->getGenerationNumber(),
                mProducerId, slot);
        if (!c2Handle) {
            ALOGE("WrapNativeCodec2GrallocHandle failed");
            return C2_NO_MEMORY;
        }

        std::shared_ptr<C2GraphicAllocation> alloc;
        c2_status_t err = mAllocator->priorGraphicAllocation(c2Handle, &alloc);
        if (err != C2_OK) {
            ALOGE("priorGraphicAllocation failed: %d", err);
            return err;
        }

        mSlotAllocations[slot] = std::move(alloc);
        if (mSlotAllocations.size() == mBuffersRequested) {
            // Already allocated enough buffers, set allowAllocation to false to restrict the
            // eligible slots to allocated ones for future dequeue.
            status = mProducer->allowAllocation(false);
            if (status != android::NO_ERROR) {
                return asC2Error(status);
            }
            // Store buffer formats for future usage.
            mBufferFormat = BufferFormat(width, height, pixelFormat, androidUsage);
            ALOG_ASSERT(mAllocateBuffersLock.owns_lock());
            mAllocateBuffersLock.unlock();
        }
    }

    auto poolData = std::make_shared<C2VdaBqBlockPoolData>(mProducerId, slot, shared_from_this());
    *block = _C2BlockFactory::CreateGraphicBlock(mSlotAllocations[slot], std::move(poolData));
    return C2_OK;
}

void C2VdaBqBlockPool::Impl::onEventNotified() {
    ALOGV("%s()", __func__);
    ::base::OnceClosure outputCb;
    {
        std::lock_guard<std::mutex> lock(mBufferReleaseMutex);

        mBufferReleasedAfterTimedOut = true;
        if (mNotifyBlockAvailableCb) {
            outputCb = std::move(mNotifyBlockAvailableCb);
        }
    }

    // Calling the callback outside the lock to avoid the deadlock.
    if (outputCb) {
        std::move(outputCb).Run();
    }
}

c2_status_t C2VdaBqBlockPool::Impl::queryGenerationAndUsage(
        H2BGraphicBufferProducer* const producer, uint32_t width, uint32_t height,
        uint32_t pixelFormat, C2AndroidMemoryUsage androidUsage, uint32_t* generation,
        uint64_t* usage) {
    ALOGV("queryGenerationAndUsage");
    sp<Fence> fence = new Fence();
    int32_t status;
    int32_t slot;

    status = producer->dequeueBuffer(width, height, pixelFormat, androidUsage, &slot, &fence);
    if (status != android::NO_ERROR && status != BUFFER_NEEDS_REALLOCATION) {
        return asC2Error(status);
    }

    // Wait for acquire fence if we get one.
    if (fence) {
        status_t fenceStatus = fence->wait(kFenceWaitTimeMs);
        if (fenceStatus != android::NO_ERROR) {
            if (producer->cancelBuffer(slot, fence) != android::NO_ERROR) {
                return C2_CORRUPTED;
            }
            if (fenceStatus == -ETIME) {  // fence wait timed out
                ALOGV("%s(): buffer (slot=%d) fence wait timed out", __func__, slot);
                return C2_TIMED_OUT;
            }
            ALOGE("buffer fence wait error: %d", fenceStatus);
            return asC2Error(fenceStatus);
        }
    }

    // Call requestBuffer to allocate buffer for the slot and obtain the reference.
    // Get generation number here.
    sp<GraphicBuffer> slotBuffer = new GraphicBuffer();
    status = producer->requestBuffer(slot, &slotBuffer);

    // Detach and delete the temporary buffer.
    if (producer->detachBuffer(slot) != android::NO_ERROR) {
        return C2_CORRUPTED;
    }

    // Check requestBuffer return flag.
    if (status != android::NO_ERROR) {
        return asC2Error(status);
    }

    // Get generation number and usage from the slot buffer.
    *usage = slotBuffer->getUsage();
    *generation = slotBuffer->getGenerationNumber();
    ALOGV("Obtained from temp buffer: generation = %u, usage = %" PRIu64 "", *generation, *usage);
    return C2_OK;
}

void C2VdaBqBlockPool::Impl::setRenderCallback(
        const C2BufferQueueBlockPool::OnRenderCallback& renderCallback) {
    ALOGV("setRenderCallback");
    std::lock_guard<std::mutex> lock(mMutex);
    mRenderCallback = renderCallback;
}

c2_status_t C2VdaBqBlockPool::Impl::requestNewBufferSet(int32_t bufferCount) {
    if (bufferCount <= 0) {
        ALOGE("Invalid requested buffer count = %d", bufferCount);
        return C2_BAD_VALUE;
    }

    if (!mAllocateBuffersLock.try_lock_for(kTimedMutexTimeoutMs)) {
        ALOGE("Cannot acquire allocate buffers / configure producer lock over %" PRId64 " ms...",
              static_cast<int64_t>(kTimedMutexTimeoutMs.count()));
        return C2_BLOCKING;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    if (!mProducer) {
        ALOGD("No HGraphicBufferProducer is configured...");
        return C2_NO_INIT;
    }

    if (mProducerSwitched) {
        // Some slots can be occupied by buffers transferred from the old producer. They will not
        // used in the current producer. Free the slots of the buffers here. But we cannot find a
        // slot is associated with the staled buffer. We free all slots whose associated buffers
        // are not owned by client.
        ALOGI("requestNewBufferSet: detachBuffer all slots forcedly");
        for (int32_t slot = 0; slot < static_cast<int32_t>(NUM_BUFFER_SLOTS); ++slot) {
            if (mSlotAllocations.find(slot) != mSlotAllocations.end()) {
                // Skip detaching the buffer which is owned by client now.
                continue;
            }
            status_t status = mProducer->detachBuffer(slot);
            if (status == android::NO_INIT) {
                // No more active buffer slot. Break the loop now.
                break;
            } else if (status != android::NO_ERROR) {
                return C2_CORRUPTED;
            }
        }
        mProducerSwitched = false;
    }

    ALOGV("Requested new buffer count: %d, still dequeued buffer count: %zu", bufferCount,
          mSlotAllocations.size());

    // The remained slot indices in |mSlotAllocations| now are still dequeued (un-available).
    // maxDequeuedBufferCount should be set to "new requested buffer count" + "still dequeued buffer
    // count" to make sure it has enough available slots to request buffer from.
    status_t status = mProducer->setMaxDequeuedBufferCount(bufferCount + mSlotAllocations.size());
    if (status != android::NO_ERROR) {
        return asC2Error(status);
    }

    // Release all remained slot buffer references here. CCodec should either cancel or queue its
    // owned buffers from this set before the next resolution change.
    mSlotAllocations.clear();
    mProducerChangeSlotMap.clear();
    mBuffersRequested = static_cast<size_t>(bufferCount);

    status = mProducer->allowAllocation(true);
    if (status != android::NO_ERROR) {
        return asC2Error(status);
    }
    return C2_OK;
}

void C2VdaBqBlockPool::Impl::configureProducer(const sp<HGraphicBufferProducer>& producer) {
    ALOGV("configureProducer");
    if (producer == nullptr) {
        ALOGE("input producer is nullptr...");
        return;
    }

    std::unique_lock<std::timed_mutex> configureProducerLock(
            mConfigureProducerAndAllocateBuffersMutex, std::defer_lock);
    if (!configureProducerLock.try_lock_for(kTimedMutexTimeoutMs)) {
        ALOGE("Cannot acquire configure producer / allocate buffers lock over %" PRId64 " ms...",
              static_cast<int64_t>(kTimedMutexTimeoutMs.count()));
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    auto newProducer = std::make_unique<H2BGraphicBufferProducer>(producer);
    uint64_t producerId;
    if (newProducer->getUniqueId(&producerId) != android::NO_ERROR) {
        return;
    }

    if (mProducer && mProducerId != producerId) {
        ALOGI("Producer (Surface) is going to switch... ( %" PRIu64 " -> %" PRIu64 " )",
              mProducerId, producerId);
        if (!switchProducer(newProducer.get(), producerId)) {
            mProducerChangeSlotMap.clear();
            return;
        }
    } else {
        mSlotAllocations.clear();
    }

    if (newProducer->setDequeueTimeout(0) != android::NO_ERROR) {
        ALOGE("%s(): failed to setDequeueTimeout(0)", __func__);
        return;
    }

    // hack(b/146409777): Try to connect ARC-specific listener first.
    sp<BufferReleasedNotifier> listener = new BufferReleasedNotifier(shared_from_this());
    if (newProducer->connect(listener, 'ARC\0', false) == android::NO_ERROR) {
        ALOGI("connected to ARC-specific IGBP listener.");
        mFetchBufferNotifier = listener;
    }

    // HGraphicBufferProducer could (and should) be replaced if the client has set a new generation
    // number to producer. The old HGraphicBufferProducer will be disconnected and deprecated then.
    mProducer = std::move(newProducer);
    mProducerId = producerId;
}

bool C2VdaBqBlockPool::Impl::switchProducer(H2BGraphicBufferProducer* const newProducer,
                                            uint64_t newProducerId) {
    if (mAllocator->getId() == android::V4L2AllocatorId::SECURE_GRAPHIC) {
        // TODO(johnylin): support this when we meet the use case in the future.
        ALOGE("Switch producer for secure buffer is not supported...");
        return false;
    }

    // Set maxDequeuedBufferCount to new producer.
    // Just like requestNewBufferSet(), maxDequeuedBufferCount should be set to "requested buffer
    // count" + "buffer count in client" to make sure it has enough available slots to request
    // buffers from.
    // "Requested buffer count" could be obtained by the size of |mSlotAllocations|. However, it is
    // not able to know "buffer count in client" in blockpool's aspect. The alternative solution is
    // to set the worse case first, which is equal to the size of |mSlotAllocations|. And in the end
    // of updateGraphicBlock() routine, we could get the arbitrary "buffer count in client" by
    // counting the calls of updateGraphicBlock(willCancel=true). Then we set maxDequeuedBufferCount
    // again to the correct value.
    if (newProducer->setMaxDequeuedBufferCount(mSlotAllocations.size() * 2) != android::NO_ERROR) {
        return false;
    }

    // Reset "buffer count in client". It will be accumulated in updateGraphicBlock() routine.
    mBuffersInClient = 0;

    // Set allowAllocation to new producer.
    if (newProducer->allowAllocation(true) != android::NO_ERROR) {
        return false;
    }

    // Get a buffer from the new producer to get the generation number and usage of new producer.
    // While attaching buffers, generation number and usage must be aligned to the producer.
    uint32_t newGeneration;
    uint64_t newUsage;
    c2_status_t err = queryGenerationAndUsage(newProducer, mBufferFormat.mWidth,
                                              mBufferFormat.mHeight, mBufferFormat.mPixelFormat,
                                              mBufferFormat.mUsage, &newGeneration, &newUsage);
    if (err != C2_OK) {
        ALOGE("queryGenerationAndUsage failed: %d", err);
        return false;
    }

    // Attach all buffers to new producer.
    mProducerChangeSlotMap.clear();
    int32_t slot;
    std::map<int32_t, std::shared_ptr<C2GraphicAllocation>> newSlotAllocations;
    for (auto iter = mSlotAllocations.begin(); iter != mSlotAllocations.end(); ++iter) {
        // Convert C2GraphicAllocation to GraphicBuffer.
        uint32_t width, height, format, stride, igbp_slot, generation;
        uint64_t usage, igbp_id;
        android::_UnwrapNativeCodec2GrallocMetadata(iter->second->handle(), &width, &height,
                                                    &format, &usage, &stride, &generation, &igbp_id,
                                                    &igbp_slot);
        native_handle_t* grallocHandle =
                android::UnwrapNativeCodec2GrallocHandle(iter->second->handle());

        // Update generation number and usage.
        sp<GraphicBuffer> graphicBuffer =
                new GraphicBuffer(grallocHandle, GraphicBuffer::CLONE_HANDLE, width, height, format,
                                  1, newUsage, stride);
        if (graphicBuffer->initCheck() != android::NO_ERROR) {
            ALOGE("Failed to create GraphicBuffer: %d", graphicBuffer->initCheck());
            return false;
        }
        graphicBuffer->setGenerationNumber(newGeneration);
        native_handle_delete(grallocHandle);

        if (newProducer->attachBuffer(graphicBuffer, &slot) != android::NO_ERROR) {
            return false;
        }
        // Convert back to C2GraphicAllocation wrapping new producer id, generation number, usage
        // and slot index.
        ALOGV("buffer wraps { producer id: %" PRIu64 ", slot: %d }", newProducerId, slot);
        C2Handle* c2Handle = android::WrapNativeCodec2GrallocHandle(
                graphicBuffer->handle, width, height, format, newUsage, stride, newGeneration,
                newProducerId, slot);
        if (!c2Handle) {
            ALOGE("WrapNativeCodec2GrallocHandle failed");
            return false;
        }
        std::shared_ptr<C2GraphicAllocation> alloc;
        c2_status_t err = mAllocator->priorGraphicAllocation(c2Handle, &alloc);
        if (err != C2_OK) {
            ALOGE("priorGraphicAllocation failed: %d", err);
            return false;
        }

        // Store to |newSlotAllocations| and also store old-to-new producer slot map.
        ALOGV("Transfered buffer from old producer to new, slot prev: %d -> new %d", iter->first,
              slot);
        newSlotAllocations[slot] = std::move(alloc);
        mProducerChangeSlotMap[iter->first] = slot;
    }

    // Set allowAllocation to false so producer could not allocate new buffers.
    if (newProducer->allowAllocation(false) != android::NO_ERROR) {
        ALOGE("allowAllocation(false) failed");
        return false;
    }

    // Try to detach all buffers from old producer.
    for (const auto& slotAllocation : mSlotAllocations) {
        status_t status = mProducer->detachBuffer(slotAllocation.first);
        if (status != android::NO_ERROR) {
            ALOGW("detachBuffer slot=%d from old producer failed: %d", slotAllocation.first,
                  status);
        }
    }

    mSlotAllocations = std::move(newSlotAllocations);
    return true;
}

c2_status_t C2VdaBqBlockPool::Impl::updateGraphicBlock(
        bool willCancel, uint32_t oldSlot, uint32_t* newSlot,
        std::shared_ptr<C2GraphicBlock>* block /* nonnull */) {
    std::lock_guard<std::mutex> lock(mMutex);

    if (mProducerChangeSlotMap.empty()) {
        ALOGD("A new buffer set is requested right after producer change, no more update needed.");
        return C2_CANCELED;
    }

    auto it = mProducerChangeSlotMap.find(static_cast<int32_t>(oldSlot));
    if (it == mProducerChangeSlotMap.end()) {
        ALOGE("Cannot find old slot = %u in map...", oldSlot);
        return C2_NOT_FOUND;
    }

    int32_t slot = it->second;
    *newSlot = static_cast<uint32_t>(slot);
    mProducerChangeSlotMap.erase(it);

    if (willCancel) {
        sp<Fence> fence = new Fence();
        // The old C2GraphicBlock might be owned by client. Cancel this slot.
        if (mProducer->cancelBuffer(slot, fence) != android::NO_ERROR) {
            return C2_CORRUPTED;
        }
        // Client might try to attach the old buffer to the current producer on client's end,
        // although it is useless for us anymore. However it will still occupy an available slot.
        mBuffersInClient++;
    } else {
        // The old C2GraphicBlock is still owned by component, replace by the new one and keep this
        // slot dequeued.
        auto poolData =
                std::make_shared<C2VdaBqBlockPoolData>(mProducerId, slot, shared_from_this());
        *block = _C2BlockFactory::CreateGraphicBlock(mSlotAllocations[slot], std::move(poolData));
    }

    if (mProducerChangeSlotMap.empty()) {
        // The updateGraphicBlock() routine is about to finish.
        // Set the correct maxDequeuedBufferCount to producer, which is "requested buffer count" +
        // "buffer count in client".
        ALOGV("Requested buffer count: %zu, buffer count in client: %u", mSlotAllocations.size(),
              mBuffersInClient);
        if (mProducer->setMaxDequeuedBufferCount(mSlotAllocations.size() + mBuffersInClient) !=
            android::NO_ERROR) {
            return C2_CORRUPTED;
        }
        mProducerSwitched = true;
    }

    return C2_OK;
}

c2_status_t C2VdaBqBlockPool::Impl::getMinBuffersForDisplay(size_t* bufferCount) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mProducer) {
        ALOGD("No HGraphicBufferProducer is configured...");
        return C2_NO_INIT;
    }

    int32_t status, value;
    status = mProducer->query(NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &value);
    if (status != android::NO_ERROR) {
        ALOGE("query(NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS) failed: %d", status);
        return asC2Error(status);
    }
    if (value <= 0) {
        ALOGE("Illegal value of NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS = %d", value);
        return C2_BAD_VALUE;
    }
    *bufferCount = static_cast<size_t>(value);
    return C2_OK;
}

void C2VdaBqBlockPool::Impl::detachBuffer(uint64_t producerId, int32_t slotId) {
    ALOGV("detachBuffer: producer id = %" PRIu64 ", slot = %d", producerId, slotId);
    std::lock_guard<std::mutex> lock(mMutex);
    if (producerId == mProducerId && mProducer) {
        if (mProducer->detachBuffer(slotId) != android::NO_ERROR) {
            return;
        }

        auto it = mSlotAllocations.find(slotId);
        // It may happen that the slot is not included in |mSlotAllocations|, which means it is
        // released after resolution change.
        if (it != mSlotAllocations.end()) {
            mSlotAllocations.erase(it);
        }
    }
}

bool C2VdaBqBlockPool::Impl::setNotifyBlockAvailableCb(::base::OnceClosure cb) {
    ALOGV("%s()", __func__);
    if (mFetchBufferNotifier == nullptr) {
        return false;
    }

    ::base::OnceClosure outputCb;
    {
        std::lock_guard<std::mutex> lock(mBufferReleaseMutex);

        // If there is any buffer released after dequeueBuffer() timed out, then we could notify the
        // caller directly.
        if (mBufferReleasedAfterTimedOut) {
            outputCb = std::move(cb);
        } else {
            mNotifyBlockAvailableCb = std::move(cb);
        }
    }

    // Calling the callback outside the lock to avoid the deadlock.
    if (outputCb) {
        std::move(outputCb).Run();
    }
    return true;
}

C2VdaBqBlockPool::C2VdaBqBlockPool(const std::shared_ptr<C2Allocator>& allocator,
                                   const local_id_t localId)
      : C2BufferQueueBlockPool(allocator, localId), mLocalId(localId), mImpl(new Impl(allocator)) {}

c2_status_t C2VdaBqBlockPool::fetchGraphicBlock(
        uint32_t width, uint32_t height, uint32_t format, C2MemoryUsage usage,
        std::shared_ptr<C2GraphicBlock>* block /* nonnull */) {
    if (mImpl) {
        return mImpl->fetchGraphicBlock(width, height, format, usage, block);
    }
    return C2_NO_INIT;
}

void C2VdaBqBlockPool::setRenderCallback(
        const C2BufferQueueBlockPool::OnRenderCallback& renderCallback) {
    if (mImpl) {
        mImpl->setRenderCallback(renderCallback);
    }
}

c2_status_t C2VdaBqBlockPool::requestNewBufferSet(int32_t bufferCount) {
    if (mImpl) {
        return mImpl->requestNewBufferSet(bufferCount);
    }
    return C2_NO_INIT;
}

void C2VdaBqBlockPool::configureProducer(const sp<HGraphicBufferProducer>& producer) {
    if (mImpl) {
        mImpl->configureProducer(producer);
    }
}

c2_status_t C2VdaBqBlockPool::updateGraphicBlock(
        bool willCancel, uint32_t oldSlot, uint32_t* newSlot,
        std::shared_ptr<C2GraphicBlock>* block /* nonnull */) {
    if (mImpl) {
        return mImpl->updateGraphicBlock(willCancel, oldSlot, newSlot, block);
    }
    return C2_NO_INIT;
}

c2_status_t C2VdaBqBlockPool::getMinBuffersForDisplay(size_t* bufferCount) {
    if (mImpl) {
        return mImpl->getMinBuffersForDisplay(bufferCount);
    }
    return C2_NO_INIT;
}

bool C2VdaBqBlockPool::setNotifyBlockAvailableCb(::base::OnceClosure cb) {
    if (mImpl) {
        return mImpl->setNotifyBlockAvailableCb(std::move(cb));
    }
    return false;
}

C2VdaBqBlockPoolData::C2VdaBqBlockPoolData(uint64_t producerId, int32_t slotId,
                                           const std::shared_ptr<C2VdaBqBlockPool::Impl>& pool)
      : mProducerId(producerId), mSlotId(slotId), mPool(pool) {}

C2VdaBqBlockPoolData::~C2VdaBqBlockPoolData() {
    if (mShared || !mPool) {
        return;
    }
    mPool->detachBuffer(mProducerId, mSlotId);
}

}  // namespace android
