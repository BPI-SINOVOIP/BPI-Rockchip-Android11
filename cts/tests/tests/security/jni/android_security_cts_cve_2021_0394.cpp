/*
 * Copyright (C) 2021 The Android Open Source Project
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
#include <string.h>

static constexpr char kDefaultValue = 'x';
static constexpr size_t kStringLength = 4096;

jboolean android_security_cts_cve_2021_0394_poc(JNIEnv* env, jobject thiz) {
    (void) thiz;
    char array[kStringLength];
    memset(array, kDefaultValue, kStringLength - 1);
    array[kStringLength - 1] = 0;
    jstring s = env->NewStringUTF(array);
    jboolean isVulnerable = false;
    char invalidChars[] = { '\xc0', '\xe0', '\xf0' };
    for (int c = 0; c < sizeof(invalidChars) / sizeof(char); ++c) {
        char invalid = invalidChars[c];
        array[kStringLength - 2] = invalid;
        s = env->NewStringUTF(array);
        const char* UTFChars = env->GetStringUTFChars(s, nullptr);
        if (UTFChars[kStringLength - 2] != '?') {
            isVulnerable = true;
        }
        env->ReleaseStringUTFChars(s, UTFChars);
    }
    return isVulnerable;
}

static JNINativeMethod gMethods[] = { { "poc", "()Z",
        (void*) android_security_cts_cve_2021_0394_poc }, };

int register_android_security_cts_cve_2021_0394(JNIEnv* env) {
    jclass clazz = env->FindClass("android/security/cts/CVE_2021_0394");
    return env->RegisterNatives(clazz, gMethods,
                                sizeof(gMethods) / sizeof(JNINativeMethod));
}
