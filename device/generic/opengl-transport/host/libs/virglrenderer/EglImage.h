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
#include <map>

#include <EGL/egl.h>
#include <EGL/eglext.h>

typedef EGLBoolean (*PFNEGLDESTROYIMAGEKHR)(EGLDisplay, EGLImageKHR);

struct EglImage {
    static std::map<uint32_t, EglImage*> map;
    static uint32_t nextId;

    EglImage(EGLDisplay dpy_, EGLImageKHR image_, PFNEGLDESTROYIMAGEKHR pfnEglDestroyImageKHR_)
        : pfnEglDestroyImageKHR(pfnEglDestroyImageKHR_), image(image_), dpy(dpy_), id(nextId++) {
        map.emplace(id, this);
    }

    ~EglImage() {
        pfnEglDestroyImageKHR(dpy, image);
        map.erase(id);
    }

    PFNEGLDESTROYIMAGEKHR pfnEglDestroyImageKHR;
    EGLImageKHR image;
    EGLDisplay dpy;
    uint32_t id;
};
