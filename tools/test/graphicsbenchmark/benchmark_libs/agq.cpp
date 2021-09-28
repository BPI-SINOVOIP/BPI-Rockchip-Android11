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

#include "agq.h"
#include <jni.h>
#include <android/log.h>
#include <time.h>

#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, "AndroidGameQualification", __VA_ARGS__))

static JavaVM* sJavaVm = nullptr;
static JNIEnv* sJniEnv = nullptr;

// Initialize sJavaVm if the app uses JNI.
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
    sJavaVm = vm;
    sJavaVm->GetEnv(reinterpret_cast<void**>(&sJniEnv), JNI_VERSION_1_6);
    return sJniEnv->GetVersion();
}

namespace android {

static const char* const INTENT_START = "com.android.game.qualification.START";

static JNIEnv* getJniEnv() {
    if (sJniEnv == nullptr) {
        sJavaVm->AttachCurrentThread(&sJniEnv, nullptr);
    }
    return sJniEnv;
}

// Convert timespec to milliseconds.
jlong timespecToMs(timespec spec) {
    return jlong(spec.tv_sec) * 1000 + spec.tv_nsec / 1000000;
}

// Create an Intent jobject in Java.
static jobject createIntent() {
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    JNIEnv* env = getJniEnv();
    jclass intentClass = env->FindClass("android/content/Intent");
    jmethodID constructor = env->GetMethodID(intentClass, "<init>", "(Ljava/lang/String;)V");
    jobject intent = env->NewObject(intentClass, constructor, env->NewStringUTF(INTENT_START));
    jmethodID set_type =
            env->GetMethodID(
                intentClass,
                "setType",
                "(Ljava/lang/String;)Landroid/content/Intent;");
    env->CallObjectMethod(intent, set_type, env->NewStringUTF("text/plain"));
    jmethodID put_extra =
            env->GetMethodID(
                intentClass,
                "putExtra",
                "(Ljava/lang/String;J)Landroid/content/Intent;");

    jlong timestamp = timespecToMs(spec);
    env->CallObjectMethod(intent, put_extra, env->NewStringUTF("timestamp"), timestamp);

    LOGD("Created intent %s at %lld", INTENT_START, (long long)timestamp);
    return intent;
}

// Implementation of android::GameQualification::Impl.

class GameQualification::Impl {
public:
    void startLoop(jobject context);
    void startLoop(ANativeActivity* activity);
};


void GameQualification::Impl::startLoop(jobject context) {
    JNIEnv* env = getJniEnv();
    jclass contextClass = env->FindClass("android/content/Context");
    jmethodID method =
            env->GetMethodID(
                contextClass,
                "sendBroadcast",
                "(Landroid/content/Intent;)V");
    env->CallVoidMethod(context, method, createIntent());
}

void GameQualification::Impl::startLoop(ANativeActivity* activity) {
    sJavaVm = activity->vm;
    startLoop(activity->clazz);
}

/* Implementation of GameQualification */

GameQualification::GameQualification() {
    mImpl = new GameQualification::Impl();
}

GameQualification::~GameQualification() {
    delete mImpl;
}

void GameQualification::startLoop(jobject context) {
    mImpl->startLoop(context);
}

void GameQualification::startLoop(ANativeActivity* activity) {
    mImpl->startLoop(activity);
}

} // end of namespace android
