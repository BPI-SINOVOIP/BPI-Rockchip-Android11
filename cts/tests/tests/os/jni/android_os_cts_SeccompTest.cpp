/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include <android/log.h>
#include <jni.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(ARCH_SUPPORTS_SECCOMP)
#include <libminijail.h>
#include <seccomp_bpf_tests.h>
#endif

jboolean android_security_cts_SeccompBpfTest_runKernelUnitTest(
      JNIEnv* env, jobject thiz __unused, jstring name) {
#if defined(ARCH_SUPPORTS_SECCOMP)
    const char* nameStr = env->GetStringUTFChars(name, nullptr);
    return run_seccomp_test(nameStr);
#endif  // ARCH_SUPPORTS_SECCOMP

    return false;
}

jboolean android_security_cts_SeccompBpfTest_nativeInstallTestFilter(
        JNIEnv*, jclass, jint policyFd) {
#if !defined(ARCH_SUPPORTS_SECCOMP)
    return false;
#else
    minijail* j = minijail_new();
    minijail_no_new_privs(j);
    minijail_use_seccomp_filter(j);
    minijail_set_seccomp_filter_tsync(j);
    minijail_parse_seccomp_filters_from_fd(j, policyFd);
    minijail_enter(j);
    minijail_destroy(j);

    close(policyFd);
    return true;
#endif
}

jstring android_security_cts_SeccompBpfTest_getPolicyAbiString(JNIEnv* env, jclass) {
    const char* string;
#if defined(__arm__)
    string = "arm";
#elif defined(__aarch64__)
    string = "arm64";
#elif defined(__i386__)
    string = "i386";
#elif defined(__x86_64__)
    string = "x86-64";
#else
    return nullptr;
#endif
    return env->NewStringUTF(string);
}

jint android_security_cts_SeccompBpfTest_getClockBootTime(JNIEnv*, jclass) {
    struct timespec ts;
    int rv = clock_gettime(CLOCK_BOOTTIME_ALARM, &ts);
    return rv;
}

static JNINativeMethod methods[] = {
    { "runKernelUnitTest", "(Ljava/lang/String;)Z",
        (void*)android_security_cts_SeccompBpfTest_runKernelUnitTest },
    { "nativeInstallTestFilter", "(I)Z",
        (void*)android_security_cts_SeccompBpfTest_nativeInstallTestFilter },
    { "getPolicyAbiString", "()Ljava/lang/String;",
        (void*)android_security_cts_SeccompBpfTest_getPolicyAbiString },
    { "getClockBootTime", "()I",
        (void*)android_security_cts_SeccompBpfTest_getClockBootTime },
};

int register_android_os_cts_SeccompTest(JNIEnv* env) {
    jclass clazz = env->FindClass("android/os/cts/SeccompTest");
    return env->RegisterNatives(clazz, methods, sizeof(methods) / sizeof(JNINativeMethod));
}
