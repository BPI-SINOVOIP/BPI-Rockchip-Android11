/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef ANDROID_AUTOMOTIVE_EVS_V1_1_CAMERAPROXY_H
#define ANDROID_AUTOMOTIVE_EVS_V1_1_CAMERAPROXY_H

#include <android/hardware/automotive/evs/1.1/types.h>
#include <android/hardware/automotive/evs/1.1/IEvsCamera.h>
#include <android/hardware/automotive/evs/1.1/IEvsCameraStream.h>
#include <android/hardware/automotive/evs/1.1/IEvsDisplay.h>

#include <deque>
#include <set>
#include <thread>
#include <unordered_map>

#include <utils/Mutex.h>


using namespace std;
using namespace ::android::hardware::automotive::evs::V1_1;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_handle;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::automotive::evs::V1_0::EvsResult;
using ::android::hardware::automotive::evs::V1_0::IEvsDisplay;
using BufferDesc_1_0 = ::android::hardware::automotive::evs::V1_0::BufferDesc;
using BufferDesc_1_1 = ::android::hardware::automotive::evs::V1_1::BufferDesc;
using IEvsCamera_1_0 = ::android::hardware::automotive::evs::V1_0::IEvsCamera;
using IEvsCamera_1_1 = ::android::hardware::automotive::evs::V1_1::IEvsCamera;
using IEvsCameraStream_1_0 = ::android::hardware::automotive::evs::V1_0::IEvsCameraStream;
using IEvsCameraStream_1_1 = ::android::hardware::automotive::evs::V1_1::IEvsCameraStream;
using IEvsDisplay_1_0 = ::android::hardware::automotive::evs::V1_0::IEvsDisplay;
using IEvsDisplay_1_1 = ::android::hardware::automotive::evs::V1_1::IEvsDisplay;

namespace android {
namespace automotive {
namespace evs {
namespace V1_1 {
namespace implementation {


class HalCamera;        // From HalCamera.h


// This class represents an EVS camera to the client application.  As such it presents
// the IEvsCamera interface, and also proxies the frame delivery to the client's
// IEvsCameraStream object.
class VirtualCamera : public IEvsCamera_1_1 {
public:
    explicit          VirtualCamera(const std::vector<sp<HalCamera>>& halCameras);
    virtual           ~VirtualCamera();

    unsigned          getAllowedBuffers() { return mFramesAllowed; };
    bool              isStreaming()       { return mStreamState == RUNNING; }
    bool              getVersion() const  { return (int)(mStream_1_1 != nullptr); }
    vector<sp<HalCamera>>
                      getHalCameras();
    void              setDescriptor(CameraDesc* desc) { mDesc = desc; }

    // Proxy to receive frames and forward them to the client's stream
    bool              notify(const EvsEventDesc& event);
    bool              deliverFrame(const BufferDesc& bufDesc);

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
    Return<EvsResult> doneWithFrame_1_1(const hardware::hidl_vec<BufferDesc_1_1>& buffer) override;
    Return<EvsResult> pauseVideoStream() override { return EvsResult::UNDERLYING_SERVICE_ERROR; }
    Return<EvsResult> resumeVideoStream() override { return EvsResult::UNDERLYING_SERVICE_ERROR; }
    Return<EvsResult> setMaster() override;
    Return<EvsResult> forceMaster(const sp<IEvsDisplay_1_0>& display) override;
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

    // Dump current status to a given file descriptor
    std::string       toString(const char* indent = "") const;


private:
    void shutdown();

    // The low level camera interface that backs this proxy
    unordered_map<string,
                 wp<HalCamera>> mHalCamera;

    sp<IEvsCameraStream_1_0>    mStream;
    sp<IEvsCameraStream_1_1>    mStream_1_1;

    unsigned                    mFramesAllowed  = 1;
    enum {
        STOPPED,
        RUNNING,
        STOPPING,
    }                           mStreamState;

    unordered_map<string,
         deque<BufferDesc_1_1>> mFramesHeld;
    thread                      mCaptureThread;
    CameraDesc*                 mDesc;

    mutable std::mutex          mFrameDeliveryMutex;
    std::condition_variable     mFramesReadySignal;
    std::set<std::string>       mSourceCameras GUARDED_BY(mFrameDeliveryMutex);

};

} // namespace implementation
} // namespace V1_1
} // namespace evs
} // namespace automotive
} // namespace android

#endif  // ANDROID_AUTOMOTIVE_EVS_V1_1_CAMERAPROXY_H
