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

#pragma once

#include <android/hardware/automotive/evs/1.1/IEvsCamera.h>
#include <android/hardware/automotive/evs/1.1/IEvsCameraStream.h>
#include <android/hardware/automotive/evs/1.1/IEvsDisplay.h>
#include <android/hardware/automotive/evs/1.1/IEvsEnumerator.h>

#include <ConfigManager.h>

#include <ui/GraphicBuffer.h>

#include <mutex>
#include <thread>

using ::android::hardware::automotive::evs::V1_0::EvsResult;
using ::android::hardware::automotive::evs::V1_1::CameraParam;
using ::android::hardware::automotive::evs::V1_1::EvsEventDesc;
using ::android::hardware::automotive::evs::V1_1::EvsEventType;

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

using BufferDesc_1_0 = ::android::hardware::automotive::evs::V1_0::BufferDesc;
using BufferDesc_1_1 = ::android::hardware::automotive::evs::V1_1::BufferDesc;
using CameraDesc_1_1 = ::android::hardware::automotive::evs::V1_1::CameraDesc;
using IEvsCamera_1_0 = ::android::hardware::automotive::evs::V1_0::IEvsCamera;
using IEvsCamera_1_1 = ::android::hardware::automotive::evs::V1_1::IEvsCamera;
using IEvsCameraStream_1_0 = ::android::hardware::automotive::evs::V1_0::IEvsCameraStream;
using IEvsCameraStream_1_1 = ::android::hardware::automotive::evs::V1_1::IEvsCameraStream;
using IEvsDisplay_1_0 = ::android::hardware::automotive::evs::V1_0::IEvsDisplay;
using IEvsEnumerator_1_1 = ::android::hardware::automotive::evs::V1_1::IEvsEnumerator;

// A simplified implementation for Evs Camera. Only necessary methods are
// implemented.
class MockEvsCamera : public IEvsCamera_1_1 {
public:
    MockEvsCamera(const std::string& cameraId, const Stream& streamCfg);

    // Methods from ::android::hardware::automotive::evs::V1_0::IEvsCamera follow.
    Return<void> getCameraInfo(getCameraInfo_cb _hidl_cb) override;
    Return<EvsResult> setMaxFramesInFlight(uint32_t bufferCount) override;
    Return<EvsResult> startVideoStream(const ::android::sp<IEvsCameraStream_1_0>& stream) override;
    Return<void> doneWithFrame(const BufferDesc_1_0& buffer) override;
    Return<void> stopVideoStream() override;
    Return<int32_t> getExtendedInfo(uint32_t opaqueIdentifier) override;
    Return<EvsResult> setExtendedInfo(uint32_t opaqueIdentifier, int32_t opaqueValue) override;

    // Methods from ::android::hardware::automotive::evs::V1_1::IEvsCamera follow.
    Return<void> getCameraInfo_1_1(getCameraInfo_1_1_cb _hidl_cb) override;
    Return<void> getPhysicalCameraInfo(const hidl_string& deviceId,
                                       getPhysicalCameraInfo_cb _hidl_cb) override;
    Return<EvsResult> doneWithFrame_1_1(const hardware::hidl_vec<BufferDesc_1_1>& buffer) override;
    Return<EvsResult> pauseVideoStream() override { return EvsResult::UNDERLYING_SERVICE_ERROR; }
    Return<EvsResult> resumeVideoStream() override { return EvsResult::UNDERLYING_SERVICE_ERROR; }
    Return<EvsResult> setMaster() override;
    Return<EvsResult> forceMaster(const sp<IEvsDisplay_1_0>& display) override;
    Return<EvsResult> unsetMaster() override;
    Return<void> getParameterList(getParameterList_cb _hidl_cb) override;
    Return<void> getIntParameterRange(CameraParam id, getIntParameterRange_cb _hidl_cb) override;
    Return<void> setIntParameter(CameraParam id, int32_t value,
                                 setIntParameter_cb _hidl_cb) override;
    Return<void> getIntParameter(CameraParam id, getIntParameter_cb _hidl_cb) override;
    Return<EvsResult> setExtendedInfo_1_1(uint32_t opaqueIdentifier,
                                          const hidl_vec<uint8_t>& opaqueValue) override;
    Return<void> getExtendedInfo_1_1(uint32_t opaqueIdentifier,
                                     getExtendedInfo_1_1_cb _hidl_cb) override;
    Return<void> importExternalBuffers(const hidl_vec<BufferDesc_1_1>& buffers,
                                       importExternalBuffers_cb _hidl_cb) override;

private:
    void initializeFrames(int framesCount);
    void generateFrames();

    std::unique_ptr<ConfigManager> mConfigManager;

    std::mutex mAccessLock;

    enum StreamStateValues {
        STOPPED,
        RUNNING,
        STOPPING,
        DEAD,
    };
    StreamStateValues mStreamState GUARDED_BY(mAccessLock);
    Stream mStreamCfg;

    std::vector<android::sp<GraphicBuffer>> mGraphicBuffers;
    std::vector<BufferDesc_1_1> mBufferDescs;
    CameraDesc_1_1 mCameraDesc;

    std::string mCameraId;
    std::thread mCaptureThread;  // The thread we'll use to synthesize frames

    android::sp<IEvsCameraStream_1_1> mStream;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
