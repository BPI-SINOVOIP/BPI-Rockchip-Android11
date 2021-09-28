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
#include <android/hardware/camera/device/3.2/ICameraDevice.h>
#include <android-base/logging.h>

// Eventually we shouldn't need this dependency, but for now the
// graphics allocator interface isn't fully supported on all platforms
// and this is our work around.
using ::android::GraphicBuffer;


VideoTex::VideoTex(sp<IEvsEnumerator> pEnum,
                   sp<IEvsCamera> pCamera,
                   sp<StreamHandler> pStreamHandler,
                   EGLDisplay glDisplay)
    : TexWrapper()
    , mEnumerator(pEnum)
    , mCamera(pCamera)
    , mStreamHandler(pStreamHandler)
    , mDisplay(glDisplay) {
    // Nothing but initialization here...
}

VideoTex::~VideoTex() {
    // Tell the stream to stop flowing
    mStreamHandler->asyncStopStream();

    // Close the camera
    mEnumerator->closeCamera(mCamera);

    // Drop our device texture image
    if (mKHRimage != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(mDisplay, mKHRimage);
        mKHRimage = EGL_NO_IMAGE_KHR;
    }
}


// Return true if the texture contents are changed
bool VideoTex::refresh() {
    if (!mStreamHandler->newFrameAvailable()) {
        // No new image has been delivered, so there's nothing to do here
        return false;
    }

    // If we already have an image backing us, then it's time to return it
    if (mImageBuffer.buffer.nativeHandle.getNativeHandle() != nullptr) {
        // Drop our device texture image
        if (mKHRimage != EGL_NO_IMAGE_KHR) {
            eglDestroyImageKHR(mDisplay, mKHRimage);
            mKHRimage = EGL_NO_IMAGE_KHR;
        }

        // Return it since we're done with it
        mStreamHandler->doneWithFrame(mImageBuffer);
    }

    // Get the new image we want to use as our contents
    mImageBuffer = mStreamHandler->getNewFrame();


    // create a GraphicBuffer from the existing handle
    const AHardwareBuffer_Desc* pDesc =
        reinterpret_cast<const AHardwareBuffer_Desc *>(&mImageBuffer.buffer.description);
    sp<GraphicBuffer> pGfxBuffer = new GraphicBuffer(mImageBuffer.buffer.nativeHandle,
                                                     GraphicBuffer::CLONE_HANDLE,
                                                     pDesc->width,
                                                     pDesc->height,
                                                     pDesc->format,
                                                     1,//pDesc->layers,
                                                     GRALLOC_USAGE_HW_TEXTURE,
                                                     pDesc->stride);
    if (pGfxBuffer.get() == nullptr) {
        LOG(ERROR) << "Failed to allocate GraphicBuffer to wrap image handle";
        // Returning "true" in this error condition because we already released the
        // previous image (if any) and so the texture may change in unpredictable ways now!
        return true;
    }

    // Get a GL compatible reference to the graphics buffer we've been given
    EGLint eglImageAttributes[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
    EGLClientBuffer clientBuf = static_cast<EGLClientBuffer>(pGfxBuffer->getNativeBuffer());
    mKHRimage = eglCreateImageKHR(mDisplay, EGL_NO_CONTEXT,
                                  EGL_NATIVE_BUFFER_ANDROID, clientBuf,
                                  eglImageAttributes);
    if (mKHRimage == EGL_NO_IMAGE_KHR) {
        const char *msg = getEGLError();
        LOG(ERROR) << "Error creating EGLImage: " << msg;
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


VideoTex* createVideoTexture(sp<IEvsEnumerator> pEnum,
                             const char* evsCameraId,
                             std::unique_ptr<Stream> streamCfg,
                             EGLDisplay glDisplay,
                             bool useExternalMemory,
                             android_pixel_format_t format) {
    // Set up the camera to feed this texture
    sp<IEvsCamera> pCamera = nullptr;
    sp<StreamHandler> pStreamHandler = nullptr;
    if (streamCfg != nullptr) {
        pCamera = pEnum->openCamera_1_1(evsCameraId, *streamCfg);

        // Initialize the stream that will help us update this texture's contents
        pStreamHandler = new StreamHandler(pCamera,
                                           2,     // number of buffers
                                           useExternalMemory,
                                           format,
                                           streamCfg->width,
                                           streamCfg->height);
    } else {
        pCamera =
            IEvsCamera::castFrom(pEnum->openCamera(evsCameraId))
            .withDefault(nullptr);

        // Initialize the stream with the default resolution
        pStreamHandler = new StreamHandler(pCamera,
                                           2,     // number of buffers
                                           useExternalMemory,
                                           format);
    }

    if (pCamera == nullptr) {
        LOG(ERROR) << "Failed to allocate new EVS Camera interface for " << evsCameraId;
        return nullptr;
    }

    if (pStreamHandler == nullptr) {
        LOG(ERROR) << "Failed to allocate FrameHandler";
        return nullptr;
    }

    // Start the video stream
    if (!pStreamHandler->startStream()) {
        printf("Couldn't start the camera stream (%s)\n", evsCameraId);
        LOG(ERROR) << "Start stream failed for " << evsCameraId;
        return nullptr;
    }

    return new VideoTex(pEnum, pCamera, pStreamHandler, glDisplay);
}
