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

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <egl_functions.h>
#include <egl_extensions_functions.h>

#define EGL_DEFINE_TYPE(return_type, function_name, signature, callargs) \
    typedef return_type(EGLAPIENTRY* function_name##_t) signature;

#define EGL_DECLARE_MEMBER(return_type, function_name, signature, callargs) \
    function_name##_t function_name;

LIST_EGL_FUNCTIONS(EGL_DEFINE_TYPE)
LIST_EGL_EXTENSIONS_FUNCTIONS(EGL_DEFINE_TYPE)

struct EGLDispatch {
    LIST_EGL_FUNCTIONS(EGL_DECLARE_MEMBER)
    LIST_EGL_EXTENSIONS_FUNCTIONS(EGL_DECLARE_MEMBER)
};

bool egl_dispatch_init();

extern EGLDispatch s_egl;
