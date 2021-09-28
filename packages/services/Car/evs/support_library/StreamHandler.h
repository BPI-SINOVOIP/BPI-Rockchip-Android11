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

#include <condition_variable>
#include <queue>
#include <thread>
#include <shared_mutex>
#include <ui/GraphicBuffer.h>
#include <android/hardware/automotive/evs/1.0/IEvsCameraStream.h>
#include <android/hardware/automotive/evs/1.0/IEvsCamera.h>
#include <android/hardware/automotive/evs/1.0/IEvsDisplay.h>

#include "BaseRenderCallback.h"
#include "BaseAnalyzeCallback.h"

namespace android {
namespace automotive {
namespace evs {
namespace support {

using namespace ::android::hardware::automotive::evs::V1_0;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_handle;
using ::android::sp;


/*
 * StreamHandler:
 * This class can be used to receive camera imagery from an IEvsCamera implementation.  It will
 * hold onto the most recent image buffer, returning older ones.
 * Note that the video frames are delivered on a background thread, while the control interface
 * is actuated from the applications foreground thread.
 */
class StreamHandler : public IEvsCameraStream {
public:
    virtual ~StreamHandler() {
        // The shutdown logic is supposed to be handled by ResourceManager
        // class. But if something goes wrong, we want to make sure that the
        // related resources are still released properly.
        if (mCamera != nullptr) {
            shutdown();
        }
    };

    StreamHandler(android::sp <IEvsCamera> pCamera);
    void shutdown();

    bool startStream();

    bool newDisplayFrameAvailable();
    const BufferDesc& getNewDisplayFrame();
    void doneWithFrame(const BufferDesc& buffer);

    /*
     * Attaches a render callback to the StreamHandler.
     *
     * Every frame will be processed by the attached render callback before it
     * is delivered to the client by method getNewDisplayFrame().
     *
     * Since there is only one DisplayUseCase allowed at the same time, at most
     * only one render callback can be attached. The current render callback
     * needs to be detached first (by method detachRenderCallback()), before a
     * new callback can be attached. In other words, the call will be ignored
     * if the current render callback is not null.
     *
     * @see detachRenderCallback()
     * @see getNewDisplayFrame()
     */
    void attachRenderCallback(BaseRenderCallback*);

    /*
     * Detaches the current render callback.
     *
     * If no render callback is attached, this call will be ignored.
     *
     * @see attachRenderCallback(BaseRenderCallback*)
     */
    void detachRenderCallback();

    /*
     * Attaches an analyze callback to the StreamHandler.
     *
     * When there is a valid analyze callback attached, a thread dedicated for
     * the analyze callback will be allocated. When the thread is not busy, the
     * next available evs frame will be copied (now happens in binder thread).
     * And the copy will be passed into the analyze thread, and be processed by
     * the analyze callback.
     *
     * Since there is only one AnalyzeUseCase allowed at the same time, at most
     * only one analyze callback can be attached. The current analyze callback
     * needs to be detached first (by method detachAnalyzeCallback()), before a
     * new callback can be attached. In other words, the call will be ignored
     * if the current analyze callback is not null.
     *
     * @see detachAnalyzeCallback()
     */
    // TODO(b/130246434): now only one analyze use case is supported, so one
    // analyze thread id good enough. But we should be able to support several
    // analyze use cases running at the same time, so we should probably use a
    // thread pool to handle the cases.
    void attachAnalyzeCallback(BaseAnalyzeCallback*);

    /*
     * Detaches the current analyze callback.
     *
     * If no analyze callback is attached, this call will be ignored.
     *
     * @see attachAnalyzeCallback(BaseAnalyzeCallback*)
     */
    void detachAnalyzeCallback();

private:
    // Implementation for ::android::hardware::automotive::evs::V1_0::ICarCameraStream
    Return<void> deliverFrame(const BufferDesc& buffer)  override;

    bool processFrame(const BufferDesc&, BufferDesc&);
    bool copyAndAnalyzeFrame(const BufferDesc&);

    // Values initialized as startup
    android::sp <IEvsCamera>    mCamera;

    // Since we get frames delivered to us asnchronously via the ICarCameraStream interface,
    // we need to protect all member variables that may be modified while we're streaming
    // (ie: those below)
    std::mutex                  mLock;
    std::condition_variable     mSignal;

    bool                        mRunning = false;

    BufferDesc                  mOriginalBuffers[2];
    int                         mHeldBuffer = -1;   // Index of the one currently held by the client
    int                         mReadyBuffer = -1;  // Index of the newest available buffer

    BufferDesc                  mProcessedBuffers[2];
    BufferDesc                  mAnalyzeBuffer GUARDED_BY(mAnalyzerLock);

    BaseRenderCallback*         mRenderCallback = nullptr;

    BaseAnalyzeCallback*        mAnalyzeCallback GUARDED_BY(mAnalyzerLock);
    std::atomic<bool>           mAnalyzerRunning;
    std::shared_mutex           mAnalyzerLock;
    std::condition_variable_any mAnalyzerSignal;
};

}  // namespace support
}  // namespace evs
}  // namespace automotive
}  // namespace android

#endif //EVS_VTS_STREAMHANDLER_H

