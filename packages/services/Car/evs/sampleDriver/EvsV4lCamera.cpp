/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "EvsV4lCamera.h"
#include "EvsEnumerator.h"
#include "bufferCopy.h"

#include <android/hardware_buffer.h>
#include <android-base/logging.h>
#include <ui/GraphicBufferAllocator.h>
#include <ui/GraphicBufferMapper.h>
#include <utils/SystemClock.h>


namespace android {
namespace hardware {
namespace automotive {
namespace evs {
namespace V1_1 {
namespace implementation {

// Default camera output image resolution
const std::array<int32_t, 2> kDefaultResolution = {640, 480};

// Arbitrary limit on number of graphics buffers allowed to be allocated
// Safeguards against unreasonable resource consumption and provides a testable limit
static const unsigned MAX_BUFFERS_IN_FLIGHT = 100;

EvsV4lCamera::EvsV4lCamera(const char *deviceName,
                           unique_ptr<ConfigManager::CameraInfo> &camInfo) :
        mFramesAllowed(0),
        mFramesInUse(0),
        mCameraInfo(camInfo) {
    LOG(DEBUG) << "EvsV4lCamera instantiated";

    mDescription.v1.cameraId = deviceName;
    if (camInfo != nullptr) {
        mDescription.metadata.setToExternal((uint8_t *)camInfo->characteristics,
                                            get_camera_metadata_size(camInfo->characteristics));
    }

    // Default output buffer format.
    mFormat = HAL_PIXEL_FORMAT_RGBA_8888;

    // How we expect to use the gralloc buffers we'll exchange with our client
    mUsage  = GRALLOC_USAGE_HW_TEXTURE     |
              GRALLOC_USAGE_SW_READ_RARELY |
              GRALLOC_USAGE_SW_WRITE_OFTEN;
}


EvsV4lCamera::~EvsV4lCamera() {
    LOG(DEBUG) << "EvsV4lCamera being destroyed";
    shutdown();
}


//
// This gets called if another caller "steals" ownership of the camera
//
void EvsV4lCamera::shutdown()
{
    LOG(DEBUG) << "EvsV4lCamera shutdown";

    // Make sure our output stream is cleaned up
    // (It really should be already)
    stopVideoStream();

    // Note:  Since stopVideoStream is blocking, no other threads can now be running

    // Close our video capture device
    mVideo.close();

    // Drop all the graphics buffers we've been using
    if (mBuffers.size() > 0) {
        GraphicBufferAllocator& alloc(GraphicBufferAllocator::get());
        for (auto&& rec : mBuffers) {
            if (rec.inUse) {
                LOG(WARNING) << "Releasing buffer despite remote ownership";
            }
            alloc.free(rec.handle);
            rec.handle = nullptr;
        }
        mBuffers.clear();
    }
}


// Methods from ::android::hardware::automotive::evs::V1_0::IEvsCamera follow.
Return<void> EvsV4lCamera::getCameraInfo(getCameraInfo_cb _hidl_cb) {
    LOG(DEBUG) << __FUNCTION__;

    // Send back our self description
    _hidl_cb(mDescription.v1);
    return Void();
}


Return<EvsResult> EvsV4lCamera::setMaxFramesInFlight(uint32_t bufferCount) {
    LOG(DEBUG) << __FUNCTION__;
    std::lock_guard<std::mutex> lock(mAccessLock);

    // If we've been displaced by another owner of the camera, then we can't do anything else
    if (!mVideo.isOpen()) {
        LOG(WARNING) << "Ignoring setMaxFramesInFlight call when camera has been lost.";
        return EvsResult::OWNERSHIP_LOST;
    }

    // We cannot function without at least one video buffer to send data
    if (bufferCount < 1) {
        LOG(ERROR) << "Ignoring setMaxFramesInFlight with less than one buffer requested";
        return EvsResult::INVALID_ARG;
    }

    // Update our internal state
    if (setAvailableFrames_Locked(bufferCount)) {
        return EvsResult::OK;
    } else {
        return EvsResult::BUFFER_NOT_AVAILABLE;
    }
}


Return<EvsResult> EvsV4lCamera::startVideoStream(const sp<IEvsCameraStream_1_0>& stream)  {
    LOG(DEBUG) << __FUNCTION__;
    std::lock_guard<std::mutex> lock(mAccessLock);

    // If we've been displaced by another owner of the camera, then we can't do anything else
    if (!mVideo.isOpen()) {
        LOG(WARNING) << "Ignoring startVideoStream call when camera has been lost.";
        return EvsResult::OWNERSHIP_LOST;
    }
    if (mStream.get() != nullptr) {
        LOG(ERROR) << "Ignoring startVideoStream call when a stream is already running.";
        return EvsResult::STREAM_ALREADY_RUNNING;
    }

    // If the client never indicated otherwise, configure ourselves for a single streaming buffer
    if (mFramesAllowed < 1) {
        if (!setAvailableFrames_Locked(1)) {
            LOG(ERROR) << "Failed to start stream because we couldn't get a graphics buffer";
            return EvsResult::BUFFER_NOT_AVAILABLE;
        }
    }

    // Choose which image transfer function we need
    // Map from V4L2 to Android graphic buffer format
    const uint32_t videoSrcFormat = mVideo.getV4LFormat();
    LOG(INFO) << "Configuring to accept " << (char*)&videoSrcFormat
              << " camera data and convert to " << std::hex << mFormat;

    switch (mFormat) {
    case HAL_PIXEL_FORMAT_YCRCB_420_SP:
        switch (videoSrcFormat) {
        case V4L2_PIX_FMT_NV21:     mFillBufferFromVideo = fillNV21FromNV21;    break;
        case V4L2_PIX_FMT_YUYV:     mFillBufferFromVideo = fillNV21FromYUYV;    break;
        default:
            LOG(ERROR) << "Unhandled camera output format: "
                       << ((char*)&videoSrcFormat)[0]
                       << ((char*)&videoSrcFormat)[1]
                       << ((char*)&videoSrcFormat)[2]
                       << ((char*)&videoSrcFormat)[3]
                       << std::hex << videoSrcFormat;
        }
        break;
    case HAL_PIXEL_FORMAT_RGBA_8888:
        switch (videoSrcFormat) {
        case V4L2_PIX_FMT_YUYV:     mFillBufferFromVideo = fillRGBAFromYUYV;    break;
        default:
            LOG(ERROR) << "Unhandled camera format " << (char*)&videoSrcFormat;
        }
        break;
    case HAL_PIXEL_FORMAT_YCBCR_422_I:
        switch (videoSrcFormat) {
        case V4L2_PIX_FMT_YUYV:     mFillBufferFromVideo = fillYUYVFromYUYV;    break;
        case V4L2_PIX_FMT_UYVY:     mFillBufferFromVideo = fillYUYVFromUYVY;    break;
        default:
            LOG(ERROR) << "Unhandled camera format " << (char*)&videoSrcFormat;
        }
        break;
    default:
        LOG(ERROR) << "Unhandled camera format " << (char*)&mFormat;
    }


    // Record the user's callback for use when we have a frame ready
    mStream = stream;
    mStream_1_1 = IEvsCameraStream_1_1::castFrom(mStream).withDefault(nullptr);

    // Set up the video stream with a callback to our member function forwardFrame()
    if (!mVideo.startStream([this](VideoCapture*, imageBuffer* tgt, void* data) {
                                this->forwardFrame(tgt, data);
                            })
    ) {
        // No need to hold onto this if we failed to start
        mStream = nullptr;
        mStream_1_1 = nullptr;
        LOG(ERROR) << "Underlying camera start stream failed";
        return EvsResult::UNDERLYING_SERVICE_ERROR;
    }

    return EvsResult::OK;
}


Return<void> EvsV4lCamera::doneWithFrame(const BufferDesc_1_0& buffer)  {
    LOG(DEBUG) << __FUNCTION__;
    doneWithFrame_impl(buffer.bufferId, buffer.memHandle);

    return Void();
}


Return<void> EvsV4lCamera::stopVideoStream()  {
    LOG(DEBUG) << __FUNCTION__;

    // Tell the capture device to stop (and block until it does)
    mVideo.stopStream();

    if (mStream_1_1 != nullptr) {
        // V1.1 client is waiting on STREAM_STOPPED event.
        std::unique_lock <std::mutex> lock(mAccessLock);

        EvsEventDesc event;
        event.aType = EvsEventType::STREAM_STOPPED;
        auto result = mStream_1_1->notify(event);
        if (!result.isOk()) {
            LOG(ERROR) << "Error delivering end of stream event";
        }

        // Drop our reference to the client's stream receiver
        mStream_1_1 = nullptr;
        mStream     = nullptr;
    } else if (mStream != nullptr) {
        std::unique_lock <std::mutex> lock(mAccessLock);

        // Send one last NULL frame to signal the actual end of stream
        BufferDesc_1_0 nullBuff = {};
        auto result = mStream->deliverFrame(nullBuff);
        if (!result.isOk()) {
            LOG(ERROR) << "Error delivering end of stream marker";
        }

        // Drop our reference to the client's stream receiver
        mStream = nullptr;
    }

    return Void();
}


Return<int32_t> EvsV4lCamera::getExtendedInfo(uint32_t /*opaqueIdentifier*/)  {
    LOG(DEBUG) << __FUNCTION__;
    // Return zero by default as required by the spec
    return 0;
}


Return<EvsResult> EvsV4lCamera::setExtendedInfo(uint32_t /*opaqueIdentifier*/,
                                                int32_t  /*opaqueValue*/)  {
    LOG(DEBUG) << __FUNCTION__;
    std::lock_guard<std::mutex> lock(mAccessLock);

    // If we've been displaced by another owner of the camera, then we can't do anything else
    if (!mVideo.isOpen()) {
        LOG(WARNING) << "Ignoring setExtendedInfo call when camera has been lost.";
        return EvsResult::OWNERSHIP_LOST;
    }

    // We don't store any device specific information in this implementation
    return EvsResult::INVALID_ARG;
}


// Methods from ::android::hardware::automotive::evs::V1_1::IEvsCamera follow.
Return<void> EvsV4lCamera::getCameraInfo_1_1(getCameraInfo_1_1_cb _hidl_cb) {
    LOG(DEBUG) << __FUNCTION__;

    // Send back our self description
    _hidl_cb(mDescription);
    return Void();
}


Return<void> EvsV4lCamera::getPhysicalCameraInfo(const hidl_string& id,
                                                 getPhysicalCameraInfo_cb _hidl_cb) {
    LOG(DEBUG) << __FUNCTION__;

    // This method works exactly same as getCameraInfo_1_1() in EVS HW module.
    (void)id;
    _hidl_cb(mDescription);
    return Void();
}


Return<EvsResult> EvsV4lCamera::doneWithFrame_1_1(const hidl_vec<BufferDesc_1_1>& buffers)  {
    LOG(DEBUG) << __FUNCTION__;

    for (auto&& buffer : buffers) {
        doneWithFrame_impl(buffer.bufferId, buffer.buffer.nativeHandle);
    }

    return EvsResult::OK;
}


Return<EvsResult> EvsV4lCamera::pauseVideoStream() {
    return EvsResult::UNDERLYING_SERVICE_ERROR;
}


Return<EvsResult> EvsV4lCamera::resumeVideoStream() {
    return EvsResult::UNDERLYING_SERVICE_ERROR;
}


Return<EvsResult> EvsV4lCamera::setMaster() {
    /* Because EVS HW module reference implementation expects a single client at
     * a time, this returns a success code always.
     */
    return EvsResult::OK;
}


Return<EvsResult> EvsV4lCamera::forceMaster(const sp<IEvsDisplay_1_0>&) {
    /* Because EVS HW module reference implementation expects a single client at
     * a time, this returns a success code always.
     */
    return EvsResult::OK;
}


Return<EvsResult> EvsV4lCamera::unsetMaster() {
    /* Because EVS HW module reference implementation expects a single client at
     * a time, there is no chance that this is called by a non-master client and
     * therefore returns a success code always.
     */
    return EvsResult::OK;
}


Return<void> EvsV4lCamera::getParameterList(getParameterList_cb _hidl_cb) {
    hidl_vec<CameraParam> hidlCtrls;
    if (mCameraInfo != nullptr) {
        hidlCtrls.resize(mCameraInfo->controls.size());
        unsigned idx = 0;
        for (auto& [cid, range]: mCameraInfo->controls) {
            hidlCtrls[idx++] = cid;
        }
    }

    _hidl_cb(hidlCtrls);
    return Void();
}


Return<void> EvsV4lCamera::getIntParameterRange(CameraParam id,
                                                getIntParameterRange_cb _hidl_cb) {
    if (mCameraInfo != nullptr) {
        auto range = mCameraInfo->controls[id];
        _hidl_cb(get<0>(range), get<1>(range), get<2>(range));
    } else {
        _hidl_cb(0, 0, 0);
    }

    return Void();
}


Return<void> EvsV4lCamera::setIntParameter(CameraParam id, int32_t value,
                                           setIntParameter_cb _hidl_cb) {
    uint32_t v4l2cid = V4L2_CID_BASE;
    hidl_vec<int32_t> values;
    values.resize(1);
    if (!convertToV4l2CID(id, v4l2cid)) {
        _hidl_cb(EvsResult::INVALID_ARG, values);
    } else {
        EvsResult result = EvsResult::OK;
        v4l2_control control = {v4l2cid, value};
        if (mVideo.setParameter(control) < 0 ||
            mVideo.getParameter(control) < 0) {
            result = EvsResult::UNDERLYING_SERVICE_ERROR;
        }

        values[0] = control.value;
        _hidl_cb(result, values);
    }

    return Void();
}


Return<void> EvsV4lCamera::getIntParameter(CameraParam id,
                                           getIntParameter_cb _hidl_cb) {
    uint32_t v4l2cid = V4L2_CID_BASE;
    hidl_vec<int32_t> values;
    values.resize(1);
    if (!convertToV4l2CID(id, v4l2cid)) {
        _hidl_cb(EvsResult::INVALID_ARG, values);
    } else {
        EvsResult result = EvsResult::OK;
        v4l2_control control = {v4l2cid, 0};
        if (mVideo.getParameter(control) < 0) {
            result = EvsResult::INVALID_ARG;
        }

        // Report a result
        values[0] = control.value;
        _hidl_cb(result, values);
    }

    return Void();
}


Return<EvsResult> EvsV4lCamera::setExtendedInfo_1_1(uint32_t opaqueIdentifier,
                                                    const hidl_vec<uint8_t>& opaqueValue) {
    mExtInfo.insert_or_assign(opaqueIdentifier, opaqueValue);
    return EvsResult::OK;
}


Return<void> EvsV4lCamera::getExtendedInfo_1_1(uint32_t opaqueIdentifier,
                                               getExtendedInfo_1_1_cb _hidl_cb) {
    const auto it = mExtInfo.find(opaqueIdentifier);
    hidl_vec<uint8_t> value;
    auto status = EvsResult::OK;
    if (it == mExtInfo.end()) {
        status = EvsResult::INVALID_ARG;
    } else {
        value = mExtInfo[opaqueIdentifier];
    }

    _hidl_cb(status, value);
    return Void();
}


Return<void>
EvsV4lCamera::importExternalBuffers(const hidl_vec<BufferDesc_1_1>& buffers,
                                    importExternalBuffers_cb _hidl_cb) {
    LOG(DEBUG) << __FUNCTION__;

    // If we've been displaced by another owner of the camera, then we can't do anything else
    if (!mVideo.isOpen()) {
        LOG(WARNING) << "Ignoring a request add external buffers "
                     << "when camera has been lost.";
        _hidl_cb(EvsResult::UNDERLYING_SERVICE_ERROR, mFramesAllowed);
        return {};
    }

    auto numBuffersToAdd = buffers.size();
    if (numBuffersToAdd < 1) {
        LOG(DEBUG) << "No buffers to add.";
        _hidl_cb(EvsResult::OK, mFramesAllowed);
        return {};
    }

    {
        std::scoped_lock<std::mutex> lock(mAccessLock);

        if (numBuffersToAdd > (MAX_BUFFERS_IN_FLIGHT - mFramesAllowed)) {
            numBuffersToAdd -= (MAX_BUFFERS_IN_FLIGHT - mFramesAllowed);
            LOG(WARNING) << "Exceed the limit on number of buffers.  "
                         << numBuffersToAdd << " buffers will be added only.";
        }

        GraphicBufferMapper& mapper = GraphicBufferMapper::get();
        const auto before = mFramesAllowed;
        for (auto i = 0; i < numBuffersToAdd; ++i) {
            // TODO: reject if external buffer is configured differently.
            auto& b = buffers[i];
            const AHardwareBuffer_Desc* pDesc =
                reinterpret_cast<const AHardwareBuffer_Desc *>(&b.buffer.description);

            // Import a buffer to add
            buffer_handle_t memHandle = nullptr;
            status_t result = mapper.importBuffer(b.buffer.nativeHandle,
                                                  pDesc->width,
                                                  pDesc->height,
                                                  1,
                                                  pDesc->format,
                                                  pDesc->usage,
                                                  pDesc->stride,
                                                  &memHandle);
            if (result != android::NO_ERROR || !memHandle) {
                LOG(WARNING) << "Failed to import a buffer " << b.bufferId;
                continue;
            }

            auto stored = false;
            for (auto&& rec : mBuffers) {
                if (rec.handle == nullptr) {
                    // Use this existing entry
                    rec.handle = memHandle;
                    rec.inUse = false;

                    stored = true;
                    break;
                }
            }

            if (!stored) {
                // Add a BufferRecord wrapping this handle to our set of available buffers
                mBuffers.emplace_back(memHandle);
            }

            ++mFramesAllowed;
        }

        _hidl_cb(EvsResult::OK, mFramesAllowed - before);
        return {};
    }
}


EvsResult EvsV4lCamera::doneWithFrame_impl(const uint32_t bufferId,
                                           const buffer_handle_t memHandle) {
    std::lock_guard <std::mutex> lock(mAccessLock);

    // If we've been displaced by another owner of the camera, then we can't do anything else
    if (!mVideo.isOpen()) {
        LOG(WARNING) << "Ignoring doneWithFrame call when camera has been lost.";
    } else {
        if (memHandle == nullptr) {
            LOG(ERROR) << "Ignoring doneWithFrame called with null handle";
        } else if (bufferId >= mBuffers.size()) {
            LOG(ERROR) << "Ignoring doneWithFrame called with invalid bufferId " << bufferId
                       << " (max is " << mBuffers.size() - 1 << ")";
        } else if (!mBuffers[bufferId].inUse) {
            LOG(ERROR) << "Ignoring doneWithFrame called on frame " << bufferId
                       << " which is already free";
        } else {
            // Mark the frame as available
            mBuffers[bufferId].inUse = false;
            mFramesInUse--;

            // If this frame's index is high in the array, try to move it down
            // to improve locality after mFramesAllowed has been reduced.
            if (bufferId >= mFramesAllowed) {
                // Find an empty slot lower in the array (which should always exist in this case)
                for (auto&& rec : mBuffers) {
                    if (rec.handle == nullptr) {
                        rec.handle = mBuffers[bufferId].handle;
                        mBuffers[bufferId].handle = nullptr;
                        break;
                    }
                }
            }
        }
    }

    return EvsResult::OK;
}


bool EvsV4lCamera::setAvailableFrames_Locked(unsigned bufferCount) {
    if (bufferCount < 1) {
        LOG(ERROR) << "Ignoring request to set buffer count to zero";
        return false;
    }
    if (bufferCount > MAX_BUFFERS_IN_FLIGHT) {
        LOG(ERROR) << "Rejecting buffer request in excess of internal limit";
        return false;
    }

    // Is an increase required?
    if (mFramesAllowed < bufferCount) {
        // An increase is required
        unsigned needed = bufferCount - mFramesAllowed;
        LOG(INFO) << "Allocating " << needed << " buffers for camera frames";

        unsigned added = increaseAvailableFrames_Locked(needed);
        if (added != needed) {
            // If we didn't add all the frames we needed, then roll back to the previous state
            LOG(ERROR) << "Rolling back to previous frame queue size";
            decreaseAvailableFrames_Locked(added);
            return false;
        }
    } else if (mFramesAllowed > bufferCount) {
        // A decrease is required
        unsigned framesToRelease = mFramesAllowed - bufferCount;
        LOG(INFO) << "Returning " << framesToRelease << " camera frame buffers";

        unsigned released = decreaseAvailableFrames_Locked(framesToRelease);
        if (released != framesToRelease) {
            // This shouldn't happen with a properly behaving client because the client
            // should only make this call after returning sufficient outstanding buffers
            // to allow a clean resize.
            LOG(ERROR) << "Buffer queue shrink failed -- too many buffers currently in use?";
        }
    }

    return true;
}


unsigned EvsV4lCamera::increaseAvailableFrames_Locked(unsigned numToAdd) {
    // Acquire the graphics buffer allocator
    GraphicBufferAllocator &alloc(GraphicBufferAllocator::get());

    unsigned added = 0;


    while (added < numToAdd) {
        unsigned pixelsPerLine;
        buffer_handle_t memHandle = nullptr;
        status_t result = alloc.allocate(mVideo.getWidth(), mVideo.getHeight(),
                                         mFormat, 1,
                                         mUsage,
                                         &memHandle, &pixelsPerLine, 0, "EvsV4lCamera");
        if (result != NO_ERROR) {
            LOG(ERROR) << "Error " << result << " allocating "
                       << mVideo.getWidth() << " x " << mVideo.getHeight()
                       << " graphics buffer";
            break;
        }
        if (!memHandle) {
            LOG(ERROR) << "We didn't get a buffer handle back from the allocator";
            break;
        }
        if (mStride) {
            if (mStride != pixelsPerLine) {
                LOG(ERROR) << "We did not expect to get buffers with different strides!";
            }
        } else {
            // Gralloc defines stride in terms of pixels per line
            mStride = pixelsPerLine;
        }

        // Find a place to store the new buffer
        bool stored = false;
        for (auto&& rec : mBuffers) {
            if (rec.handle == nullptr) {
                // Use this existing entry
                rec.handle = memHandle;
                rec.inUse = false;
                stored = true;
                break;
            }
        }
        if (!stored) {
            // Add a BufferRecord wrapping this handle to our set of available buffers
            mBuffers.emplace_back(memHandle);
        }

        mFramesAllowed++;
        added++;
    }

    return added;
}


unsigned EvsV4lCamera::decreaseAvailableFrames_Locked(unsigned numToRemove) {
    // Acquire the graphics buffer allocator
    GraphicBufferAllocator &alloc(GraphicBufferAllocator::get());

    unsigned removed = 0;

    for (auto&& rec : mBuffers) {
        // Is this record not in use, but holding a buffer that we can free?
        if ((rec.inUse == false) && (rec.handle != nullptr)) {
            // Release buffer and update the record so we can recognize it as "empty"
            alloc.free(rec.handle);
            rec.handle = nullptr;

            mFramesAllowed--;
            removed++;

            if (removed == numToRemove) {
                break;
            }
        }
    }

    return removed;
}


// This is the async callback from the video camera that tells us a frame is ready
void EvsV4lCamera::forwardFrame(imageBuffer* pV4lBuff, void* pData) {
    bool readyForFrame = false;
    size_t idx = 0;

    // Lock scope for updating shared state
    {
        std::lock_guard<std::mutex> lock(mAccessLock);

        // Are we allowed to issue another buffer?
        if (mFramesInUse >= mFramesAllowed) {
            // Can't do anything right now -- skip this frame
            LOG(WARNING) << "Skipped a frame because too many are in flight";
        } else {
            // Identify an available buffer to fill
            for (idx = 0; idx < mBuffers.size(); idx++) {
                if (!mBuffers[idx].inUse) {
                    if (mBuffers[idx].handle != nullptr) {
                        // Found an available record, so stop looking
                        break;
                    }
                }
            }
            if (idx >= mBuffers.size()) {
                // This shouldn't happen since we already checked mFramesInUse vs mFramesAllowed
                LOG(ERROR) << "Failed to find an available buffer slot";
            } else {
                // We're going to make the frame busy
                mBuffers[idx].inUse = true;
                mFramesInUse++;
                readyForFrame = true;
            }
        }
    }

    if (!readyForFrame) {
        // We need to return the video buffer so it can capture a new frame
        mVideo.markFrameConsumed(pV4lBuff->index);
    } else {
        // Assemble the buffer description we'll transmit below
        BufferDesc_1_1 bufDesc_1_1 = {};
        AHardwareBuffer_Desc* pDesc =
            reinterpret_cast<AHardwareBuffer_Desc *>(&bufDesc_1_1.buffer.description);
        pDesc->width  = mVideo.getWidth();
        pDesc->height = mVideo.getHeight();
        pDesc->layers = 1;
        pDesc->format = mFormat;
        pDesc->usage  = mUsage;
        pDesc->stride = mStride;
        bufDesc_1_1.buffer.nativeHandle = mBuffers[idx].handle;
        bufDesc_1_1.bufferId = idx;
        bufDesc_1_1.deviceId = mDescription.v1.cameraId;
        // timestamp in microseconds.
        bufDesc_1_1.timestamp =
            pV4lBuff->timestamp.tv_sec * 1e+6 + pV4lBuff->timestamp.tv_usec;

        // Lock our output buffer for writing
        // TODO(b/145459970): Sometimes, physical camera device maps a buffer
        // into the address that is about to be unmapped by another device; this
        // causes SEGV_MAPPER.
        void *targetPixels = nullptr;
        GraphicBufferMapper &mapper = GraphicBufferMapper::get();
        status_t result =
            mapper.lock(bufDesc_1_1.buffer.nativeHandle,
                        GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_NEVER,
                        android::Rect(pDesc->width, pDesc->height),
                        (void **)&targetPixels);

        // If we failed to lock the pixel buffer, we're about to crash, but log it first
        if (!targetPixels) {
            // TODO(b/145457727): When EvsHidlTest::CameraToDisplayRoundTrip
            // test case was repeatedly executed, EVS occasionally fails to map
            // a buffer.
            LOG(ERROR) << "Camera failed to gain access to image buffer for writing - "
                       << " status: " << statusToString(result)
                       << " , error: " << strerror(errno);
        }

        // Transfer the video image into the output buffer, making any needed
        // format conversion along the way
        mFillBufferFromVideo(bufDesc_1_1, (uint8_t *)targetPixels, pData, mVideo.getStride());

        // Unlock the output buffer
        mapper.unlock(bufDesc_1_1.buffer.nativeHandle);

        // Give the video frame back to the underlying device for reuse
        // Note that we do this before making the client callback to give the
        // underlying camera more time to capture the next frame
        mVideo.markFrameConsumed(pV4lBuff->index);

        // Issue the (asynchronous) callback to the client -- can't be holding
        // the lock
        bool flag = false;
        if (mStream_1_1 != nullptr) {
            hidl_vec<BufferDesc_1_1> frames;
            frames.resize(1);
            frames[0] = bufDesc_1_1;
            auto result = mStream_1_1->deliverFrame_1_1(frames);
            flag = result.isOk();
        } else {
            BufferDesc_1_0 bufDesc_1_0 = {
                pDesc->width,
                pDesc->height,
                pDesc->stride,
                bufDesc_1_1.pixelSize,
                static_cast<uint32_t>(pDesc->format),
                static_cast<uint32_t>(pDesc->usage),
                bufDesc_1_1.bufferId,
                bufDesc_1_1.buffer.nativeHandle
            };

            auto result = mStream->deliverFrame(bufDesc_1_0);
            flag = result.isOk();
        }

        if (flag) {
            LOG(DEBUG) << "Delivered " << bufDesc_1_1.buffer.nativeHandle.getNativeHandle()
                       << " as id " << bufDesc_1_1.bufferId;
        } else {
            // This can happen if the client dies and is likely unrecoverable.
            // To avoid consuming resources generating failing calls, we stop sending
            // frames.  Note, however, that the stream remains in the "STREAMING" state
            // until cleaned up on the main thread.
            LOG(ERROR) << "Frame delivery call failed in the transport layer.";

            // Since we didn't actually deliver it, mark the frame as available
            std::lock_guard<std::mutex> lock(mAccessLock);
            mBuffers[idx].inUse = false;

            mFramesInUse--;
        }
    }
}


bool EvsV4lCamera::convertToV4l2CID(CameraParam id, uint32_t& v4l2cid) {
    switch (id) {
        case CameraParam::BRIGHTNESS:
            v4l2cid = V4L2_CID_BRIGHTNESS;
            break;
        case CameraParam::CONTRAST:
            v4l2cid = V4L2_CID_CONTRAST;
            break;
        case CameraParam::AUTO_WHITE_BALANCE:
            v4l2cid = V4L2_CID_AUTO_WHITE_BALANCE;
            break;
        case CameraParam::WHITE_BALANCE_TEMPERATURE:
            v4l2cid = V4L2_CID_WHITE_BALANCE_TEMPERATURE;
            break;
        case CameraParam::SHARPNESS:
            v4l2cid = V4L2_CID_SHARPNESS;
            break;
        case CameraParam::AUTO_EXPOSURE:
            v4l2cid = V4L2_CID_EXPOSURE_AUTO;
            break;
        case CameraParam::ABSOLUTE_EXPOSURE:
            v4l2cid = V4L2_CID_EXPOSURE_ABSOLUTE;
            break;
        case CameraParam::AUTO_FOCUS:
            v4l2cid = V4L2_CID_FOCUS_AUTO;
            break;
        case CameraParam::ABSOLUTE_FOCUS:
            v4l2cid = V4L2_CID_FOCUS_ABSOLUTE;
            break;
        case CameraParam::ABSOLUTE_ZOOM:
            v4l2cid = V4L2_CID_ZOOM_ABSOLUTE;
            break;
        default:
            LOG(ERROR) << "Camera parameter " << static_cast<unsigned>(id) << " is unknown.";
            return false;
    }

    return mCameraControls.find(v4l2cid) != mCameraControls.end();
}


sp<EvsV4lCamera> EvsV4lCamera::Create(const char *deviceName) {
    unique_ptr<ConfigManager::CameraInfo> nullCamInfo = nullptr;

    return Create(deviceName, nullCamInfo);
}


sp<EvsV4lCamera> EvsV4lCamera::Create(const char *deviceName,
                                      unique_ptr<ConfigManager::CameraInfo> &camInfo,
                                      const Stream *requestedStreamCfg) {
    LOG(INFO) << "Create " << deviceName;
    sp<EvsV4lCamera> evsCamera = new EvsV4lCamera(deviceName, camInfo);
    if (evsCamera == nullptr) {
        return nullptr;
    }

    // Initialize the video device
    bool success = false;
    if (camInfo != nullptr && requestedStreamCfg != nullptr) {
        // Validate a given stream configuration.  If there is no exact match,
        // this will try to find the best match based on:
        // 1) same output format
        // 2) the largest resolution that is smaller that a given configuration.
        int32_t streamId = -1, area = INT_MIN;
        for (auto& [id, cfg] : camInfo->streamConfigurations) {
            // RawConfiguration has id, width, height, format, direction, and
            // fps.
            if (cfg[3] == static_cast<uint32_t>(requestedStreamCfg->format)) {
                if (cfg[1] == requestedStreamCfg->width &&
                    cfg[2] == requestedStreamCfg->height) {
                    // Find exact match.
                    streamId = id;
                    break;
                } else if (requestedStreamCfg->width  > cfg[1] &&
                           requestedStreamCfg->height > cfg[2] &&
                           cfg[1] * cfg[2] > area) {
                    streamId = id;
                    area = cfg[1] * cfg[2];
                }
            }

        }

        if (streamId >= 0) {
            LOG(INFO) << "Try to open a video with "
                      << "width: " << camInfo->streamConfigurations[streamId][1]
                      << ", height: " << camInfo->streamConfigurations[streamId][2]
                      << ", format: " << camInfo->streamConfigurations[streamId][3];
            success =
                evsCamera->mVideo.open(deviceName,
                                       camInfo->streamConfigurations[streamId][1],
                                       camInfo->streamConfigurations[streamId][2]);
            evsCamera->mFormat = static_cast<uint32_t>(camInfo->streamConfigurations[streamId][3]);
        }
    }

    if (!success) {
        // Create a camera object with the default resolution and format
        // , HAL_PIXEL_FORMAT_RGBA_8888.
        LOG(INFO) << "Open a video with default parameters";
        success =
            evsCamera->mVideo.open(deviceName, kDefaultResolution[0], kDefaultResolution[1]);
        if (!success) {
            LOG(ERROR) << "Failed to open a video stream";
            return nullptr;
        }
    }

    // List available camera parameters
    evsCamera->mCameraControls = evsCamera->mVideo.enumerateCameraControls();

    // Please note that the buffer usage flag does not come from a given stream
    // configuration.
    evsCamera->mUsage  = GRALLOC_USAGE_HW_TEXTURE     |
                         GRALLOC_USAGE_SW_READ_RARELY |
                         GRALLOC_USAGE_SW_WRITE_OFTEN;

    return evsCamera;
}


} // namespace implementation
} // namespace V1_1
} // namespace evs
} // namespace automotive
} // namespace hardware
} // namespace android
