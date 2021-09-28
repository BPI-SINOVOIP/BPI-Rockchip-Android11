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

#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "nativehelper/scoped_utf_chars.h"

// Information about the Java StructTm class.
static struct {
    jclass clazz;
    jmethodID ctor;
    jfieldID tm_sec;
    jfieldID tm_min;
    jfieldID tm_hour;
    jfieldID tm_mday;
    jfieldID tm_mon;
    jfieldID tm_year;
    jfieldID tm_wday;
    jfieldID tm_yday;
    jfieldID tm_isdst;
    jfieldID tm_gmtoff;
    jfieldID tm_zone;
} gStructTmClassInfo;

// The following is not threadsafe but that's a problem with getenv(), setenv(), mktime() it
// would be impractical to get around. Luckily, tests are single threaded.
#define SET_TZ(jni_env, javaTzId) \
    char _originalTz[64]; \
    char* _tzEnv = getenv("TZ"); \
    if (_tzEnv != nullptr) { \
        strncpy(_originalTz, _tzEnv, sizeof(_originalTz)); \
        _originalTz[sizeof(_originalTz) - 1] = 0; \
    } \
    ScopedUtfChars _tzId(jni_env, javaTzId); \
    setenv("TZ", _tzId.c_str(), 1);

#define UNSET_TZ() \
    if (_tzEnv != nullptr) { \
        setenv("TZ", _originalTz, 1); \
    }


static jobject android_text_format_cts_NativeTimeFunctions_localtime_tz(
        JNIEnv* env, jclass, jint javaTimep, jstring javaTzId)
{
    // Call localtime_r
    struct tm out_tm;
    {
        SET_TZ(env, javaTzId);

        const time_t timep = javaTimep;
        localtime_r(&timep, &out_tm);

        UNSET_TZ();
    }

    // Copy the results to a StructTm object.
    jobject javaTm = env->NewObject(gStructTmClassInfo.clazz, gStructTmClassInfo.ctor);
    env->SetIntField(javaTm, gStructTmClassInfo.tm_sec, out_tm.tm_sec);
    env->SetIntField(javaTm, gStructTmClassInfo.tm_min, out_tm.tm_min);
    env->SetIntField(javaTm, gStructTmClassInfo.tm_hour, out_tm.tm_hour);
    env->SetIntField(javaTm, gStructTmClassInfo.tm_mday, out_tm.tm_mday);
    env->SetIntField(javaTm, gStructTmClassInfo.tm_mon, out_tm.tm_mon);
    env->SetIntField(javaTm, gStructTmClassInfo.tm_year, out_tm.tm_year);
    env->SetIntField(javaTm, gStructTmClassInfo.tm_wday, out_tm.tm_wday);
    env->SetIntField(javaTm, gStructTmClassInfo.tm_yday, out_tm.tm_yday);
    env->SetIntField(javaTm, gStructTmClassInfo.tm_isdst, out_tm.tm_isdst);
    env->SetLongField(javaTm, gStructTmClassInfo.tm_gmtoff, out_tm.tm_gmtoff);
    env->SetObjectField(javaTm, gStructTmClassInfo.tm_zone, env->NewStringUTF(out_tm.tm_zone));
    return javaTm;
}

static jint android_text_format_cts_NativeTimeFunctions_mktime_tz(
        JNIEnv* env, jclass, jobject javaTm, jstring javaTzId)
{
    // Populate a tm struct from the Java StructTm object.
    struct tm in_tm;
    in_tm.tm_sec = env->GetIntField(javaTm, gStructTmClassInfo.tm_sec);
    in_tm.tm_min = env->GetIntField(javaTm, gStructTmClassInfo.tm_min);
    in_tm.tm_hour = env->GetIntField(javaTm, gStructTmClassInfo.tm_hour);
    in_tm.tm_mday = env->GetIntField(javaTm, gStructTmClassInfo.tm_mday);
    in_tm.tm_mon = env->GetIntField(javaTm, gStructTmClassInfo.tm_mon);
    in_tm.tm_year = env->GetIntField(javaTm, gStructTmClassInfo.tm_year);
    in_tm.tm_wday = env->GetIntField(javaTm, gStructTmClassInfo.tm_wday);
    in_tm.tm_yday = env->GetIntField(javaTm, gStructTmClassInfo.tm_yday);
    in_tm.tm_isdst = env->GetIntField(javaTm, gStructTmClassInfo.tm_isdst);
    in_tm.tm_gmtoff = env->GetLongField(javaTm, gStructTmClassInfo.tm_gmtoff);

    jstring javaTmZone =
            static_cast<jstring>(env->GetObjectField(javaTm, gStructTmClassInfo.tm_zone));
    const char* tmZone = nullptr;
    if (javaTmZone != nullptr) {
        tmZone = env->GetStringUTFChars(javaTmZone, nullptr);
        in_tm.tm_zone = tmZone;
    }

    // Call mktime
    jint result = -1;
    {
        SET_TZ(env, javaTzId);

        result = mktime(&in_tm);

        UNSET_TZ();
    }

    if (javaTmZone != nullptr) {
        env->ReleaseStringUTFChars(javaTmZone, tmZone);
    }

    return result;
}

static JNINativeMethod gMethods[] = {
    { "localtime_tz", "(ILjava/lang/String;)Landroid/text/format/cts/NativeTimeFunctions$StructTm;",
        (void *) android_text_format_cts_NativeTimeFunctions_localtime_tz },
    { "mktime_tz", "(Landroid/text/format/cts/NativeTimeFunctions$StructTm;Ljava/lang/String;)I",
        (void *) android_text_format_cts_NativeTimeFunctions_mktime_tz },
};

int register_android_text_format_cts_NativeTimeFunctions(JNIEnv* env)
{
    jclass javaTmClass = env->FindClass("android/text/format/cts/NativeTimeFunctions$StructTm");
    gStructTmClassInfo.clazz = (jclass) env->NewGlobalRef(javaTmClass);
    gStructTmClassInfo.ctor = env->GetMethodID(javaTmClass, "<init>", "()V");
    gStructTmClassInfo.tm_sec = env->GetFieldID(javaTmClass, "tm_sec", "I");
    gStructTmClassInfo.tm_min = env->GetFieldID(javaTmClass, "tm_min", "I");
    gStructTmClassInfo.tm_hour = env->GetFieldID(javaTmClass, "tm_hour", "I");
    gStructTmClassInfo.tm_mday = env->GetFieldID(javaTmClass, "tm_mday", "I");
    gStructTmClassInfo.tm_mon = env->GetFieldID(javaTmClass, "tm_mon", "I");
    gStructTmClassInfo.tm_year = env->GetFieldID(javaTmClass, "tm_year", "I");
    gStructTmClassInfo.tm_wday = env->GetFieldID(javaTmClass, "tm_wday", "I");
    gStructTmClassInfo.tm_yday = env->GetFieldID(javaTmClass, "tm_yday", "I");
    gStructTmClassInfo.tm_isdst = env->GetFieldID(javaTmClass, "tm_isdst", "I");
    gStructTmClassInfo.tm_gmtoff = env->GetFieldID(javaTmClass, "tm_gmtoff", "J");
    gStructTmClassInfo.tm_zone = env->GetFieldID(javaTmClass, "tm_zone", "Ljava/lang/String;");

    jclass clazz = env->FindClass("android/text/format/cts/NativeTimeFunctions");
    return env->RegisterNatives(clazz, gMethods,
            sizeof(gMethods) / sizeof(JNINativeMethod));
}
