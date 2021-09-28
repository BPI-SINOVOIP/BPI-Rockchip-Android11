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
#include <OpenGLESDispatch/GLESv3Dispatch.h>

#include <dlfcn.h>
#include <stdio.h>

#define DEFAULT_GLESv2_LIB "libGLESv2_swiftshader.so"

GLESv3Dispatch s_gles3;

#define LOOKUP_SYMBOL(return_type, function_name, signature, callargs)        \
    s_gles3.function_name = (function_name##_t)dlsym(handle, #function_name); \
    if ((!s_gles3.function_name) && s_egl.eglGetProcAddress)                  \
        s_gles3.function_name = (function_name##_t)s_egl.eglGetProcAddress(#function_name);

bool gles3_dispatch_init() {
    void* handle = dlopen(DEFAULT_GLESv2_LIB, RTLD_NOW);
    if (!handle) {
        printf("Failed to open %s\n", dlerror());
        return false;
    }

    LIST_GLES3_FUNCTIONS(LOOKUP_SYMBOL, LOOKUP_SYMBOL)

    return true;
}
