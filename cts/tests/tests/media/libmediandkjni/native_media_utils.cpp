/*
 * Copyright (C) 2017 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "NativeMedia"
#include <log/log.h>

#include <stdlib.h>
#include <math.h>
#include <string>
#include <algorithm>
#include <iterator>
#include "native_media_utils.h"

namespace Utils {

Status Thread::startThread() {
    assert(mHandle == 0);
    if (pthread_create(&mHandle, nullptr, Thread::thread_wrapper, this) != 0) {
        ALOGE("Failed to create thread");
        return FAIL;
    }
    return OK;
}

Status Thread::joinThread() {
    assert(mHandle != 0);
    void *ret;
    pthread_join(mHandle, &ret);
    return OK;
}

//static
void* Thread::thread_wrapper(void *obj) {
    assert(obj != nullptr);
    Thread *t = reinterpret_cast<Thread *>(obj);
    t->run();
    return nullptr;
}

}; // namespace Utils
