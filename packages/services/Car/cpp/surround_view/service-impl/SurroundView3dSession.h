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
#include <android/hardware/automotive/evs/1.1/IEvsEnumerator.h>
#include <android/hardware/automotive/sv/1.0/types.h>
#include <android/hardware/automotive/sv/1.0/ISurroundViewStream.h>
#include <android/hardware/automotive/sv/1.0/ISurroundView3dSession.h>

#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

#include "AnimationModule.h"
#include "VhalHandler.h"

#include <thread>

#include <ui/GraphicBuffer.h>

using namespace ::android::hardware::automotive::evs::V1_1;
using namespace ::android::hardware::automotive::sv::V1_0;
using namespace ::android::hardware::automotive::vehicle::V2_0;
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

class SurroundView3dSession : public ISurroundView3dSession {

    /*
     * FramesHandler:
     * This class can be used to receive camera imagery from an IEvsCamera implementation.  It will
     * hold onto the most recent image buffer, returning older ones.
     * Note that the video frames are delivered on a background thread, while the control interface
     * is actuated from the applications foreground thread.
     */
    class FramesHandler : public IEvsCameraStream {
    public:
        FramesHandler(sp<IEvsCamera> pCamera, sp<SurroundView3dSession> pSession);

    private:
        // Implementation for ::android::hardware::automotive::evs::V1_0::IEvsCameraStream
        Return<void> deliverFrame(const BufferDesc_1_0& buffer) override;

        // Implementation for ::android::hardware::automotive::evs::V1_1::IEvsCameraStream
        Return<void> deliverFrame_1_1(const hidl_vec<BufferDesc_1_1>& buffer) override;
        Return<void> notify(const EvsEventDesc& event) override;

        // Values initialized as startup
        sp<IEvsCamera> mCamera;

        sp<SurroundView3dSession> mSession;
    };

public:
    // TODO(b/158479099): use strong pointer for VhalHandler
    SurroundView3dSession(sp<IEvsEnumerator> pEvs,
                          VhalHandler* vhalHandler,
                          AnimationModule* animationModule,
                          IOModuleConfig* pConfig);
    ~SurroundView3dSession();
    bool initialize();

    // Methods from ::android::hardware::automotive::sv::V1_0::ISurroundViewSession.
    Return<SvResult> startStream(
        const sp<ISurroundViewStream>& stream) override;
    Return<void> stopStream() override;
    Return<void> doneWithFrames(const SvFramesDesc& svFramesDesc) override;

    // Methods from ISurroundView3dSession follow.
    Return<SvResult> setViews(const hidl_vec<View3d>& views) override;
    Return<SvResult> set3dConfig(const Sv3dConfig& sv3dConfig) override;
    Return<void> get3dConfig(get3dConfig_cb _hidl_cb) override;
    Return<SvResult>  updateOverlays(const OverlaysData& overlaysData);
    Return<void> projectCameraPointsTo3dSurface(
        const hidl_vec<Point2dInt>& cameraPoints,
        const hidl_string& cameraId,
        projectCameraPointsTo3dSurface_cb _hidl_cb);

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

    // Synchronization necessary to deconflict mCaptureThread from the main service thread
    std::mutex mAccessLock;

    std::vector<View3d> mViews GUARDED_BY(mAccessLock);

    Sv3dConfig mConfig GUARDED_BY(mAccessLock);

    std::vector<std::string> mEvsCameraIds GUARDED_BY(mAccessLock);

    std::unique_ptr<SurroundView> mSurroundView GUARDED_BY(mAccessLock);

    std::vector<SurroundViewInputBufferPointers>
        mInputPointers GUARDED_BY(mAccessLock);
    SurroundViewResultPointer mOutputPointer GUARDED_BY(mAccessLock);
    int mOutputWidth, mOutputHeight GUARDED_BY(mAccessLock);

    sp<GraphicBuffer> mSvTexture GUARDED_BY(mAccessLock);

    bool mIsInitialized GUARDED_BY(mAccessLock) = false;

    VhalHandler* mVhalHandler;
    AnimationModule* mAnimationModule;
    IOModuleConfig* mIOModuleConfig;

    std::vector<Overlay> mOverlays GUARDED_BY(mAccessLock);
    bool mOverlayIsUpdated GUARDED_BY(mAccessLock) = false;

    std::vector<VehiclePropValue> mPropertyValues;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
