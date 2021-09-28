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
#include <vector>
#include <stdio.h>
#include <fcntl.h>
#include <alloca.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <png.h>

#include "VideoTex.h"
#include "glError.h"

#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferAllocator.h>
#include <ui/GraphicBufferMapper.h>

namespace android {
namespace automotive {
namespace evs {
namespace support {

// Eventually we shouldn't need this dependency, but for now the
// graphics allocator interface isn't fully supported on all platforms
// and this is our work around.
using ::android::GraphicBuffer;


VideoTex::VideoTex(EGLDisplay glDisplay)
    : TexWrapper()
    , mDisplay(glDisplay) {
    // Nothing but initialization here...
}

VideoTex::~VideoTex() {
    // Drop our device texture image
    if (mKHRimage != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(mDisplay, mKHRimage);
        mKHRimage = EGL_NO_IMAGE_KHR;
    }
}


// Return true if the texture contents are changed
bool VideoTex::refresh(const BufferDesc& imageBuffer) {
    // No new image has been delivered, so there's nothing to do here
    if (imageBuffer.memHandle.getNativeHandle() == nullptr) {
        return false;
    }

    // Drop our device texture image
    if (mKHRimage != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(mDisplay, mKHRimage);
        mKHRimage = EGL_NO_IMAGE_KHR;
    }

    // create a GraphicBuffer from the existing handle
    sp<GraphicBuffer> imageGraphicBuffer = new GraphicBuffer(
        imageBuffer.memHandle, GraphicBuffer::CLONE_HANDLE, imageBuffer.width,
        imageBuffer.height, imageBuffer.format, 1, // layer count
        GRALLOC_USAGE_HW_TEXTURE, imageBuffer.stride);

    if (imageGraphicBuffer.get() == nullptr) {
        ALOGE("Failed to allocate GraphicBuffer to wrap image handle");
        // Returning "true" in this error condition because we already released the
        // previous image (if any) and so the texture may change in unpredictable ways now!
        return true;
    }


    // Get a GL compatible reference to the graphics buffer we've been given
    EGLint eglImageAttributes[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
    EGLClientBuffer clientBuf = static_cast<EGLClientBuffer>(imageGraphicBuffer->getNativeBuffer());
    mKHRimage = eglCreateImageKHR(mDisplay, EGL_NO_CONTEXT,
                                  EGL_NATIVE_BUFFER_ANDROID, clientBuf,
                                  eglImageAttributes);
    if (mKHRimage == EGL_NO_IMAGE_KHR) {
        const char *msg = getEGLError();
        ALOGE("error creating EGLImage: %s", msg);
        return false;
    } else {
        // Update the texture handle we already created to refer to this gralloc buffer
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, glId());
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(mKHRimage));

        // Initialize the sampling properties (it seems the sample may not work if this isn't done)
        // The user of this texture may very well want to set their own filtering, but we're going
        // to pay the (minor) price of setting this up for them to avoid the dreaded "black image"
        // if they forget.
        // TODO:  Can we do this once for the texture ID rather than ever refresh?
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    return true;
}
}  // namespace support
}  // namespace evs
}  // namespace automotive
}  // namespace android
