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

#ifndef _NATIVE_MEDIA_UTILS_H_
#define _NATIVE_MEDIA_UTILS_H_

#include <pthread.h>
#include <sys/cdefs.h>
#include <stddef.h>
#include <assert.h>
#include <vector>

#include <android/native_window_jni.h>

#include "media/NdkMediaFormat.h"
#include "media/NdkMediaExtractor.h"
#include "media/NdkMediaCodec.h"
#include "media/NdkMediaMuxer.h"

namespace Utils {

enum Status : int32_t {
    FAIL = -1,
    OK = 0,
};

class Thread {
public:
    Thread()
        : mHandle(0) {
    }
    virtual ~Thread() {
        assert(mExited);
        mHandle = 0;
    }
    Thread(const Thread& ) = delete;
    Status startThread();
    Status joinThread();

protected:
    virtual void run() = 0;

private:
    static void* thread_wrapper(void *);
    pthread_t mHandle;
};

static inline void deleter_AMediExtractor(AMediaExtractor *_a) {
    AMediaExtractor_delete(_a);
}

static inline void deleter_AMediaCodec(AMediaCodec *_a) {
    AMediaCodec_delete(_a);
}

static inline void deleter_AMediaFormat(AMediaFormat *_a) {
    AMediaFormat_delete(_a);
}

static inline void deleter_AMediaMuxer(AMediaMuxer *_a) {
    AMediaMuxer_delete(_a);
}

static inline void deleter_ANativeWindow(ANativeWindow *_a) {
    ANativeWindow_release(_a);
}

}; //namespace Utils

#endif // _NATIVE_MEDIA_UTILS_H_
