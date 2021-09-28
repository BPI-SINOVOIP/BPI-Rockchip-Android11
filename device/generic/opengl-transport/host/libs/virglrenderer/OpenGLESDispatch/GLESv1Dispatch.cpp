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
#include <OpenGLESDispatch/GLESv1Dispatch.h>

#include <dlfcn.h>
#include <stdio.h>

#define DEFAULT_GLES_CM_LIB "libGLES_CM_swiftshader.so"

GLESv1Dispatch s_gles1;

#define LOOKUP_SYMBOL(return_type, function_name, signature, callargs)        \
    s_gles1.function_name = (function_name##_t)dlsym(handle, #function_name); \
    if ((!s_gles1.function_name) && s_egl.eglGetProcAddress)                  \
        s_gles1.function_name = (function_name##_t)s_egl.eglGetProcAddress(#function_name);

bool gles1_dispatch_init() {
    void* handle = dlopen(DEFAULT_GLES_CM_LIB, RTLD_NOW);
    if (!handle) {
        printf("Failed to open %s\n", dlerror());
        return false;
    }

    LIST_GLES1_FUNCTIONS(LOOKUP_SYMBOL, LOOKUP_SYMBOL)

    return true;
}
