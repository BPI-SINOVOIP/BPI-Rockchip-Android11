/*
 * Copyright 2018 Rockchip Electronics Co. LTD
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
 *
 */

#define LOG_TAG "RkNativeAudioSetting"
#include "android/graphics/Bitmap.h"

#include <nativehelper/JNIHelp.h>
#include <nativehelper/ScopedUtfChars.h>
#include <jni.h>
#include <memory>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <inttypes.h>
#include <sstream>

#include <linux/netlink.h>
#include <sys/socket.h>


#include <rksoundsetting/RkAudioSetting.h>

#include <unordered_map>

namespace android {


static RkAudioSetting *mAudioSetting = nullptr;

///////////////////////////////////////////////////////////////////////////////////////////////

RkAudioSetting* getAudioSetting() {
    if (mAudioSetting == nullptr) {
        mAudioSetting = new RkAudioSetting();
        if (mAudioSetting != nullptr) {
        } else {
            ALOGD("get mAudioSetting is NULL");
        }
    }
    return mAudioSetting;
}

static void nativeSetSelect(JNIEnv* env, jobject obj, jint device) {
    RkAudioSetting *setting = getAudioSetting();

    if (setting != nullptr) {
        setting->setSelect(device);
    } else {
        ALOGD("get mAudioSetting is NULL");
    }
}

static void nativeupdataFormatForEdid(JNIEnv* env, jobject obj) {
    RkAudioSetting *setting = getAudioSetting();

    if (setting != nullptr) {
        setting->updataFormatForEdid();
    } else {
        ALOGD("get mAudioSetting is NULL");
    }
}

static void nativeSetFormat(JNIEnv* env, jobject obj, jint device, jint close, jstring format) {
    const char* mformat = env->GetStringUTFChars(format, NULL);
    RkAudioSetting *setting = getAudioSetting();

    if (setting != nullptr) {
        setting->setFormat(device, close, mformat);
    } else {
        ALOGD("get mAudioSetting is NULL");
    }
    env->ReleaseStringUTFChars(format, mformat);
}

static void nativeSetMode(JNIEnv* env, jobject obj, jint device, jint mode) {
    RkAudioSetting *setting = getAudioSetting();

    if (setting != nullptr){
        setting->setMode(device, mode);
    } else {
        ALOGD("get mAudioSetting is NULL");
    }
}

static jint nativeGetSelect(JNIEnv* env, jobject obj, jint device) {
    int value = 0;
    RkAudioSetting *setting = getAudioSetting();

    if (setting != nullptr) {
        value = setting->getSelect(device);
    } else {
         ALOGD("get mAudioSetting is NULL");
    }

    return static_cast<jint>(value);
}

static jint nativeGetFormat(JNIEnv* env, jobject obj, jint device, jstring format) {
    int value = 0;
    const char* mformat = env->GetStringUTFChars(format, NULL);
    RkAudioSetting *setting = getAudioSetting();

    if (setting != nullptr) {
        value = setting->getFormat(device, mformat);
    } else {
        ALOGD("get mAudioSetting is NULL");
    }

    env->ReleaseStringUTFChars(format, mformat);
    return static_cast<jint>(value);
}

static jint nativeGetMode(JNIEnv* env, jobject obj, jint device) {
    int value = 0;
    RkAudioSetting *setting = getAudioSetting();

    if (setting != nullptr) {
        value = setting->getMode(device);
    } else {
        ALOGD("get mAudioSetting is NULL");
    }

    return static_cast<jint>(value);
}

// ----------------------------------------------------------------------------
//com.android.server.AudioSetting
static const JNINativeMethod sRkAudioSettingMethods[] = {
    {"nativeGetSelect", "(I)I",
        (void*)nativeGetSelect},
    {"nativeGetMode", "(I)I",
        (void*) nativeGetMode},
    {"nativeGetFormat", "(ILjava/lang/String;)I",
        (void*) nativeGetFormat},

    {"nativeSetFormat", "(IILjava/lang/String;)V",
        (void*)nativeSetFormat},
    {"nativeSetMode", "(II)V",
        (void*)nativeSetMode},
    {"nativeSetSelect", "(I)V",
        (void*)nativeSetSelect},
    {"nativeupdataFormatForEdid", "()V",
        (void*)nativeupdataFormatForEdid},
};

#define FIND_CLASS(var, className) \
    var = env->FindClass(className); \
    LOG_FATAL_IF(! var, "Unable to find class " className);

#define GET_METHOD_ID(var, clazz, methodName, methodDescriptor) \
    var = env->GetMethodID(clazz, methodName, methodDescriptor); \
    LOG_FATAL_IF(! var, "Unable to find method " methodName);

#define GET_FIELD_ID(var, clazz, fieldName, fieldDescriptor) \
    var = env->GetFieldID(clazz, fieldName, fieldDescriptor); \
    LOG_FATAL_IF(! var, "Unable to find field " fieldName);

int register_com_android_server_audio_RkAudioSetting(JNIEnv* env)
{
    int res = jniRegisterNativeMethods(env, "com/android/server/audio/RkAudioSetting",
            sRkAudioSettingMethods, NELEM(sRkAudioSettingMethods));
    LOG_FATAL_IF(res < 0, "Unable to register native methods register_com_android_server_audio_RkAudioSetting");
    (void)res; // Don't complain about unused variable in the LOG_NDEBUG case

    jclass clazz;
    FIND_CLASS(clazz, "com/android/server/audio/RkAudioSetting");

    return 0;
}
}
