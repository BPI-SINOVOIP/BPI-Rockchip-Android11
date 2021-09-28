// Copyright 2020 The Android Open Source Project
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
#include "PixelStreamManager.h"

#include <android-base/logging.h>
#include <vndk/hardware_buffer.h>

#include <algorithm>
#include <mutex>
#include <thread>

#include "PixelFormatUtils.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace stream_manager {

PixelMemHandle::PixelMemHandle(int bufferId, int streamId, int additionalUsageFlags)
    : mBufferId(bufferId),
      mStreamId(streamId),
      mBuffer(nullptr),
      mUsage(AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN | additionalUsageFlags) {
}

PixelMemHandle::~PixelMemHandle() {
    if (mBuffer) {
        AHardwareBuffer_release(mBuffer);
    }
}

int PixelMemHandle::getStreamId() const {
    return mStreamId;
}

proto::PacketType PixelMemHandle::getType() const {
    return proto::PacketType::PIXEL_DATA;
}
uint64_t PixelMemHandle::getTimeStamp() const {
    return mTimestamp;
}

uint32_t PixelMemHandle::getSize() const {
    return 0;
}

const char* PixelMemHandle::getData() const {
    return nullptr;
}

AHardwareBuffer* PixelMemHandle::getHardwareBuffer() const {
    return mBuffer;
}

/* Sets frame info */
Status PixelMemHandle::setFrameData(uint64_t timestamp, const InputFrame& inputFrame) {
    // Allocate a new buffer if it is currently null.
    FrameInfo frameInfo = inputFrame.getFrameInfo();
    if (mBuffer == nullptr) {
        mDesc.format = PixelFormatToHardwareBufferFormat(frameInfo.format);
        mDesc.height = frameInfo.height;
        mDesc.width = frameInfo.width;
        mDesc.layers = 1;
        mDesc.rfu0 = 0;
        mDesc.rfu1 = 0;
        mDesc.stride = frameInfo.stride;
        mDesc.usage = mUsage;
        int err = AHardwareBuffer_allocate(&mDesc, &mBuffer);

        if (err != 0 || mBuffer == nullptr) {
            LOG(ERROR) << "Failed to allocate hardware buffer with error " << err;
            return Status::NO_MEMORY;
        }

        // Update mDesc with the actual descriptor with which the buffer was created. The actual
        // stride could be different from the specified stride.
        AHardwareBuffer_describe(mBuffer, &mDesc);
    }

    // Verifies that the input frame data has the same type as the allocated buffer.
    if (frameInfo.width != mDesc.width || frameInfo.height != mDesc.height ||
        PixelFormatToHardwareBufferFormat(frameInfo.format) != mDesc.format) {
        LOG(ERROR) << "Variable image sizes from the same stream id is not supported.";
        return Status::INVALID_ARGUMENT;
    }

    // Locks the frame for copying the input frame data.
    void* mappedBuffer = nullptr;
    int err = AHardwareBuffer_lock(mBuffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN, -1, nullptr,
                                   &mappedBuffer);
    if (err != 0 || mappedBuffer == nullptr) {
        LOG(ERROR) << "Unable to lock a realased hardware buffer.";
        return Status::INTERNAL_ERROR;
    }

    // Copies the input frame data.
    int bytesPerPixel = numBytesPerPixel(static_cast<AHardwareBuffer_Format>(mDesc.format));
    // The stride for hardware buffer is specified in pixels while the stride
    // for InputFrame data structure is specified in bytes.
    if (mDesc.stride * bytesPerPixel == frameInfo.stride) {
        memcpy(mappedBuffer, inputFrame.getFramePtr(), mDesc.stride * mDesc.height * bytesPerPixel);
    } else {
        for (int y = 0; y < frameInfo.height; y++) {
            memcpy((uint8_t*)mappedBuffer + mDesc.stride * y * bytesPerPixel,
                   inputFrame.getFramePtr() + y * frameInfo.stride,
                   std::min(frameInfo.stride, mDesc.stride * bytesPerPixel));
        }
    }

    AHardwareBuffer_unlock(mBuffer, nullptr);
    mTimestamp = timestamp;

    return Status::SUCCESS;
}

int PixelMemHandle::getBufferId() const {
    return mBufferId;
}

void PixelStreamManager::setEngineInterface(std::shared_ptr<StreamEngineInterface> engine) {
    std::lock_guard lock(mLock);
    mEngine = engine;
}

Status PixelStreamManager::setMaxInFlightPackets(uint32_t maxPackets) {
    std::lock_guard lock(mLock);
    if (mBuffersInUse.size() > maxPackets) {
        LOG(ERROR) << "Cannot set max in flight packets after graph has already started.";
        return Status::ILLEGAL_STATE;
    }

    mMaxInFlightPackets = maxPackets;
    std::lock_guard stateLock(mStateLock);
    mState = CONFIG_DONE;
    return Status::SUCCESS;
}

Status PixelStreamManager::freePacket(int bufferId) {
    std::lock_guard lock(mLock);

    auto it = mBuffersInUse.find(bufferId);

    if (it == mBuffersInUse.end()) {
        std::lock_guard stateLock(mStateLock);
        // If the graph has already been stopped, we free the buffers
        // asynchronously, so return SUCCESS if freePacket is called later.
        if (mState == STOPPED) {
            return Status::SUCCESS;
        }

        LOG(ERROR) << "Unable to find the mem handle. Duplicate release may possible have been "
                      "called";
        return Status::INVALID_ARGUMENT;
    }

    it->second.outstandingRefCount -= 1;
    if (it->second.outstandingRefCount == 0) {
        mBuffersReady.push_back(it->second.handle);
        mBuffersInUse.erase(it);
    }
    return Status::SUCCESS;
}

