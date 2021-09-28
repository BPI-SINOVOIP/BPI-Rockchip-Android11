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
#define TAG "NativeMidiManager-JNI"

#include <android/log.h>
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

#include <amidi/AMidi.h>

#include "MidiTestManager.h"

static MidiTestManager sTestManager;

static bool DEBUG = false;

extern "C" {

void Java_com_android_cts_verifier_audio_midilib_NativeMidiManager_initN(
        JNIEnv* env, jobject midiTestModule) {

    sTestManager.jniSetup(env);
}

void Java_com_android_cts_verifier_audio_midilib_NativeMidiManager_startTest(
        JNIEnv* env, jobject thiz, jobject testModuleObj, jobject midiObj) {

    (void)thiz;

    if (DEBUG) {
        ALOGI("NativeMidiManager_startTest(%p, %p)", testModuleObj, midiObj);
    }

    media_status_t status;

    AMidiDevice* nativeMidiDevice = NULL;
    status = AMidiDevice_fromJava(env, midiObj, &nativeMidiDevice);
    if (DEBUG) {
        ALOGI("nativeSendDevice:%p, status:%d", nativeMidiDevice, status);
    }

    sTestManager.RunTest(testModuleObj, nativeMidiDevice, nativeMidiDevice);

    status = AMidiDevice_release(nativeMidiDevice);
    if (DEBUG) {
        ALOGI("device release status:%d", status);
    }
}

} // extern "C"
