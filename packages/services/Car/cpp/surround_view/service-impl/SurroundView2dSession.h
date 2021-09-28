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

#include "IOModule.h"

#include <android/hardware/automotive/evs/1.1/IEvsCamera.h>
#include <android/hardware/automotive/evs/1.1/IEvsCameraStream.h>
#include <android/hardware/automotive/evs/1.1/IEvsEnumerator.h>
#include <android/hardware/automotive/sv/1.0/types.h>
#include <android/hardware/automotive/sv/1.0/ISurroundViewStream.h>
#include <android/hardware/automotive/sv/1.0/ISurroundView2dSession.h>

#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

#include <ui/GraphicBuffer.h>

#include <thread>

using namespace ::android::hardware::automotive::evs::V1_1;
using namespace ::android::hardware::automotive::sv::V1_0;
using namespace ::android_auto::surround_view;

using ::android::hardware::Return;
using ::android::hardware::hidl_vec;
using ::android::sp;
using ::std::condition_variable;

using BufferDesc_1_0  = ::android::hardware::automotive::evs::V1_0::BufferDesc;
using BufferDesc_1_1  = ::android::hardware::automotive::evs::V1_1::BufferDesc;

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

class SurroundView2dSession : public ISurroundView2dSession {

    /*
     * FramesHandler:
     * This class can be used to receive camera imagery from an IEvsCamera implementation.  It will
     * hold onto the most recent image buffer, returning older ones.
     * Note that the video frames are delivered on a background thread, while the control interface
     * is actuated from the applications foreground thread.
     */
    class FramesHandler : public IEvsCameraStream {
    public:
        FramesHandler(sp<IEvsCamera> pCamera, sp<SurroundView2dSession> pSession);

    private:
        // Implementation for ::android::hardware::automotive::evs::V1_0::IEvsCameraStream
        Return<void> deliverFrame(const BufferDesc_1_0& buffer) override;

        // Implementation for ::android::hardware::automotive::evs::V1_1::IEvsCameraStream
        Return<void> deliverFrame_1_1(const hidl_vec<BufferDesc_1_1>& buffer) override;
        Return<void> notify(const EvsEventDesc& event) override;

        // Values initialized as startup
        sp <IEvsCamera> mCamera;

        sp<SurroundView2dSession> mSession;
    };

public:
    SurroundView2dSession(sp<IEvsEnumerator> pEvs, IOModuleConfig* pConfig);
    ~SurroundView2dSession();
    bool initialize();

    // Methods from ::android::hardware::automotive::sv::V1_0::ISurroundViewSession.
    Return<SvResult> startStream(
        const sp<ISurroundViewStream>& stream) override;
    Return<void> stopStream() override;
    Return<void> doneWithFrames(const SvFramesDesc& svFramesDesc) override;

    // Methods from ISurroundView2dSession follow.
    Return<void> get2dMappingInfo(get2dMappingInfo_cb _hidl_cb) override;
    Return<SvResult> set2dConfig(const Sv2dConfig& sv2dConfig) override;
    Return<void> get2dConfig(get2dConfig_cb _hidl_cb) override;
    Return<void> projectCameraPoints(
        const hidl_vec<Point2dInt>& points2dCamera,
        const hidl_string& cameraId,
        projectCameraPoints_cb _hidl_cb) override;

private:
    void processFrames();

    // Set up and open the Evs camera(s), triggered when session is created.
    bool setupEvs();

    // Start Evs camera video stream, triggered when SV stream is started.
    bool startEvs();

    bool handleFrames(int sequenceId);

    bool copyFromBufferToPointers(BufferDesc_1_1 buffer,
                                  SurroundViewInputBufferPointers pointers);

    enum StreamStateValues {
        STOPPED,
        RUNNING,
        STOPPING,
        DEAD,
    };

    // EVS Enumerator to control the start/stop of the Evs Stream
    sp<IEvsEnumerator> mEvs;

    IOModuleConfig* mIOModuleConfig;

    // Instance and metadata for the opened Evs Camera
    sp<IEvsCamera> mCamera;
    CameraDesc mCameraDesc;
    std::vector<SurroundViewCameraParams> mCameraParams;

    // Stream subscribed for the session.
    sp<ISurroundViewStream> mStream GUARDED_BY(mAccessLock);
    StreamStateValues mStreamState GUARDED_BY(mAccessLock);

    std::thread mProcessThread; // The thread we'll use to process frames

    // Reference to the inner class, to handle the incoming Evs frames
    sp<FramesHandler> mFramesHandler;

    // Used to signal a set of frames is ready
    condition_variable mFramesSignal GUARDED_BY(mAccessLock);
    bool mProcessingEvsFrames GUARDED_BY(mAccessLock);

    int mSequenceId;

    struct FramesRecord {
        SvFramesDesc frames;
        bool inUse = false;
    };

    FramesRecord mFramesRecord GUARDED_BY(mAccessLock);

    // Synchronization necessary to deconflict mCaptureThread from the main
    // service thread
    std::mutex mAccessLock;

    std::vector<std::string> mEvsCameraIds GUARDED_BY(mAccessLock);

    std::unique_ptr<SurroundView> mSurroundView GUARDED_BY(mAccessLock);

    std::vector<SurroundViewInputBufferPointers>
        mInputPointers GUARDED_BY(mAccessLock);
    SurroundViewResultPointer mOutputPointer GUARDED_BY(mAccessLock);

    Sv2dConfig mConfig GUARDED_BY(mAccessLock);
    int mHeight GUARDED_BY(mAccessLock);

    // TODO(b/158479099): Rename it to mMappingInfo
    Sv2dMappingInfo mInfo GUARDED_BY(mAccessLock);
    int mOutputWidth, mOutputHeight GUARDED_BY(mAccessLock);
    sp<GraphicBuffer> mOutputHolder;

    sp<GraphicBuffer> mSvTexture GUARDED_BY(mAccessLock);

    bool mIsInitialized GUARDED_BY(mAccessLock) = false;

    bool mGpuAccelerationEnabled;
    hidl_vec<BufferDesc_1_1> mEvsGraphicBuffers;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android

