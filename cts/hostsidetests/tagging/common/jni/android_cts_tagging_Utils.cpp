/*
 * Copyright (C) 2020 The Android Open Source Project
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

/*
 * Native implementation for the JniStaticTest parts.
 */

#include <jni.h>
#include <stdlib.h>
#include <sys/prctl.h>

extern "C" JNIEXPORT jboolean
Java_android_cts_tagging_Utils_kernelSupportsTaggedPointers() {
#ifdef __aarch64__
#define PR_SET_TAGGED_ADDR_CTRL 55
#define PR_TAGGED_ADDR_ENABLE (1UL << 0)
  int res = prctl(PR_GET_TAGGED_ADDR_CTRL, 0, 0, 0, 0);
  return res >= 0 && res & PR_TAGGED_ADDR_ENABLE;
#else
  return false;
#endif
}

extern "C" JNIEXPORT jint JNICALL
Java_android_cts_tagging_Utils_nativeHeapPointerTag(JNIEnv *) {
#ifdef __aarch64__
  void *p = malloc(10);
  jint tag = reinterpret_cast<uintptr_t>(p) >> 56;
  free(p);
  return tag;
#else
  return 0;
#endif
}
