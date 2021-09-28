/*
 * Copyright (C) 2010 The Android Open Source Project
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
#include <cpu-features.h>
#include <jni.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/utsname.h>

#include <string>

jboolean android_cts_CpuFeatures_isArmCpu(JNIEnv* env, jobject thiz)
{
    AndroidCpuFamily cpuFamily = android_getCpuFamily();
    return cpuFamily == ANDROID_CPU_FAMILY_ARM;
}

jboolean android_cts_CpuFeatures_isMipsCpu(JNIEnv* env, jobject thiz)
{
    AndroidCpuFamily cpuFamily = android_getCpuFamily();
    return cpuFamily == ANDROID_CPU_FAMILY_MIPS;
}

jboolean android_cts_CpuFeatures_isX86Cpu(JNIEnv* env, jobject thiz)
{
    AndroidCpuFamily cpuFamily = android_getCpuFamily();
    return cpuFamily == ANDROID_CPU_FAMILY_X86;
}

jboolean android_cts_CpuFeatures_isArm64Cpu(JNIEnv* env, jobject thiz)
{
    AndroidCpuFamily cpuFamily = android_getCpuFamily();
    return cpuFamily == ANDROID_CPU_FAMILY_ARM64;
}

jboolean android_cts_CpuFeatures_isMips64Cpu(JNIEnv* env, jobject thiz)
{
    AndroidCpuFamily cpuFamily = android_getCpuFamily();
    return cpuFamily == ANDROID_CPU_FAMILY_MIPS64;
}

jboolean android_cts_CpuFeatures_isX86_64Cpu(JNIEnv* env, jobject thiz)
{
    AndroidCpuFamily cpuFamily = android_getCpuFamily();
    return cpuFamily == ANDROID_CPU_FAMILY_X86_64;
}

jint android_cts_CpuFeatures_getHwCaps(JNIEnv*, jobject)
{
    return (jint)getauxval(AT_HWCAP);
}

jboolean android_cts_CpuFeatures_isNativeBridgedCpu(JNIEnv* env, jobject thiz)
{
#if defined(__arm__) || defined(__aarch64__)
  // If the test is compiled for arm use uname() to check if host CPU is x86.
  struct utsname uname_data;
  uname(&uname_data);
  std::string machine = uname_data.machine;
  // Matches all of i386, i686 and x86_64.
  return machine.find("86") != std::string::npos;
#else
  return false;
#endif
}

static JNINativeMethod gMethods[] = {
    {  "isArmCpu", "()Z",
            (void *) android_cts_CpuFeatures_isArmCpu  },
    {  "isMipsCpu", "()Z",
            (void *) android_cts_CpuFeatures_isMipsCpu  },
    {  "isX86Cpu", "()Z",
            (void *) android_cts_CpuFeatures_isX86Cpu  },
    {  "isArm64Cpu", "()Z",
            (void *) android_cts_CpuFeatures_isArm64Cpu  },
    {  "isMips64Cpu", "()Z",
            (void *) android_cts_CpuFeatures_isMips64Cpu  },
    {  "isX86_64Cpu", "()Z",
            (void *) android_cts_CpuFeatures_isX86_64Cpu  },
    {  "getHwCaps", "()I",
            (void *) android_cts_CpuFeatures_getHwCaps  },
    {  "isNativeBridgedCpu", "()Z",
            (void *) android_cts_CpuFeatures_isNativeBridgedCpu  },
};

int register_android_cts_CpuFeatures(JNIEnv* env)
{
    jclass clazz = env->FindClass("com/android/compatibility/common/util/CpuFeatures");

    return env->RegisterNatives(clazz, gMethods,
            sizeof(gMethods) / sizeof(JNINativeMethod));
}
