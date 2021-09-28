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

#include "SurroundViewServiceCallback.h"

#include <android-base/logging.h>
#include <math/mat4.h>
#include <ui/GraphicBuffer.h>
#include <utils/Log.h>

#include "shader_simpleTex.h"
#include "shader.h"

using android::GraphicBuffer;
using android::hardware::automotive::evs::V1_0::DisplayState;
using android::hardware::automotive::evs::V1_0::EvsResult;
using android::hardware::Return;
using android::sp;
using std::string;

EGLDisplay   SurroundViewServiceCallback::sGLDisplay;
GLuint       SurroundViewServiceCallback::sFrameBuffer;
GLuint       SurroundViewServiceCallback::sColorBuffer;
GLuint       SurroundViewServiceCallback::sDepthBuffer;
GLuint       SurroundViewServiceCallback::sTextureId;
EGLImageKHR  SurroundViewServiceCallback::sKHRimage;

const char* SurroundViewServiceCallback::getEGLError(void) {
    switch (eglGetError()) {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST";
        default:
            return "Unknown error";
    }
}

const string SurroundViewServiceCallback::getGLFramebufferError(void) {
    switch (glCheckFramebufferStatus(GL_FRAMEBUFFER)) {
    case GL_FRAMEBUFFER_COMPLETE:
        return "GL_FRAMEBUFFER_COMPLETE";
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
    case GL_FRAMEBUFFER_UNSUPPORTED:
        return "GL_FRAMEBUFFER_UNSUPPORTED";
    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
        return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
    default:
        return std::to_string(glCheckFramebufferStatus(GL_FRAMEBUFFER));
    }
}

bool SurroundViewServiceCallback::prepareGL() {
    LOG(DEBUG) << __FUNCTION__;

    // Just trivially return success if we're already prepared
    if (sGLDisplay != EGL_NO_DISPLAY) {
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

    // Set up our OpenGL ES context associated with the default display
    // (though we won't be visible)
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        LOG(ERROR) << "Failed to get egl display";
        return false;
    }

    EGLint major = 0;
    EGLint minor = 0;
    if (!eglInitialize(display, &major, &minor)) {
        LOG(ERROR) << "Failed to initialize EGL: "
                   << getEGLError();
        return false;
    } else {
        LOG(INFO) << "Initialized EGL at "
                  << major
                  << "."
                  << minor;
    }

    // Select the configuration that "best" matches our desired characteristics
    EGLConfig egl_config;
    EGLint num_configs;
    if (!eglChooseConfig(display, config_attribs, &egl_config, 1,
                         &num_configs)) {
        LOG(ERROR) << "eglChooseConfig() failed with error: "
                   << getEGLError();
        return false;
    }

    // Create a placeholder pbuffer so we have a surface to bind -- we never intend
    // to draw to this because attachRenderTarget will be called first.
    EGLint surface_attribs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    EGLSurface sPlaceholderSurface = eglCreatePbufferSurface(display, egl_config, surface_attribs);
    if (sPlaceholderSurface == EGL_NO_SURFACE) {
        LOG(ERROR) << "Failed to create OpenGL ES Placeholder surface: " << getEGLError();
        return false;
    } else {
        LOG(INFO) << "Placeholder surface looks good!  :)";
    }

    //
    // Create the EGL context
    //
    EGLContext context = eglCreateContext(display, egl_config,
                                          EGL_NO_CONTEXT, context_attribs);
    if (context == EGL_NO_CONTEXT) {
        LOG(ERROR) << "Failed to create OpenGL ES Context: "
                   << getEGLError();
        return false;
    }

    // Activate our render target for drawing
    if (!eglMakeCurrent(display, sPlaceholderSurface, sPlaceholderSurface, context)) {
        LOG(ERROR) << "Failed to make the OpenGL ES Context current: "
                   << getEGLError();
        return false;
    } else {
        LOG(INFO) << "We made our context current!  :)";
    }

    // Report the extensions available on this implementation
    const char* gl_extensions = (const char*) glGetString(GL_EXTENSIONS);
    LOG(INFO) << "GL EXTENSIONS:\n  "
              << gl_extensions;

    // Reserve handles for the color and depth targets we'll be setting up
    glGenRenderbuffers(1, &sColorBuffer);
    glGenRenderbuffers(1, &sDepthBuffer);

    // Set up the frame buffer object we can modify and use for off screen
    // rendering
    glGenFramebuffers(1, &sFrameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, sFrameBuffer);

    LOG(INFO) << "FrameBuffer is bound to "
              << sFrameBuffer;

    // New (from TextWrapper)
    glGenTextures(1, &sTextureId);

    // Now that we're assured success, store object handles we constructed
    sGLDisplay = display;

    GLuint mShaderProgram = 0;
    // Load our shader program if we don't have it already
    if (!mShaderProgram) {
        mShaderProgram = buildShaderProgram(kVtxShaderSimpleTexture,
                                            kPixShaderSimpleTexture,
                                            "simpleTexture");
        if (!mShaderProgram) {
            LOG(ERROR) << "Error building shader program";
            return false;
        }
    }

    // Select our screen space simple texture shader
    glUseProgram(mShaderProgram);

    // Set up the model to clip space transform (identity matrix if we're
    // modeling in screen space)
    GLint loc = glGetUniformLocation(mShaderProgram, "cameraMat");
    if (loc < 0) {
        LOG(ERROR) << "Couldn't set shader parameter 'cameraMat'";
    } else {
        const android::mat4 identityMatrix;
        glUniformMatrix4fv(loc, 1, false, identityMatrix.asArray());
    }

    GLint sampler = glGetUniformLocation(mShaderProgram, "tex");
    if (sampler < 0) {
        LOG(ERROR) << "Couldn't set shader parameter 'tex'";
    } else {
        // Tell the sampler we looked up from the shader to use texture slot 0
        // as its source
        glUniform1i(sampler, 0);
    }

    return true;
}

