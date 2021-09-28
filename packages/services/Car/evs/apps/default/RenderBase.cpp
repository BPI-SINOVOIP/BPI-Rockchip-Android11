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

#include "RenderBase.h"
#include "glError.h"

#include <android-base/logging.h>
#include <ui/GraphicBuffer.h>

// Eventually we shouldn't need this dependency, but for now the
// graphics allocator interface isn't fully supported on all platforms
// and this is our work around.
using ::android::GraphicBuffer;


// OpenGL state shared among all renderers
EGLDisplay   RenderBase::sDisplay = EGL_NO_DISPLAY;
EGLContext   RenderBase::sContext = EGL_NO_CONTEXT;
EGLSurface   RenderBase::sDummySurface = EGL_NO_SURFACE;
GLuint       RenderBase::sFrameBuffer = -1;
GLuint       RenderBase::sColorBuffer = -1;
GLuint       RenderBase::sDepthBuffer = -1;
EGLImageKHR  RenderBase::sKHRimage = EGL_NO_IMAGE_KHR;
unsigned     RenderBase::sWidth  = 0;
unsigned     RenderBase::sHeight = 0;
float        RenderBase::sAspectRatio = 0.0f;


bool RenderBase::prepareGL() {
    // Just trivially return success if we're already prepared
    if (sDisplay != EGL_NO_DISPLAY) {
        return true;
    }

    // Hardcoded to RGBx output display
    const EGLint config_attribs[] = {
        // Tag                  Value
        EGL_RENDERABLE_TYPE,    EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,           8,
        EGL_GREEN_SIZE,         8,
        EGL_BLUE_SIZE,          8,
        EGL_NONE
    };

    // Select OpenGL ES v 3
    const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};


    // Set up our OpenGL ES context associated with the default display (though we won't be visible)
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        LOG(ERROR) << "Failed to get egl display";
        return false;
    }

    EGLint major = 0;
    EGLint minor = 0;
    if (!eglInitialize(display, &major, &minor)) {
        LOG(ERROR) << "Failed to initialize EGL: " << getEGLError();
        return false;
    } else {
        LOG(INFO) << "Intiialized EGL at " << major << "." << minor;
    }


    // Select the configuration that "best" matches our desired characteristics
    EGLConfig egl_config;
    EGLint num_configs;
    if (!eglChooseConfig(display, config_attribs, &egl_config, 1, &num_configs)) {
        LOG(ERROR) << "eglChooseConfig() failed with error: " << getEGLError();
        return false;
    }


    // Create a dummy pbuffer so we have a surface to bind -- we never intend to draw to this
    // because attachRenderTarget will be called first.
    EGLint surface_attribs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    sDummySurface = eglCreatePbufferSurface(display, egl_config, surface_attribs);
    if (sDummySurface == EGL_NO_SURFACE) {
        LOG(ERROR) << "Failed to create OpenGL ES Dummy surface: " << getEGLError();
        return false;
    } else {
        LOG(INFO) << "Dummy surface looks good!  :)";
    }


    //
    // Create the EGL context
    //
    EGLContext context = eglCreateContext(display, egl_config, EGL_NO_CONTEXT, context_attribs);
    if (context == EGL_NO_CONTEXT) {
        LOG(ERROR) << "Failed to create OpenGL ES Context: " << getEGLError();
        return false;
    }


    // Activate our render target for drawing
    if (!eglMakeCurrent(display, sDummySurface, sDummySurface, context)) {
        LOG(ERROR) << "Failed to make the OpenGL ES Context current: " << getEGLError();
        return false;
    } else {
        LOG(INFO) << "We made our context current!  :)";
    }


    // Report the extensions available on this implementation
    const char* gl_extensions = (const char*) glGetString(GL_EXTENSIONS);
    LOG(INFO) << "GL EXTENSIONS:\n  " << gl_extensions;


    // Reserve handles for the color and depth targets we'll be setting up
    glGenRenderbuffers(1, &sColorBuffer);
    glGenRenderbuffers(1, &sDepthBuffer);

    // Set up the frame buffer object we can modify and use for off screen rendering
    glGenFramebuffers(1, &sFrameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, sFrameBuffer);


    // Now that we're assured success, store object handles we constructed
    sDisplay = display;
    sContext = context;

    return true;
}


bool RenderBase::attachRenderTarget(const BufferDesc& tgtBuffer) {
    const AHardwareBuffer_Desc* pDesc =
        reinterpret_cast<const AHardwareBuffer_Desc *>(&tgtBuffer.buffer.description);
    // Hardcoded to RGBx for now
    if (pDesc->format != HAL_PIXEL_FORMAT_RGBA_8888) {
        LOG(ERROR) << "Unsupported target buffer format";
        return false;
    }

    // create a GraphicBuffer from the existing handle
    sp<GraphicBuffer> pGfxBuffer = new GraphicBuffer(tgtBuffer.buffer.nativeHandle,
                                                     GraphicBuffer::CLONE_HANDLE,
                                                     pDesc->width,
                                                     pDesc->height,
                                                     pDesc->format,
                                                     pDesc->layers,
                                                     GRALLOC_USAGE_HW_RENDER,
                                                     pDesc->stride);
    if (pGfxBuffer.get() == nullptr) {
        LOG(ERROR) << "Failed to allocate GraphicBuffer to wrap image handle";
        return false;
    }

    // Get a GL compatible reference to the graphics buffer we've been given
    EGLint eglImageAttributes[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
    EGLClientBuffer clientBuf = static_cast<EGLClientBuffer>(pGfxBuffer->getNativeBuffer());
    sKHRimage = eglCreateImageKHR(sDisplay, EGL_NO_CONTEXT,
                                  EGL_NATIVE_BUFFER_ANDROID, clientBuf,
                                  eglImageAttributes);
    if (sKHRimage == EGL_NO_IMAGE_KHR) {
        LOG(ERROR) << "Error creating EGLImage for target buffer: " << getEGLError();
        return false;
    }

    // Construct a render buffer around the external buffer
    glBindRenderbuffer(GL_RENDERBUFFER, sColorBuffer);
    glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, static_cast<GLeglImageOES>(sKHRimage));
    if (eglGetError() != EGL_SUCCESS) {
        LOG(INFO) << "glEGLImageTargetRenderbufferStorageOES => " << getEGLError();
        return false;
    }

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, sColorBuffer);
    if (eglGetError() != EGL_SUCCESS) {
        LOG(ERROR) << "glFramebufferRenderbuffer => " << getEGLError();
        return false;
    }

    GLenum checkResult = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (checkResult != GL_FRAMEBUFFER_COMPLETE) {
        LOG(ERROR) << "Offscreen framebuffer not configured successfully ("
                   << checkResult << ": " << getGLFramebufferError() << ")";
        return false;
    }

    // Store the size of our target buffer
    sWidth = pDesc->width;
    sHeight = pDesc->height;
    sAspectRatio = (float)sWidth / sHeight;

    // Set the viewport
    glViewport(0, 0, sWidth, sHeight);

    // We don't actually need the clear if we're going to cover the whole screen anyway
    // Clear the color buffer
    glClearColor(0.8f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    return true;
}


void RenderBase::detachRenderTarget() {
    // Drop our external render target
    if (sKHRimage != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(sDisplay, sKHRimage);
        sKHRimage = EGL_NO_IMAGE_KHR;
    }
}
