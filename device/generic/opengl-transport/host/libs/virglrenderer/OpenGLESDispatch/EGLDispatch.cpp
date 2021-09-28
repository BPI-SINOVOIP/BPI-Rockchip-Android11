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

#include <OpenGLESDispatch/EGLDispatch.h>

#include <dlfcn.h>
#include <stdio.h>

EGLDispatch s_egl;

#define DEFAULT_EGL_LIB "libEGL_swiftshader.so"

#define EGL_LOAD_FIELD(return_type, function_name, signature, callargs) \
    s_egl.function_name = (function_name##_t)dlsym(handle, #function_name);

#define EGL_LOAD_FIELD_WITH_EGL(return_type, function_name, signature, callargs) \
    if ((!s_egl.function_name) && s_egl.eglGetProcAddress)                       \
        s_egl.function_name = (function_name##_t)s_egl.eglGetProcAddress(#function_name);

#define EGL_LOAD_OPTIONAL_FIELD(return_type, function_name, signature, callargs)          \
    if (s_egl.eglGetProcAddress)                                                          \
        s_egl.function_name = (function_name##_t)s_egl.eglGetProcAddress(#function_name); \
    if (!s_egl.function_name || !s_egl.eglGetProcAddress)                                 \
    EGL_LOAD_FIELD(return_type, function_name, signature, callargs)

bool egl_dispatch_init() {
    void* handle = dlopen(DEFAULT_EGL_LIB, RTLD_NOW);
    if (!handle) {
        printf("Failed to open %s\n", dlerror());
        return false;
    }

    LIST_EGL_FUNCTIONS(EGL_LOAD_FIELD)
    LIST_EGL_FUNCTIONS(EGL_LOAD_FIELD_WITH_EGL)
    LIST_EGL_EXTENSIONS_FUNCTIONS(EGL_LOAD_OPTIONAL_FIELD)

    return true;
}
