/*
* Copyright (C) 2015 The Android Open Source Project
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
#ifndef __COMMON_EGL_IMAGE_H
#define __COMMON_EGL_IMAGE_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#if PLATFORM_SDK_VERSION >= 16
#if EMULATOR_OPENGL_POST_O >= 1
#include <nativebase/nativebase.h>
#endif
#include <cutils/native_handle.h>
#else // PLATFORM_SDK_VERSION >= 16
#include <private/ui/android_natives_priv.h>
#endif // PLATFORM_SDK_VERSION >= 16

struct EGLImage_t
{
    EGLDisplay dpy;
    EGLenum target;

    union
    {
        android_native_buffer_t *native_buffer;
        uint32_t host_egl_image;
    };
};

#endif
