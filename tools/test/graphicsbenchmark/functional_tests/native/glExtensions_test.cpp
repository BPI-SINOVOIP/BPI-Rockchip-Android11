/*
 * Copyright 2018 The Android Open Source Project
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
 *
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include "testutils.h"

static EGLConfig eglConf;
static EGLSurface eglSurface;
static EGLContext eglCtx;
static EGLDisplay eglDisp;

void setupEGL(int w, int h) {
    const EGLint confAttr[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_RED_SIZE,   8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE,  8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE
    };

    const EGLint ctxAttr[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
    };

    const EGLint surfaceAttr[] = {
             EGL_WIDTH, w,
             EGL_HEIGHT, h,
             EGL_NONE
    };

    EGLint eglMajVers, eglMinVers;
    EGLint numConfigs;

    eglDisp = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(eglDisp, &eglMajVers, &eglMinVers);

    eglChooseConfig(eglDisp, confAttr, &eglConf, 1, &numConfigs);

    eglCtx = eglCreateContext(eglDisp, eglConf, EGL_NO_CONTEXT, ctxAttr);

    eglSurface = eglCreatePbufferSurface(eglDisp, eglConf, surfaceAttr);

    eglMakeCurrent(eglDisp, eglSurface, eglSurface, eglCtx);
}

void shutdownEGL() {
    eglMakeCurrent(eglDisp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(eglDisp, eglCtx);
    eglDestroySurface(eglDisp, eglSurface);
    eglTerminate(eglDisp);

    eglDisp = EGL_NO_DISPLAY;
    eglSurface = EGL_NO_SURFACE;
    eglCtx = EGL_NO_CONTEXT;
}

/**
 * The following OpenGL extensions are required:
 *     GL_EXT_color_buffer_half_float
 *     GL_EXT_shader_framebuffer_fetch
 */
TEST(glExtensions, glExtensions) {
    ASSUME_GAMECORE_CERTIFIED();

    std::vector<std::string> neededExts {"GL_EXT_color_buffer_half_float",
                                         "GL_EXT_shader_framebuffer_fetch"};

    setupEGL(64,64);

    std::string extString(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));

    std::istringstream iss(extString);
    //split by space
    std::vector<std::string> availableExts(std::istream_iterator<std::string>{iss},
                                           std::istream_iterator<std::string>());

    for (auto& ext : neededExts) {
        if (std::find(availableExts.begin(), availableExts.end(), ext) == availableExts.end())
            ADD_FAILURE() << "Could not find the GL extension: " << ext;
    }

    shutdownEGL();
}

/**
 * The following EGL extensions are required:
 *     EGL_ANDROID_get_frame_timestamps
 *     EGL_ANDROID_presentation_time
 *     EGL_KHR_fence_sync
 */
TEST(glExtensions, eglExtensions) {
    ASSUME_GAMECORE_CERTIFIED();

    std::vector<std::string> neededExts {"EGL_ANDROID_get_frame_timestamps",
                                         "EGL_ANDROID_presentation_time",
                                         "EGL_KHR_fence_sync"};

    setupEGL(64,64);

    std::string extString(eglQueryString(eglDisp, EGL_EXTENSIONS));

    std::istringstream iss(extString);
    //split by space
    std::vector<std::string> availableExts(std::istream_iterator<std::string>{iss},
                                           std::istream_iterator<std::string>());

    for (auto& ext : neededExts) {
        if (std::find(availableExts.begin(), availableExts.end(), ext) == availableExts.end())
            ADD_FAILURE() << "Could not find the EGL extension: " << ext;
    }

    shutdownEGL();
}
