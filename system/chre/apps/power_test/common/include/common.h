/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef CHRE_POWER_TEST_COMMON_H_
#define CHRE_POWER_TEST_COMMON_H_

#ifndef CHRE_NANOAPP_INTERNAL
#include "chre/util/nanoapp/log.h"
#endif  // CHRE_NANOAPP_INTERNAL

#ifdef CHRE_TCM_BUILD
#define LOG_TAG "[PowerTest_TCM]"
#else  // CHRE_TCM_BUILD
#define LOG_TAG "[PowerTest]"
#endif  // CHRE_TCM_BUILD

#endif  // CHRE_POWER_TEST_COMMON_H_