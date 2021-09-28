/*
 * Copyright 2018 The Android Open Source Project
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

#include <jni.h>
#include <android/trace.h>

static jboolean isEnabled(JNIEnv*, jclass) {
    return ATrace_isEnabled();
}

static void beginEndSection(JNIEnv*, jclass) {
    ATrace_beginSection("ndk::beginEndSection");
    ATrace_endSection();
}

static void asyncBeginEndSection(JNIEnv*, jclass) {
    ATrace_beginAsyncSection("ndk::asyncBeginEndSection", 4770);
    ATrace_endAsyncSection("ndk::asyncBeginEndSection", 4770);
}


static void counter(JNIEnv*, jclass) {
    ATrace_setCounter("ndk::counter", 10);
    ATrace_setCounter("ndk::counter", 20);
    ATrace_setCounter("ndk::counter", 30);
    ATrace_setCounter("ndk::counter", 9223372000000005807L);
}

static JNINativeMethod gMethods[] = {
    { "isEnabled", "()Z", (void*) isEnabled },
    { "beginEndSection", "()V", (void*) beginEndSection },
    { "asyncBeginEndSection", "()V", (void*) asyncBeginEndSection },
    { "counter", "()V", (void*) counter },
};

jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    JNIEnv* env = nullptr;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
        return JNI_ERR;
    }
    jclass clazz = env->FindClass("com/android/cts/atracetestapp/AtraceNdkMethods");
    env->RegisterNatives(clazz, gMethods, sizeof(gMethods) / sizeof(JNINativeMethod));
    return JNI_VERSION_1_4;
}
