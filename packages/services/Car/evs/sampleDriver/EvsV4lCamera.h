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

#ifndef ANDROID_HARDWARE_AUTOMOTIVE_EVS_V1_1_EVSV4LCAMERA_H
#define ANDROID_HARDWARE_AUTOMOTIVE_EVS_V1_1_EVSV4LCAMERA_H

#include <android/hardware/automotive/evs/1.1/types.h>
#include <android/hardware/automotive/evs/1.1/IEvsCamera.h>
#include <android/hardware/automotive/evs/1.1/IEvsCameraStream.h>
#include <android/hardware/automotive/evs/1.1/IEvsDisplay.h>
#include <android/hardware/camera/device/3.2/ICameraDevice.h>
#include <ui/GraphicBuffer.h>

#include <functional>
#include <thread>
#include <set>

#include "VideoCapture.h"
#include "ConfigManager.h"

using ::android::hardware::hidl_string;
using ::android::hardware::camera::device::V3_2::Stream;
using ::android::hardware::automotive::evs::V1_0::EvsResult;
using ::android::hardware::automotive::evs::V1_0::CameraDesc;
using IEvsDisplay_1_0      = ::android::hardware::automotive::evs::V1_0::IEvsDisplay;
using IEvsDisplay_1_1      = ::android::hardware::automotive::evs::V1_1::IEvsDisplay;
using BufferDesc_1_0       = ::android::hardware::automotive::evs::V1_0::BufferDesc;
using BufferDesc_1_1       = ::android::hardware::automotive::evs::V1_1::BufferDesc;
using IEvsCameraStream_1_0 = ::android::hardware::automotive::evs::V1_0::IEvsCameraStream;
using IEvsCameraStream_1_1 = ::android::hardware::automotive::evs::V1_1::IEvsCameraStream;

namespace android {
namespace hardware {
namespace automotive {
namespace evs {
namespace V1_1 {
namespace implementation {


// From EvsEnumerator.h
class EvsEnumerator;


class EvsV4lCamera : public IEvsCamera {
public:
    // Methods from ::android::hardware::automotive::evs::V1_0::IEvsCamera follow.
    Return<void>      getCameraInfo(getCameraInfo_cb _hidl_cb)  override;
    Return<EvsResult> setMaxFramesInFlight(uint32_t bufferCount) override;
    Return<EvsResult> startVideoStream(const ::android::sp<IEvsCameraStream_1_0>& stream) override;
    Return<void>      doneWithFrame(const BufferDesc_1_0& buffer) override;
    Return<void>      stopVideoStream() override;
    Return<int32_t>   getExtendedInfo(uint32_t opaqueIdentifier) override;
    Return<EvsResult> setExtendedInfo(uint32_t opaqueIdentifier, int32_t opaqueValue) override;

    // Methods from ::android::hardware::automotive::evs::V1_1::IEvsCamera follow.
    Return<void>      getCameraInfo_1_1(getCameraInfo_1_1_cb _hidl_cb)  override;
    Return<void>      getPhysicalCameraInfo(const hidl_string& deviceId,
                                            getPhysicalCameraInfo_cb _hidl_cb)  override;
    Return<EvsResult> pauseVideoStream() override;
    Return<EvsResult> resumeVideoStream() override;
    Return<EvsResult> doneWithFrame_1_1(const hidl_vec<BufferDesc_1_1>& buffer) override;
    Return<EvsResult> setMaster() override;
    Return<EvsResult> forceMaster(const sp<IEvsDisplay_1_0>&) override;
    Return<EvsResult> unsetMaster() override;
    Return<void>      getParameterList(getParameterList_cb _hidl_cb) override;
    Return<void>      getIntParameterRange(CameraParam id,
                                           getIntParameterRange_cb _hidl_cb) override;
    Return<void>      setIntParameter(CameraParam id, int32_t value,
                                      setIntParameter_cb _hidl_cb) override;
    Return<void>      getIntParameter(CameraParam id,
                                      getIntParameter_cb _hidl_cb) override;
    Return<EvsResult> setExtendedInfo_1_1(uint32_t opaqueIdentifier,
                                          const hidl_vec<uint8_t>& opaqueValue) override;
    Return<void>      getExtendedInfo_1_1(uint32_t opaqueIdentifier,
                                          getExtendedInfo_1_1_cb _hidl_cb) override;
    Return<void>      importExternalBuffers(const hidl_vec<BufferDesc_1_1>& buffers,
                                            importExternalBuffers_cb _hidl_cb) override;

    static sp<EvsV4lCamera> Create(const char *deviceName);
    static sp<EvsV4lCamera> Create(const char *deviceName,
                                   unique_ptr<ConfigManager::CameraInfo> &camInfo,
                                   const Stream *streamCfg = nullptr);
    EvsV4lCamera(const EvsV4lCamera&) = delete;
    EvsV4lCamera& operator=(const EvsV4lCamera&) = delete;

    virtual ~EvsV4lCamera() override;
    void shutdown();

    const CameraDesc& getDesc() { return mDescription; };

private:
    // Constructors
    EvsV4lCamera(const char *deviceName,
                 unique_ptr<ConfigManager::CameraInfo> &camInfo);

    // These three functions are expected to be called while mAccessLock is held
    bool setAvailableFrames_Locked(unsigned bufferCount);
    unsigned increaseAvailableFrames_Locked(unsigned numToAdd);
    unsigned decreaseAvailableFrames_Locked(unsigned numToRemove);

    void forwardFrame(imageBuffer* tgt, void* data);
    inline bool convertToV4l2CID(CameraParam id, uint32_t& v4l2cid);

    sp <IEvsCameraStream_1_0> mStream     = nullptr;  // The callback used to deliver each frame
    sp <IEvsCameraStream_1_1> mStream_1_1 = nullptr;  // The callback used to deliver each frame

    VideoCapture              mVideo;                 // Interface to the v4l device
    CameraDesc                mDescription = {};      // The properties of this camera

    uint32_t mFormat = 0;           // Values from android_pixel_format_t
    uint32_t mUsage  = 0;           // Values from from Gralloc.h
    uint32_t mStride = 0;           // Pixels per row (may be greater than image width)

    struct BufferRecord {
        buffer_handle_t handle;
        bool inUse;

        explicit BufferRecord(buffer_handle_t h) : handle(h), inUse(false) {};
    };

    std::vector <BufferRecord> mBuffers;    // Graphics buffers to transfer images
    unsigned mFramesAllowed;                // How many buffers are we currently using
    unsigned mFramesInUse;                  // How many buffers are currently outstanding

    std::set<uint32_t> mCameraControls;     // Available camera controls

    // Which format specific function we need to use to move camera imagery into our output buffers
    void(*mFillBufferFromVideo)(const BufferDesc& tgtBuff, uint8_t* tgt,
                                void* imgData, unsigned imgStride);


    EvsResult doneWithFrame_impl(const uint32_t id, const buffer_handle_t handle);

    // Synchronization necessary to deconflict the capture thread from the main service thread
    // Note that the service interface remains single threaded (ie: not reentrant)
    std::mutex mAccessLock;

    // Static camera module information
    unique_ptr<ConfigManager::CameraInfo> &mCameraInfo;

    // Extended information
    std::unordered_map<uint32_t, std::vector<uint8_t>> mExtInfo;
};

} // namespace implementation
} // namespace V1_1
} // namespace evs
} // namespace automotive
} // namespace hardware
} // namespace android

#endif  // ANDROID_HARDWARE_AUTOMOTIVE_EVS_V1_1_EVSV4LCAMERA_H
