/*
 * Copyright (C) 2012 The Android Open Source Project
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


/**
 * This class simulates a hardware JPEG compressor.  It receives image buffers
 * in RGBA_8888 format, processes them in a worker thread, and then pushes them
 * out to their destination stream.
 */

#ifndef HW_EMULATOR_CAMERA2_JPEG_H
#define HW_EMULATOR_CAMERA2_JPEG_H

#include "utils/Thread.h"
#include "utils/Mutex.h"
#include "utils/Timers.h"

#include "Base.h"
#include "../JpegCompressor.h"
#include <CameraMetadata.h>
#include <ui/GraphicBufferMapper.h>

#include <stdio.h>

extern "C" {
#include <jpeglib.h>
}

using ::android::hardware::camera::common::V1_0::helper::CameraMetadata;

namespace android {

class JpegCompressor: private Thread, public virtual RefBase {
  public:

    JpegCompressor(GraphicBufferMapper* gbm);
    ~JpegCompressor();

    struct JpegListener {
        // Called when JPEG compression has finished, or encountered an error
        virtual void onJpegDone(const StreamBuffer &jpegBuffer,
                bool success) = 0;
        // Called when the input buffer for JPEG is not needed any more,
        // if the buffer came from the framework.
        virtual void onJpegInputDone(const StreamBuffer &inputBuffer) = 0;
        virtual ~JpegListener();
    };

    // Start compressing COMPRESSED format buffers; JpegCompressor takes
    // ownership of the Buffers vector.
    // Reserve() must be called first.
    status_t start(Buffers *buffers, JpegListener *listener, CameraMetadata* settings);

    // Compress and block until buffer is complete.
    status_t compressSynchronous(Buffers *buffers);

    status_t cancel();

    bool isBusy();
    bool isStreamInUse(uint32_t id);

    bool waitForDone(nsecs_t timeout);

    // Reserve the compressor for a later start() call.
    status_t reserve();

    // TODO: Measure this
    static const size_t kMaxJpegSize = 675000;

  private:
    Mutex mBusyMutex;
    bool mIsBusy;
    Condition mDone;
    bool mSynchronous;

    Mutex mMutex;

    Buffers *mBuffers;
    JpegListener *mListener;
    GraphicBufferMapper *mGBM;

    StreamBuffer mJpegBuffer, mAuxBuffer;
    bool mFoundJpeg, mFoundAux;
    CameraMetadata mSettings;

    status_t compress();

    void cleanUp();

    /**
     * Inherited Thread virtual overrides
     */
  private:
    virtual status_t readyToRun();
    virtual bool threadLoop();
};

} // namespace android

#endif
