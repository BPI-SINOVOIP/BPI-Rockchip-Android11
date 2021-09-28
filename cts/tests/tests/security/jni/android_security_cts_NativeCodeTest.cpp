/*
 * Copyright (C) 2013 The Android Open Source Project
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
#include <linux/futex.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <cutils/log.h>
#include <linux/perf_event.h>
#include <errno.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <linux/ipc.h>
#include <pthread.h>
#include <sys/uio.h>

#define SHMEMSIZE 0x1 /* request one page */
static jboolean android_security_cts_NativeCodeTest_doSysVipcTest(JNIEnv*, jobject)
{
#if defined(__i386__) || (_MIPS_SIM == _MIPS_SIM_ABI32)
    /* system call does not exist for x86 or mips 32 */
    return true;
#else
    key_t key = 0x1a25;

    /*
     * Not supported in bionic. Must directly invoke syscall
     * Only acceptable errno is ENOSYS: shmget syscall
     * function not implemented
     */
    return ((syscall(SYS_shmget, key, SHMEMSIZE, IPC_CREAT | 0666) == -1)
                && (errno == ENOSYS));
#endif
}

static JNINativeMethod gMethods[] = {
    {  "doSysVipcTest", "()Z",
            (void *) android_security_cts_NativeCodeTest_doSysVipcTest },
};

int register_android_security_cts_NativeCodeTest(JNIEnv* env)
{
    jclass clazz = env->FindClass("android/security/cts/NativeCodeTest");
    return env->RegisterNatives(clazz, gMethods,
            sizeof(gMethods) / sizeof(JNINativeMethod));
}
