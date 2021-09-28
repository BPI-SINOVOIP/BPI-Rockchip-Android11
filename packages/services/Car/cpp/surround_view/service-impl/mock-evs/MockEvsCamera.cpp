/*
 * Copyright 2020 The Android Open Source Project
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

#include "MockEvsCamera.h"

#include <stdlib.h>

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

// TODO(b/159733690): the number should come from xml
const int kFramesCount = 4;
const int kFrameGenerationDelayMillis = 30;

MockEvsCamera::MockEvsCamera(const string& cameraId, const Stream& streamCfg) {
    mConfigManager = ConfigManager::Create();

    mStreamCfg.height = streamCfg.height;
    mStreamCfg.width = streamCfg.width;

    mCameraDesc.v1.cameraId = cameraId;
    unique_ptr<ConfigManager::CameraGroupInfo>& cameraGroupInfo =
            mConfigManager->getCameraGroupInfo(mCameraDesc.v1.cameraId);
    if (cameraGroupInfo != nullptr) {
        mCameraDesc.metadata.setToExternal(
                (uint8_t*)cameraGroupInfo->characteristics,
                get_camera_metadata_size(cameraGroupInfo->characteristics));
    }
}

Return<void> MockEvsCamera::getCameraInfo(getCameraInfo_cb _hidl_cb) {
    // Not implemented.

    (void)_hidl_cb;
    return {};
}

Return<EvsResult> MockEvsCamera::setMaxFramesInFlight(uint32_t bufferCount) {
    // Not implemented.

    (void)bufferCount;
    return EvsResult::OK;
}

Return<EvsResult> MockEvsCamera::startVideoStream(
        const ::android::sp<IEvsCameraStream_1_0>& stream) {
    LOG(INFO) << __FUNCTION__;
    scoped_lock<mutex> lock(mAccessLock);

    mStream = IEvsCameraStream_1_1::castFrom(stream).withDefault(nullptr);

    if (mStreamState != STOPPED) {
        LOG(ERROR) << "Ignoring startVideoStream call when a stream is "
                   << "already running.";
        return EvsResult::STREAM_ALREADY_RUNNING;
    }

    // Start the frame generation thread
    mStreamState = RUNNING;
    mCaptureThread = thread([this]() { generateFrames(); });

    return EvsResult::OK;
}

Return<void> MockEvsCamera::doneWithFrame(const BufferDesc_1_0& buffer) {
    // Not implemented.

    (void)buffer;
    return {};
}

Return<void> MockEvsCamera::stopVideoStream() {
    LOG(INFO) << __FUNCTION__;

    unique_lock<mutex> lock(mAccessLock);
    if (mStreamState == RUNNING) {
        // Tell the GenerateFrames loop we want it to stop
        mStreamState = STOPPING;
        // Block outside the mutex until the "stop" flag has been acknowledged
        // We won't send any more frames, but the client might still get some
        // already in flight
        LOG(DEBUG) << __FUNCTION__ << ": Waiting for stream thread to end...";
        lock.unlock();
        mCaptureThread.join();
        lock.lock();
        mStreamState = STOPPED;
        mStream = nullptr;
        LOG(DEBUG) << "Stream marked STOPPED.";
    }
    return {};
}

Return<int32_t> MockEvsCamera::getExtendedInfo(uint32_t opaqueIdentifier) {
    // Not implemented.

    (void)opaqueIdentifier;
    return 0;
}

Return<EvsResult> MockEvsCamera::setExtendedInfo(uint32_t opaqueIdentifier,
                                                 int32_t opaqueValue) {
    // Not implemented.

    (void)opaqueIdentifier;
    (void)opaqueValue;
    return EvsResult::OK;
}

Return<void> MockEvsCamera::getCameraInfo_1_1(getCameraInfo_1_1_cb _hidl_cb) {
    _hidl_cb(mCameraDesc);
    return {};
}

Return<void> MockEvsCamera::getPhysicalCameraInfo(
        const hidl_string& deviceId, getPhysicalCameraInfo_cb _hidl_cb) {
    CameraDesc_1_1 desc = {};
    desc.v1.cameraId = deviceId;

    unique_ptr<ConfigManager::CameraInfo>& cameraInfo =
            mConfigManager->getCameraInfo(deviceId);
    if (cameraInfo != nullptr) {
        desc.metadata.setToExternal(
                (uint8_t*)cameraInfo->characteristics,
                get_camera_metadata_size(cameraInfo->characteristics));
    }

    _hidl_cb(desc);

    return {};
}

Return<EvsResult> MockEvsCamera::doneWithFrame_1_1(
        const hardware::hidl_vec<BufferDesc_1_1>& buffer) {
    // Not implemented.

    (void)buffer;
    return EvsResult::OK;
}

Return<EvsResult> MockEvsCamera::setMaster() {
    // Not implemented.

    return EvsResult::OK;
}

Return<EvsResult> MockEvsCamera::forceMaster(
        const sp<IEvsDisplay_1_0>& display) {
    // Not implemented.

    (void)display;
    return EvsResult::OK;
}

Return<EvsResult> MockEvsCamera::unsetMaster() {
    // Not implemented.

    return EvsResult::OK;
}

Return<void> MockEvsCamera::getParameterList(getParameterList_cb _hidl_cb) {
    // Not implemented.

    (void)_hidl_cb;
    return {};
}

Return<void> MockEvsCamera::getIntParameterRange(
        CameraParam id, getIntParameterRange_cb _hidl_cb) {
    // Not implemented.

    (void)id;
    (void)_hidl_cb;
    return {};
}

Return<void> MockEvsCamera::setIntParameter(CameraParam id, int32_t value,
                                            setIntParameter_cb _hidl_cb) {
    // Not implemented.

    (void)id;
    (void)value;
    (void)_hidl_cb;
    return {};
}

Return<void> MockEvsCamera::getIntParameter(
        CameraParam id, getIntParameter_cb _hidl_cb) {
    // Not implemented.

    (void)id;
    (void)_hidl_cb;
    return {};
}

Return<EvsResult> MockEvsCamera::setExtendedInfo_1_1(
    uint32_t opaqueIdentifier, const hidl_vec<uint8_t>& opaqueValue) {
    // Not implemented.

    (void)opaqueIdentifier;
    (void)opaqueValue;
    return EvsResult::OK;
}

Return<void> MockEvsCamera::getExtendedInfo_1_1(
        uint32_t opaqueIdentifier, getExtendedInfo_1_1_cb _hidl_cb) {
    // Not implemented.

    (void)opaqueIdentifier;
    (void)_hidl_cb;
    return {};
}

Return<void> MockEvsCamera::importExternalBuffers(
        const hidl_vec<BufferDesc_1_1>& buffers,
        importExternalBuffers_cb _hidl_cb) {
    // Not implemented.

    (void)buffers;
    (void)_hidl_cb;
    return {};
}

void MockEvsCamera::initializeFrames(int framesCount) {
    LOG(INFO) << "StreamCfg width: " << mStreamCfg.width
              << " height: " << mStreamCfg.height;

    string label = "EmptyBuffer_";
    mGraphicBuffers.resize(framesCount);
    mBufferDescs.resize(framesCount);
    for (int i = 0; i < framesCount; i++) {
        mGraphicBuffers[i] = new GraphicBuffer(mStreamCfg.width,
                                               mStreamCfg.height,
                                               HAL_PIXEL_FORMAT_RGBA_8888,
                                               1,
                                               GRALLOC_USAGE_HW_TEXTURE,
                                               label + (char)(i + 48));
        mBufferDescs[i].buffer.nativeHandle =
                mGraphicBuffers[i]->getNativeBuffer()->handle;
        AHardwareBuffer_Desc* pDesc =
                reinterpret_cast<AHardwareBuffer_Desc*>(
                        &mBufferDescs[i].buffer.description);
        pDesc->width = mStreamCfg.width;
        pDesc->height = mStreamCfg.height;
        pDesc->layers = 1;
        pDesc->usage = GRALLOC_USAGE_HW_TEXTURE;
        pDesc->stride = mGraphicBuffers[i]->getStride();
        pDesc->format = HAL_PIXEL_FORMAT_RGBA_8888;
    }
}

void MockEvsCamera::generateFrames() {
    initializeFrames(kFramesCount);

    while (true) {
        {
            scoped_lock<mutex> lock(mAccessLock);
            if (mStreamState != RUNNING) {
                // Break out of our main thread loop
                LOG(INFO) << "StreamState does not equal to RUNNING. "
                          << "Exiting the loop";
                break;
            }
        }

        mStream->deliverFrame_1_1(mBufferDescs);
        std::this_thread::sleep_for(
                std::chrono::milliseconds(kFrameGenerationDelayMillis));
    }

    {
        scoped_lock<mutex> lock(mAccessLock);

        if (mStream != nullptr) {
            LOG(DEBUG) << "Notify EvsEventType::STREAM_STOPPED";

            EvsEventDesc evsEventDesc;
            evsEventDesc.aType = EvsEventType::STREAM_STOPPED;
            mStream->notify(evsEventDesc);
        } else {
            LOG(WARNING) << "EVS stream is not valid any more. "
                         << "The notify call is ignored.";
        }
    }
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
