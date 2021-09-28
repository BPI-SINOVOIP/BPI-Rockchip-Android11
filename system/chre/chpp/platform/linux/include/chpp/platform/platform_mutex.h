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

#ifndef CHPP_PLATFORM_MUTEX_H_
#define CHPP_PLATFORM_MUTEX_H_

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ChppMutex {
  pthread_mutex_t lock;
};

static inline void chppMutexInit(struct ChppMutex *mutex) {
  pthread_mutex_init(&mutex->lock, NULL);
}

static inline void chppMutexLock(struct ChppMutex *mutex) {
  pthread_mutex_lock(&mutex->lock);
}

static inline void chppMutexUnlock(struct ChppMutex *mutex) {
  pthread_mutex_unlock(&mutex->lock);
}

#ifdef __cplusplus
}
#endif

#endif  // CHPP_PLATFORM_MUTEX_H_
