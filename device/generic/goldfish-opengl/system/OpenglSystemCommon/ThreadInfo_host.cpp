// Copyright 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "ThreadInfo.h"

#include "android/base/memory/LazyInstance.h"
#include "android/base/threads/Thread.h"
#include "android/base/threads/ThreadStore.h"

using android::base::LazyInstance;
using android::base::ThreadStoreBase;

static bool sDefaultTlsDestructorCallback(__attribute__((__unused__)) void* ptr) {
  return true;
}
static bool (*sTlsDestructorCallback)(void*) = sDefaultTlsDestructorCallback;

void setTlsDestructor(tlsDtorCallback func) {
    sTlsDestructorCallback = func;
}

static void doTlsDestruct(void* obj) {
    sTlsDestructorCallback(obj);
}

class ThreadInfoStore : public ThreadStoreBase {
public:
    ThreadInfoStore() : ThreadStoreBase(NULL) { }
    ~ThreadInfoStore();
};

static LazyInstance<ThreadInfoStore> sTls = LAZY_INSTANCE_INIT;

ThreadInfoStore::~ThreadInfoStore() {
    doTlsDestruct(sTls->get());
}

EGLThreadInfo *goldfish_get_egl_tls()
{
    EGLThreadInfo* ti = (EGLThreadInfo*)sTls->get();

    if (ti) return ti;

    ti = new EGLThreadInfo();
    sTls->set(ti);

    return ti;
}

EGLThreadInfo* getEGLThreadInfo() {
    return goldfish_get_egl_tls();
}

int32_t getCurrentThreadId() {
    return (int32_t)android::base::getCurrentThreadId();
}
