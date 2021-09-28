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

#include "gles1_core_functions.h"
#include "gles1_extensions_functions.h"
#include "gles2_core_functions.h"
#include "gles2_extensions_functions.h"
#include "gles3_only_functions.h"
#include "gles31_only_functions.h"

#define LIST_GLES1_FUNCTIONS(X, Y) \
    LIST_GLES1_CORE_FUNCTIONS(X) \
    LIST_GLES1_EXTENSIONS_FUNCTIONS(Y) \

#define LIST_GLES3_FUNCTIONS(X, Y) \
    LIST_GLES2_CORE_FUNCTIONS(X) \
    LIST_GLES2_EXTENSIONS_FUNCTIONS(Y) \
    LIST_GLES3_ONLY_FUNCTIONS(X) \
    LIST_GLES31_ONLY_FUNCTIONS(X)
