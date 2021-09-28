/*
 * Copyright (C) 2012 The Android Open Source Project
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
#include <stdio.h>

extern int register_android_security_cts_LinuxRngTest(JNIEnv*);
extern int register_android_security_cts_NativeCodeTest(JNIEnv*);
extern int register_android_security_cts_SeccompTest(JNIEnv*);
extern int register_android_security_cts_MMapExecutableTest(JNIEnv* env);
extern int register_android_security_cts_EncryptionTest(JNIEnv* env);
extern int register_android_security_cts_cve_2021_0394(JNIEnv* env);

jint JNI_OnLoad(JavaVM *vm, void *) {
    JNIEnv *env = NULL;

    if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) != JNI_OK) {
        return JNI_ERR;
    }

    if (register_android_security_cts_LinuxRngTest(env)) {
        return JNI_ERR;
    }

    if (register_android_security_cts_NativeCodeTest(env)) {
        return JNI_ERR;
    }

    if (register_android_security_cts_MMapExecutableTest(env)) {
        return JNI_ERR;
    }

    if (register_android_security_cts_EncryptionTest(env)) {
        return JNI_ERR;
    }

    if (register_android_security_cts_cve_2021_0394(env)) {
        return JNI_ERR;
    }

    return JNI_VERSION_1_4;
}