BufferDesc SurroundViewServiceCallback::convertBufferDesc(
    const BufferDesc_1_0& src) {
    BufferDesc dst = {};
    AHardwareBuffer_Desc* pDesc =
        reinterpret_cast<AHardwareBuffer_Desc *>(&dst.buffer.description);
    pDesc->width  = src.width;
    pDesc->height = src.height;
    pDesc->layers = 1;
    pDesc->format = src.format;
    pDesc->usage  = static_cast<uint64_t>(src.usage);
    pDesc->stride = src.stride;

    dst.buffer.nativeHandle = src.memHandle;
    dst.pixelSize = src.pixelSize;
    dst.bufferId = src.bufferId;

    return dst;
}

bool SurroundViewServiceCallback::attachRenderTarget(
    const BufferDesc& tgtBuffer) {
    const AHardwareBuffer_Desc* pDesc =
        reinterpret_cast<const AHardwareBuffer_Desc *>(
            &tgtBuffer.buffer.description);
    // Hardcoded to RGBx for now
    if (pDesc->format != HAL_PIXEL_FORMAT_RGBA_8888) {
        LOG(ERROR) << "Unsupported target buffer format";
        return false;
    }

    // create a GraphicBuffer from the existing handle
    sp<GraphicBuffer> pGfxBuffer =
        new GraphicBuffer(tgtBuffer.buffer.nativeHandle,
                          GraphicBuffer::CLONE_HANDLE,
                          pDesc->width,
                          pDesc->height,
                          pDesc->format,
                          pDesc->layers,
                          GRALLOC_USAGE_HW_RENDER,
                          pDesc->stride);
    if (pGfxBuffer == nullptr) {
        LOG(ERROR) << "Failed to allocate GraphicBuffer to wrap image handle";
        return false;
    }

    // Get a GL compatible reference to the graphics buffer we've been given
    EGLint eglImageAttributes[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
    EGLClientBuffer clientBuf = static_cast<EGLClientBuffer>(
        pGfxBuffer->getNativeBuffer());

    // Destroy current KHR image due to new request.
    if (sKHRimage != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(sGLDisplay, sKHRimage);
    }

    sKHRimage = eglCreateImageKHR(sGLDisplay, EGL_NO_CONTEXT,
                                  EGL_NATIVE_BUFFER_ANDROID, clientBuf,
                                  eglImageAttributes);
    if (sKHRimage == EGL_NO_IMAGE_KHR) {
        LOG(ERROR) << "error creating EGLImage for target buffer: "
                   << getEGLError();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, sFrameBuffer);

    // Construct a render buffer around the external buffer
    glBindRenderbuffer(GL_RENDERBUFFER, sColorBuffer);
    glEGLImageTargetRenderbufferStorageOES(
        GL_RENDERBUFFER, static_cast<GLeglImageOES>(sKHRimage));
    if (eglGetError() != EGL_SUCCESS) {
        LOG(INFO) << "glEGLImageTargetRenderbufferStorageOES => %s"
                  << getEGLError();
        return false;
    }

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, sColorBuffer);
    if (eglGetError() != EGL_SUCCESS) {
        LOG(ERROR) << "glFramebufferRenderbuffer => %s", getEGLError();
        return false;
    }

    GLenum checkResult = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (checkResult != GL_FRAMEBUFFER_COMPLETE) {
        LOG(ERROR) << "Offscreen framebuffer not configured successfully ("
                   << checkResult
                   << ": "
                   << getGLFramebufferError().c_str()
                   << ")";
        if (eglGetError() != EGL_SUCCESS) {
            LOG(ERROR) << "glCheckFramebufferStatus => "
                       << getEGLError();
        }
        return false;
    }

    // Set the viewport
    glViewport(0, 0, pDesc->width, pDesc->height);

    // We don't actually need the clear if we're going to cover the whole
    // screen anyway
    // Clear the color buffer
    glClearColor(0.8f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    return true;
}

void SurroundViewServiceCallback::detachRenderTarget() {
    // Drop our external render target
    if (sKHRimage != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(sGLDisplay, sKHRimage);
        sKHRimage = EGL_NO_IMAGE_KHR;
    }
}

SurroundViewServiceCallback::SurroundViewServiceCallback(
    sp<IEvsDisplay> pDisplay,
    sp<ISurroundViewSession> pSession) :
    mDisplay(pDisplay),
    mSession(pSession) {
    // Nothing but member initialization
}

Return<void> SurroundViewServiceCallback::notify(SvEvent svEvent) {
    // Waiting for STREAM_STARTED event.
    if (svEvent == SvEvent::STREAM_STARTED) {
        LOG(INFO) << "Received STREAM_STARTED event";

        // Set the display state to VISIBLE_ON_NEXT_FRAME
        if (mDisplay != nullptr) {
            Return<EvsResult> result =
                mDisplay->setDisplayState(DisplayState::VISIBLE_ON_NEXT_FRAME);
            if (result != EvsResult::OK) {
              LOG(ERROR) << "Failed to setDisplayState";
            }
        } else {
            LOG(WARNING) << "setDisplayState is ignored since EVS display"
                         << " is null";
        }

        // Set up OpenGL (exit if fail)
        if (!prepareGL()) {
            LOG(ERROR) << "Error while setting up OpenGL!";
            exit(EXIT_FAILURE);
        }
    } else if (svEvent == SvEvent::CONFIG_UPDATED) {
        LOG(INFO) << "Received CONFIG_UPDATED event";
    } else if (svEvent == SvEvent::STREAM_STOPPED) {
        LOG(INFO) << "Received STREAM_STOPPED event";
    } else if (svEvent == SvEvent::FRAME_DROPPED) {
        LOG(INFO) << "Received FRAME_DROPPED event";
    } else if (svEvent == SvEvent::TIMEOUT) {
        LOG(INFO) << "Received TIMEOUT event";
    } else {
        LOG(INFO) << "Received unknown event";
    }
    return {};
}

Return<void> SurroundViewServiceCallback::receiveFrames(
    const SvFramesDesc& svFramesDesc) {
    LOG(INFO) << "Incoming frames with svBuffers size: "
              << svFramesDesc.svBuffers.size();
    if (svFramesDesc.svBuffers.size() == 0) {
        return {};
    }

    // Now we assume there is only one frame for both 2d and 3d.
    auto handle =
          svFramesDesc.svBuffers[0].hardwareBuffer.nativeHandle
          .getNativeHandle();
    const AHardwareBuffer_Desc* pDesc =
          reinterpret_cast<const AHardwareBuffer_Desc *>(
              &svFramesDesc.svBuffers[0].hardwareBuffer.description);

    LOG(INFO) << "App received frames";
    LOG(INFO) << "descData: "
              << pDesc->width
              << pDesc->height
              << pDesc->layers
              << pDesc->format
              << pDesc->usage
              << pDesc->stride;
    LOG(INFO) << "nativeHandle: "
              << handle;

    // Only process the frame when EVS display is valid. If
    // not, ignore the coming frame.
    if (mDisplay == nullptr) {
        LOG(WARNING) << "Display is not ready. Skip the frame";
    } else {
        // Get display buffer from EVS display
        BufferDesc_1_0 tgtBuffer = {};
        mDisplay->getTargetBuffer([&tgtBuffer](const BufferDesc_1_0& buff) {
            tgtBuffer = buff;
        });

        if (!attachRenderTarget(convertBufferDesc(tgtBuffer))) {
            LOG(ERROR) << "Failed to attach render target";
            return {};
        } else {
            LOG(INFO) << "Successfully attached render target";
        }

        // Call HIDL API "doneWithFrames" to return the ownership
        // back to SV service
        if (mSession == nullptr) {
            LOG(ERROR) << "SurroundViewSession in callback is invalid";
            return {};
        } else {
            mSession->doneWithFrames(svFramesDesc);
        }

        // Render frame to EVS display
        LOG(INFO) << "Rendering to display buffer";
        sp<GraphicBuffer> graphicBuffer =
            new GraphicBuffer(handle,
                              GraphicBuffer::CLONE_HANDLE,
                              pDesc->width,
                              pDesc->height,
                              pDesc->format,
                              pDesc->layers,  // layer count
                              pDesc->usage,
                              pDesc->stride);

        EGLImageKHR KHRimage = EGL_NO_IMAGE_KHR;

        // Get a GL compatible reference to the graphics buffer we've been given
        EGLint eglImageAttributes[] = {
            EGL_IMAGE_PRESERVED_KHR,
            EGL_TRUE,
            EGL_NONE
        };
        EGLClientBuffer clientBuf = static_cast<EGLClientBuffer>(
            graphicBuffer->getNativeBuffer());
        KHRimage = eglCreateImageKHR(sGLDisplay, EGL_NO_CONTEXT,
                                     EGL_NATIVE_BUFFER_ANDROID, clientBuf,
                                     eglImageAttributes);
        if (KHRimage == EGL_NO_IMAGE_KHR) {
            const char *msg = getEGLError();
            LOG(ERROR) << "error creating EGLImage: "
                       << msg;
            return {};
        } else {
            LOG(INFO) << "Successfully created EGLImage";

            // Update the texture handle we already created to refer to
            // this gralloc buffer
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sTextureId);
            glEGLImageTargetTexture2DOES(GL_TEXTURE_2D,
                                         static_cast<GLeglImageOES>(KHRimage));

            // Initialize the sampling properties (it seems the sample may
            // not work if this isn't done)
            // The user of this texture may very well want to set their own
            // filtering, but we're going to pay the (minor) price of
            // setting this up for them to avoid the dreaded "black image"
            // if they forget.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                            GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                            GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                            GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                            GL_CLAMP_TO_EDGE);
        }

        // Bind the texture and assign it to the shader's sampler
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sTextureId);

        // We want our image to show up opaque regardless of alpha values
        glDisable(GL_BLEND);

        // Draw a rectangle on the screen
        const GLfloat vertsCarPos[] = {
            -1.0,  1.0, 0.0f,   // left top in window space
            1.0,  1.0, 0.0f,    // right top
            -1.0, -1.0, 0.0f,   // left bottom
            1.0, -1.0, 0.0f     // right bottom
        };
        const GLfloat vertsCarTex[] = {
            0.0f, 0.0f,   // left top
            1.0f, 0.0f,   // right top
            0.0f, 1.0f,   // left bottom
            1.0f, 1.0f    // right bottom
        };
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertsCarPos);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, vertsCarTex);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);

        // Now that everything is submitted, release our hold on the
        // texture resource
        detachRenderTarget();

        // Wait for the rendering to finish
        glFinish();
        detachRenderTarget();

        // Drop our external render target
        if (KHRimage != EGL_NO_IMAGE_KHR) {
            eglDestroyImageKHR(sGLDisplay, KHRimage);
            KHRimage = EGL_NO_IMAGE_KHR;
        }

        LOG(DEBUG) << "Rendering finished. Going to return the buffer";

        // Call HIDL API "doneWithFrames" to return the ownership
        // back to SV service
        if (mSession == nullptr) {
            LOG(WARNING) << "SurroundViewSession in callback is invalid";
        } else {
            mSession->doneWithFrames(svFramesDesc);
        }

        // Return display buffer back to EVS display
        mDisplay->returnTargetBufferForDisplay(tgtBuffer);
    }
    return {};
}
