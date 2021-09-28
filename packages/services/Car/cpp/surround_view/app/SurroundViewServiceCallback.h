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
#include <stdio.h>

#include <utils/StrongPointer.h>

#include <android/hardware/automotive/sv/1.0/ISurroundViewService.h>
#include <android/hardware/automotive/sv/1.0/ISurroundViewStream.h>
#include <android/hardware/automotive/evs/1.1/IEvsEnumerator.h>
#include <android/hardware_buffer.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

using namespace android::hardware::automotive::sv::V1_0;
using namespace android::hardware::automotive::evs::V1_1;

using BufferDesc_1_0  = ::android::hardware::automotive::evs::V1_0::BufferDesc;

class SurroundViewServiceCallback : public ISurroundViewStream {
public:
    SurroundViewServiceCallback(android::sp<IEvsDisplay> pDisplay,
                                android::sp<ISurroundViewSession> pSession);

    // Methods from ::android::hardware::automotive::sv::V1_0::ISurroundViewStream.
    android::hardware::Return<void> notify(SvEvent svEvent) override;
    android::hardware::Return<void> receiveFrames(const SvFramesDesc& svFramesDesc) override;

private:
    static const char* getEGLError(void);
    static const std::string getGLFramebufferError(void);

    bool prepareGL();
    BufferDesc convertBufferDesc(const BufferDesc_1_0& src);
    bool attachRenderTarget(const BufferDesc& tgtBuffer);
    void detachRenderTarget();

    static EGLDisplay   sGLDisplay;
    static GLuint       sFrameBuffer;
    static GLuint       sColorBuffer;
    static GLuint       sDepthBuffer;
    static GLuint       sTextureId;
    static EGLImageKHR  sKHRimage;

    android::sp<IEvsDisplay> mDisplay;
    android::sp<ISurroundViewSession> mSession;
};