void PixelStreamManager::freeAllPackets() {
    std::lock_guard lock(mLock);

    for (auto [bufferId, buffer] : mBuffersInUse) {
        mBuffersReady.push_back(buffer.handle);
    }
    mBuffersInUse.clear();
}

Status PixelStreamManager::queuePacket(const char* /*data*/, const uint32_t /*size*/,
                                       uint64_t /*timestamp*/) {
    LOG(ERROR) << "Trying to queue a semantic packet to a pixel stream manager";
    return Status::ILLEGAL_STATE;
}

Status PixelStreamManager::queuePacket(const InputFrame& frame, uint64_t timestamp) {
    std::lock_guard lock(mLock);

    // State has to be running for the callback to go back.
    {
        std::lock_guard stateLock(mStateLock);
        if (mState != RUNNING) {
            LOG(ERROR) << "Packet cannot be queued when state is not RUNNING. Current state is"
                       << mState;
            return Status::ILLEGAL_STATE;
        }
    }

    if (mEngine == nullptr) {
        LOG(ERROR) << "Stream to engine interface is not set";
        return Status::ILLEGAL_STATE;
    }

    if (mBuffersInUse.size() >= mMaxInFlightPackets) {
        LOG(INFO) << "Too many frames in flight. Skipping frame at timestamp " << timestamp;
        return Status::SUCCESS;
    }

    // A unique id per buffer is maintained by incrementing the unique id from the previously
    // created buffer. The unique id is therefore the number of buffers already created.
    if (mBuffersReady.empty()) {
        mBuffersReady.push_back(std::make_shared<PixelMemHandle>(mBuffersInUse.size(), mStreamId));
    }

    // The previously used buffer is pushed to the back of the vector. Picking the last used buffer
    // may be more cache efficient if accessing through CPU, so we use that strategy here.
    std::shared_ptr<PixelMemHandle> memHandle = mBuffersReady[mBuffersReady.size() - 1];
    mBuffersReady.resize(mBuffersReady.size() - 1);

    BufferMetadata bufferMetadata;
    bufferMetadata.outstandingRefCount = 1;
    bufferMetadata.handle = memHandle;

    mBuffersInUse.emplace(memHandle->getBufferId(), bufferMetadata);

    Status status = memHandle->setFrameData(timestamp, frame);
    if (status != Status::SUCCESS) {
        LOG(ERROR) << "Setting frame data failed with error code " << status;
        return status;
    }

    // Dispatch packet to the engine asynchronously in order to avoid circularly
    // waiting for each others' locks.
    std::thread t([this, memHandle]() {
        Status status = mEngine->dispatchPacket(memHandle);
        if (status != Status::SUCCESS) {
            mEngine->notifyError(std::string(__func__) + ":" + std::to_string(__LINE__) +
                                 " Failed to dispatch packet");
        }
    });
    t.detach();
    return Status::SUCCESS;
}

Status PixelStreamManager::handleExecutionPhase(const RunnerEvent& e) {
    std::lock_guard<std::mutex> lock(mStateLock);
    if (mState == CONFIG_DONE && e.isPhaseEntry()) {
        mState = RUNNING;
        return Status::SUCCESS;
    }
    if (mState == RESET) {
        // Cannot get to running phase from reset state without config phase
        return Status::ILLEGAL_STATE;
    }
    if (mState == RUNNING && e.isAborted()) {
        // Transition back to config completed
        mState = CONFIG_DONE;
        return Status::SUCCESS;
    }
    if (mState == RUNNING) {
        return Status::ILLEGAL_STATE;
    }
    return Status::SUCCESS;
}

Status PixelStreamManager::handleStopWithFlushPhase(const RunnerEvent& e) {
    return handleStopImmediatePhase(e);
}

Status PixelStreamManager::handleStopImmediatePhase(const RunnerEvent& e) {
    std::lock_guard<std::mutex> lock(mStateLock);
    if (mState == CONFIG_DONE || mState == RESET) {
        return ILLEGAL_STATE;
    }
    /* Cannot have stop completed if we never entered stop state */
    if (mState == RUNNING && (e.isAborted() || e.isTransitionComplete())) {
        return ILLEGAL_STATE;
    }
    /* We are being asked to stop */
    if (mState == RUNNING && e.isPhaseEntry()) {
        mState = STOPPED;
        std::thread t([this]() {
            freeAllPackets();
            mEngine->notifyEndOfStream();
        });
        t.detach();
        return SUCCESS;
    }
    /* Other Components have stopped, we can transition back to CONFIG_DONE */
    if (mState == STOPPED && e.isTransitionComplete()) {
        mState = CONFIG_DONE;
        return SUCCESS;
    }
    /* We were stopped, but stop was aborted. */
    if (mState == STOPPED && e.isAborted()) {
        mState = RUNNING;
        return SUCCESS;
    }
    return SUCCESS;
}

std::shared_ptr<MemHandle> PixelStreamManager::clonePacket(std::shared_ptr<MemHandle> handle) {
    if (!handle) {
        LOG(ERROR) << "PixelStreamManager - Received null memhandle.";
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(mLock);
    int bufferId = handle->getBufferId();
    auto it = mBuffersInUse.find(bufferId);
    if (it == mBuffersInUse.end()) {
        LOG(ERROR) << "PixelStreamManager - Attempting to clone an already freed packet.";
        return nullptr;
    }
    it->second.outstandingRefCount += 1;
    return handle;
}

PixelStreamManager::PixelStreamManager(std::string name, int streamId)
    : StreamManager(name, proto::PacketType::PIXEL_DATA), mStreamId(streamId) {
}

}  // namespace stream_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
