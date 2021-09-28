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

#include <GLES/gl.h>

#include <OpenGLESDispatch/gles_functions.h>

#define GLES1_DISPATCH_DEFINE_TYPE(return_type, func_name, signature, callargs) \
    typedef return_type(KHRONOS_APIENTRY* func_name##_t) signature;

#define GLES1_DISPATCH_DECLARE_POINTER(return_type, func_name, signature, callargs) \
    func_name##_t func_name;

LIST_GLES1_FUNCTIONS(GLES1_DISPATCH_DEFINE_TYPE, GLES1_DISPATCH_DEFINE_TYPE)

struct GLESv1Dispatch {
    LIST_GLES1_FUNCTIONS(GLES1_DISPATCH_DECLARE_POINTER, GLES1_DISPATCH_DECLARE_POINTER)
};

#undef GLES1_DISPATCH_DEFINE_TYPE
#undef GLES1_DISPATCH_DECLARE_POINTER

bool gles1_dispatch_init();

extern GLESv1Dispatch s_gles1;
