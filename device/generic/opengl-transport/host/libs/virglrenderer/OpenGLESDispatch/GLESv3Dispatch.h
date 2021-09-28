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

#include <GLES3/gl31.h>

#include <OpenGLESDispatch/gles_functions.h>

// Define function pointer types.
#define GLES3_DISPATCH_DEFINE_TYPE(return_type, func_name, signature, callargs) \
    typedef return_type(KHRONOS_APIENTRY* func_name##_t) signature;

#define GLES3_DISPATCH_DECLARE_POINTER(return_type, func_name, signature, callargs) \
    func_name##_t func_name;

LIST_GLES3_FUNCTIONS(GLES3_DISPATCH_DEFINE_TYPE, GLES3_DISPATCH_DEFINE_TYPE)

struct GLESv3Dispatch {
    LIST_GLES3_FUNCTIONS(GLES3_DISPATCH_DECLARE_POINTER, GLES3_DISPATCH_DECLARE_POINTER)
};

#undef GLES3_DISPATCH_DEFINE_TYPE
#undef GLES3_DISPATCH_DECLARE_POINTER

bool gles3_dispatch_init();

extern GLESv3Dispatch s_gles3;
