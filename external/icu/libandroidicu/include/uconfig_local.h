/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// This file provides preprocessor configuration for ICU code when used in
// libandroidicu. The libandroidicu library provides a shim layer over ICU4C.
// It is like the full ICU4C APIs with the following differences.

#define U_DISABLE_VERSION_SUFFIX 1
#define U_LIB_SUFFIX_C_NAME _android
#define U_HIDE_DRAFT_API 1
#define U_HIDE_DEPRECATED_API 1
#define U_SHOW_CPLUSPLUS_API 0
#define U_HIDE_INTERNAL_API 1
