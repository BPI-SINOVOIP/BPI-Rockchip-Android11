/*
 * Copyright (C) 2020 The Android Open Source Project
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

/*
 * Implementation Notes
 * Each platform must supply chpp/platform/platform_mutex.h which provides the
 * platform-specific definitions (and implementation as necessary) for the
 * definitions in this file.
 */

#ifndef CHPP_MUTEX_H_
#define CHPP_MUTEX_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform-specific mutex struct, including the member "lock"
 */
struct ChppMutex;

/*
 * Initializes a specified platform-specific mutex.
 *
 * @param mutex points to the ChppMutex mutex struct.
 */
static void chppMutexInit(struct ChppMutex *mutex);

/*
 * Locks a specified platform-specific mutex.
 *
 * @param mutex points to the ChppMutex mutex struct.
 */
static void chppMutexLock(struct ChppMutex *mutex);

/*
 * Unlocks a specified platform-specific mutex.
 *
 * @param mutex points to the ChppMutex mutex struct.
 */
static void chppMutexUnlock(struct ChppMutex *mutex);

#ifdef __cplusplus
}
#endif

#include "chpp/platform/platform_mutex.h"

#endif  // CHPP_MUTEX_H_
