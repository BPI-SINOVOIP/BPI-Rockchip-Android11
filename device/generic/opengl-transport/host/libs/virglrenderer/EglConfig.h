/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstdint>
#include <vector>

#include <EGL/egl.h>
#include <EGL/eglext.h>

typedef EGLBoolean (*PFNEGLGETCONFIGATTRIB)(EGLDisplay, EGLConfig, EGLint, EGLint*);

struct EglConfig {
    static std::vector<EglConfig*> vec;

    // clang-format off
    static constexpr EGLint kAttribs[] = {
        EGL_DEPTH_SIZE,
        EGL_STENCIL_SIZE,
        EGL_RENDERABLE_TYPE,
        EGL_SURFACE_TYPE,
        EGL_CONFIG_ID,
        EGL_BUFFER_SIZE,
        EGL_ALPHA_SIZE,
        EGL_BLUE_SIZE,
        EGL_GREEN_SIZE,
        EGL_RED_SIZE,
        EGL_CONFIG_CAVEAT,
        EGL_LEVEL,
        EGL_MAX_PBUFFER_HEIGHT,
        EGL_MAX_PBUFFER_PIXELS,
        EGL_MAX_PBUFFER_WIDTH,
        EGL_NATIVE_RENDERABLE,
        EGL_NATIVE_VISUAL_ID,
        EGL_NATIVE_VISUAL_TYPE,
        0x3030, // EGL_PRESERVED_RESOURCES
        EGL_SAMPLES,
        EGL_SAMPLE_BUFFERS,
        EGL_TRANSPARENT_TYPE,
        EGL_TRANSPARENT_BLUE_VALUE,
        EGL_TRANSPARENT_GREEN_VALUE,
        EGL_TRANSPARENT_RED_VALUE,
        EGL_BIND_TO_TEXTURE_RGB,
        EGL_BIND_TO_TEXTURE_RGBA,
        EGL_MIN_SWAP_INTERVAL,
        EGL_MAX_SWAP_INTERVAL,
        EGL_LUMINANCE_SIZE,
        EGL_ALPHA_MASK_SIZE,
        EGL_COLOR_BUFFER_TYPE,
        //EGL_MATCH_NATIVE_PIXMAP,
        EGL_RECORDABLE_ANDROID,
        EGL_CONFORMANT
    };
    // clang-format on

    static constexpr size_t kNumAttribs = sizeof(kAttribs) / sizeof(kAttribs[0]);

    EglConfig(EGLDisplay dpy, EGLConfig config_, PFNEGLGETCONFIGATTRIB pfnEglGetConfigAttrib)
        : config(config_) {
        for (size_t a = 0; a < kNumAttribs; a++) {
            if (!pfnEglGetConfigAttrib(dpy, config, kAttribs[a], &attribs[a]))
                attribs[a] = 0;
        }
        vec.push_back(this);
    }

    ~EglConfig() {
        for (size_t i = 0; i < EglConfig::vec.size(); i++) {
            if (vec[i] == this) {
                vec.erase(vec.begin() + i);
                break;
            }
        }
    }

    EGLint attribs[kNumAttribs];
    EGLConfig config;
};
