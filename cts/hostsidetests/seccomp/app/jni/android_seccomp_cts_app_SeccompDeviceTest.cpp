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

#include <jni.h>

#define LOG_TAG "SeccompTest"

#include <functional>
#include <android/log.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define ALOG(priority, tag, ...) ((void)__android_log_print(ANDROID_##priority, tag, __VA_ARGS__))

#define ALOGI(...) ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) ALOG(LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define PER_USER_RANGE  100000

/*
 * Function: testSyscallBlocked
 * Purpose: test that the syscall listed is blocked by seccomp
 * Parameters:
 *        nr: syscall number
 * Returns:
 *        1 if blocked, else 0
 * Exceptions: None
 */
static jboolean doTestSyscallBlocked(std::function<void()> execSyscall) {
    int pid = fork();
    if (pid == 0) {
        execSyscall();
        exit(0);
    } else {
        int status;
        int ret = waitpid(pid, &status, 0);
        if (ret != pid) {
            ALOGE("Unexpected return result from waitpid");
            return false;
        }

        if (WIFEXITED(status)) {
            ALOGE("syscall was not blocked");
            return false;
        }

        if (WIFSIGNALED(status)) {
            int signal = WTERMSIG(status);
            if (signal == 31) {
                ALOGI("syscall caused process termination");
                return true;
            }

            ALOGE("Unexpected signal");
            return false;
        }

        ALOGE("Unexpected status from syscall_exists");
        return false;
    }
}

static jboolean testSyscallBlocked(JNIEnv *, jobject, jint nr) {
    return doTestSyscallBlocked([&](){ ALOGI("Calling syscall %d", nr); syscall(nr); });
}

static jboolean testSetresuidBlocked(JNIEnv *, jobject, jint ruid, jint euid, jint suid) {
    jint userId = getuid() / PER_USER_RANGE;
    jint userRuid = userId * PER_USER_RANGE + ruid;
    jint userEuid = userId * PER_USER_RANGE + euid;
    jint userSuid = userId * PER_USER_RANGE + suid;

    return doTestSyscallBlocked([&] {ALOGE("Calling setresuid\n"); setresuid(userRuid, userEuid, userSuid);});
}

static jboolean testSetresgidBlocked(JNIEnv *, jobject, jint rgid, jint egid, jint sgid) {
    jint userId = getuid() / PER_USER_RANGE;
    jint userRgid = userId * PER_USER_RANGE + rgid;
    jint userEgid = userId * PER_USER_RANGE + egid;
    jint userSgid = userId * PER_USER_RANGE + sgid;

    return doTestSyscallBlocked([&] {ALOGE("Calling setresgid\n"); setresgid(userRgid, userEgid, userSgid);});
}

static JNINativeMethod gMethods[] = {
    { "testSyscallBlocked", "(I)Z",
            (void*) testSyscallBlocked },
    { "testSetresuidBlocked", "(III)Z",
            (void*) testSetresuidBlocked },
    { "testSetresgidBlocked", "(III)Z",
            (void*) testSetresgidBlocked },
};

int register_android_seccomp_cts_app_SeccompTest(JNIEnv* env)
{
    jclass clazz = env->FindClass("android/seccomp/cts/app/SeccompDeviceTest");

    return env->RegisterNatives(clazz, gMethods,
            sizeof(gMethods) / sizeof(JNINativeMethod));
}
