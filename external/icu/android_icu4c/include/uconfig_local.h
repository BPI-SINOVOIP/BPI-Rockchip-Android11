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

/**
 * \def ANDROID_LINK_SHARED_ICU4C
 * If an Android libarary links aganist libicuuc without this flag,
 * stops the build process. Only the following libraries should have this
 * flag,
 * - Libraries in Runtime APEX module
 * Otherwise, the libraries should use libandroidicu.
 */
#if defined(__ANDROID__) && !defined(ANDROID_LINK_SHARED_ICU4C)
#error "Please use libandroidicu and do not directly link to libicuuc or libicui18n."
#endif

