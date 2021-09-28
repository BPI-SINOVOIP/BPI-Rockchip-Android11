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
#ifndef VIDEOTEX_H
#define VIDEOTEX_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include <android/hardware/automotive/evs/1.0/IEvsEnumerator.h>

#include "BaseRenderCallback.h"
#include "StreamHandler.h"
#include "TexWrapper.h"

namespace android {
namespace automotive {
namespace evs {
namespace support {

using namespace ::android::hardware::automotive::evs::V1_0;


class VideoTex: public TexWrapper {
public:
    VideoTex() = delete;
    VideoTex(EGLDisplay glDisplay);
    virtual ~VideoTex();

    // returns true if the texture contents were updated
    bool refresh(const BufferDesc& imageBuffer);

private:
    EGLDisplay          mDisplay;
    EGLImageKHR mKHRimage = EGL_NO_IMAGE_KHR;
};
}  // namespace support
}  // namespace evs
}  // namespace automotive
}  // namespace android

#endif  // VIDEOTEX_H
