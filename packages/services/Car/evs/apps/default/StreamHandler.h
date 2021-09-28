/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef EVS_VTS_STREAMHANDLER_H
#define EVS_VTS_STREAMHANDLER_H

#include <queue>

#include "ui/GraphicBuffer.h"

#include <android/hardware/automotive/evs/1.1/IEvsCameraStream.h>
#include <android/hardware/automotive/evs/1.1/IEvsCamera.h>
#include <android/hardware/automotive/evs/1.1/IEvsDisplay.h>

using namespace ::android::hardware::automotive::evs::V1_1;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_handle;
using ::android::sp;
using ::android::hardware::automotive::evs::V1_1::IEvsDisplay;
using EvsDisplayState = ::android::hardware::automotive::evs::V1_0::DisplayState;
using BufferDesc_1_0  = ::android::hardware::automotive::evs::V1_0::BufferDesc;
using BufferDesc_1_1  = ::android::hardware::automotive::evs::V1_1::BufferDesc;


/*
 * StreamHandler:
 * This class can be used to receive camera imagery from an IEvsCamera implementation.  It will
 * hold onto the most recent image buffer, returning older ones.
 * Note that the video frames are delivered on a background thread, while the control interface
 * is actuated from the applications foreground thread.
 */
class StreamHandler : public IEvsCameraStream {
public:
    virtual ~StreamHandler() { shutdown(); };

    StreamHandler(android::sp <IEvsCamera> pCamera,
                  uint32_t numBuffers = 2,
                  bool useOwnBuffers = false,
                  android_pixel_format_t format = HAL_PIXEL_FORMAT_RGBA_8888,
                  int32_t width = 640,
                  int32_t height = 360);
    void shutdown();

    bool startStream();
    void asyncStopStream();
    void blockingStopStream();

    bool isRunning();

    bool newFrameAvailable();
    const BufferDesc_1_1& getNewFrame();
    void doneWithFrame(const BufferDesc_1_1& buffer);

private:
    // Implementation for ::android::hardware::automotive::evs::V1_0::IEvsCameraStream
    Return<void> deliverFrame(const BufferDesc_1_0& buffer)  override;

    // Implementation for ::android::hardware::automotive::evs::V1_1::IEvsCameraStream
    Return<void> deliverFrame_1_1(const hidl_vec<BufferDesc_1_1>& buffer)  override;
    Return<void> notify(const EvsEventDesc& event) override;

    // Values initialized as startup
    android::sp <IEvsCamera>    mCamera;

    // Since we get frames delivered to us asnchronously via the ICarCameraStream interface,
    // we need to protect all member variables that may be modified while we're streaming
    // (ie: those below)
    std::mutex                  mLock;
    std::condition_variable     mSignal;

    bool                        mRunning = false;

    BufferDesc                  mBuffers[2];
    int                         mHeldBuffer = -1;   // Index of the one currently held by the client
    int                         mReadyBuffer = -1;  // Index of the newest available buffer
    hidl_vec<BufferDesc_1_1>    mOwnBuffers;
    bool                        mUseOwnBuffers;
};


#endif //EVS_VTS_STREAMHANDLER_H
