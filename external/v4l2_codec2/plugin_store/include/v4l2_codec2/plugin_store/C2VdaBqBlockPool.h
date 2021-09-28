// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_V4L2_CODEC2_PLUGIN_STORE_C2_VDA_BQ_BLOCK_POOL_H
#define ANDROID_V4L2_CODEC2_PLUGIN_STORE_C2_VDA_BQ_BLOCK_POOL_H

#include <functional>
#include <map>
#include <optional>

#include <C2BqBufferPriv.h>
#include <C2Buffer.h>
#include <C2PlatformSupport.h>
#include <base/callback_forward.h>

namespace android {

/**
 * Marks the BlockPoolData in |sharedBlock| as shared. The destructor of BlockPoolData would not
 * call detachBuffer to BufferQueue if it is shared.
 *
 * \param sharedBlock  the C2ConstGraphicBlock which is about to pass to client.
 */
c2_status_t MarkBlockPoolDataAsShared(const C2ConstGraphicBlock& sharedBlock);

/**
 * The BufferQueue-backed block pool design which supports to request arbitrary count of graphic
 * buffers from IGBP, and use this buffer set among codec component and client.
 *
 * The block pool should restore the mapping table between slot indices and GraphicBuffer (or
 * C2GraphicAllocation). When component requests a new buffer, the block pool calls dequeueBuffer
 * to IGBP to obtain a valid slot index, and returns the corresponding buffer from map.
 */
class C2VdaBqBlockPool : public C2BufferQueueBlockPool {
public:
    C2VdaBqBlockPool(const std::shared_ptr<C2Allocator>& allocator, const local_id_t localId);

    ~C2VdaBqBlockPool() override = default;

    /**
     * Extracts slot index as pool ID from the graphic block.
     *
     * \note C2VdaBqBlockPool-specific function
     *
     * \param block  the graphic block allocated by bufferqueue block pool.
     *
     * Return the buffer's slot index in bufferqueue if extraction is successful.
     * Otherwise return std::nullopt.
     */
    static std::optional<uint32_t> getBufferIdFromGraphicBlock(const C2Block2D& block);

    /**
     * It's a trick here. Return C2PlatformAllocatorStore::BUFFERQUEUE instead of the ID of backing
     * allocator for client's query. It's because in platform side this ID is recognized as
     * BufferQueue-backed block pool which is only allowed to set surface.
     */
    C2Allocator::id_t getAllocatorId() const override {
        return android::C2PlatformAllocatorStore::BUFFERQUEUE;
    };

    local_id_t getLocalId() const override { return mLocalId; };

    /**
     * Tries to dequeue a buffer from producer. If the producer is allowed allocation now, call
     * requestBuffer of dequeued slot for allocating new buffer and storing the reference into
     * |mSlotAllocations|.
     *
     * When the size of |mSlotAllocations| reaches the requested buffer count, set disallow
     * allocation to producer. After that buffer set is started to be recycled by dequeue.
     *
     * \retval C2_BAD_STATE informs the caller producer is switched.
     */
    c2_status_t fetchGraphicBlock(uint32_t width, uint32_t height, uint32_t format,
                                  C2MemoryUsage usage,
                                  std::shared_ptr<C2GraphicBlock>* block /* nonnull */) override;

    void setRenderCallback(const C2BufferQueueBlockPool::OnRenderCallback& renderCallback =
                                   C2BufferQueueBlockPool::OnRenderCallback()) override;
    void configureProducer(const android::sp<HGraphicBufferProducer>& producer) override;

    /**
     * Sends the request of arbitrary number of graphic buffers allocation. If producer is given,
     * it will set maxDequeuedBufferCount with regard to the requested buffer count and allow
     * allocation to producer.
     *
     * \note C2VdaBqBlockPool-specific function
     * \note caller should release all buffer references obtained from fetchGraphicBlock() before
     *       calling this function.
     *
     * \param bufferCount  the number of requested buffers
     *
     * \retval C2_OK        the operation was successful.
     * \retval C2_NO_INIT   this class is not initialized, or producer is not assigned.
     * \retval C2_BAD_VALUE |bufferCount| is not greater than zero.
     * \retval C2_CORRUPTED some unknown, unrecoverable error occured during operation (unexpected).
     */
    c2_status_t requestNewBufferSet(int32_t bufferCount);

    /**
     * Updates the buffer from producer switch.
     *
     * \note C2VdaBqBlockPool-specific function
     *
     * \param willCancel if true, the corresponding slot will be canceled to new producer. Otherwise
     *                   the new graphic block will be returned as |block|.
     * \param oldSlot    the slot index from old producer the caller provided.
     * \param newSlot    the corresponding slot index of new producer is filled.
     * \param block      if |willCancel| is false, the new graphic block is stored.
     *
     * \retval C2_OK        the operation was successful.
     * \retval C2_NO_INIT   this class is not initialized.
     * \retval C2_NOT_FOUND cannot find |oldSlot| in the slot changing map.
     * \retval C2_CANCELED  indicates buffer format is changed and a new buffer set is allocated, no
     *                      more update needed.
     * \retval C2_CORRUPTED some unknown, unrecoverable error occured during operation (unexpected).
     */
    c2_status_t updateGraphicBlock(bool willCancel, uint32_t oldSlot, uint32_t* newSlot,
                                   std::shared_ptr<C2GraphicBlock>* block /* nonnull */);

    /**
     * Gets minimum undequeued buffer count for display from producer.
     *
     * \note C2VdaBqBlockPool-specific function
     *
     * \param bufferCount   the minimum undequeued buffer count for display is filled.
     *
     * \retval C2_OK        the operation was successful.
     * \retval C2_NO_INIT   this class is not initialized, or producer is not assigned.
     * \retval C2_BAD_VALUE the queried value is illegal (less than 0).
     * \retval C2_CORRUPTED some unknown, unrecoverable error occured during operation (unexpected).
     */
    c2_status_t getMinBuffersForDisplay(size_t* bufferCount);

    /**
     * Set the callback that will be triggered when there is block available.
     *
     * \note C2VdaBqBlockPool-specific function
     *
     * \param cb  the callback function that will be triggered when there is block available.
     *
     * Return false if we don't support to notify the caller when a buffer is available.
     *
     */
    bool setNotifyBlockAvailableCb(base::OnceClosure cb);

private:
    friend struct C2VdaBqBlockPoolData;
    class Impl;

    const local_id_t mLocalId;
    std::shared_ptr<Impl> mImpl;
};

}  // namespace android
#endif  // ANDROID_V4L2_CODEC2_PLUGIN_STORE_C2_VDA_BQ_BLOCK_POOL_H
