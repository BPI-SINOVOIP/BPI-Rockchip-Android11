/*
 * Copyright (C) 2007 The Android Open Source Project
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
 * JNI helper functions.
 *
 * This file may be included by C or C++ code, which is trouble because jni.h
 * uses different typedefs for JNIEnv in each language.
 */
#ifndef LIBNATIVEHELPER_INCLUDE_NATIVEHELPER_JNIHELP_H_
#define LIBNATIVEHELPER_INCLUDE_NATIVEHELPER_JNIHELP_H_

#include <errno.h>
#include <unistd.h>

#include "libnativehelper_api.h"

#ifndef NELEM
#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))
#endif

/*
 * For C++ code, we provide inlines that map to the C functions.  g++ always
 * inlines these, even on non-optimized builds.
 */
#if defined(__cplusplus)

inline int jniRegisterNativeMethods(JNIEnv* env, const char* className, const JNINativeMethod* gMethods, int numMethods) {
    return jniRegisterNativeMethods(&env->functions, className, gMethods, numMethods);
}

inline int jniThrowException(JNIEnv* env, const char* className, const char* msg) {
    return jniThrowException(&env->functions, className, msg);
}

/*
 * Equivalent to jniThrowException but with a printf-like format string and
 * variable-length argument list. This is only available in C++.
 */
inline int jniThrowExceptionFmt(JNIEnv* env, const char* className, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    return jniThrowExceptionFmt(&env->functions, className, fmt, args);
    va_end(args);
}

inline int jniThrowNullPointerException(JNIEnv* env, const char* msg) {
    return jniThrowNullPointerException(&env->functions, msg);
}

inline int jniThrowRuntimeException(JNIEnv* env, const char* msg) {
    return jniThrowRuntimeException(&env->functions, msg);
}

inline int jniThrowIOException(JNIEnv* env, int errnum) {
    return jniThrowIOException(&env->functions, errnum);
}

inline jobject jniCreateFileDescriptor(JNIEnv* env, int fd) {
    return jniCreateFileDescriptor(&env->functions, fd);
}

inline int jniGetFDFromFileDescriptor(JNIEnv* env, jobject fileDescriptor) {
    return jniGetFDFromFileDescriptor(&env->functions, fileDescriptor);
}

inline void jniSetFileDescriptorOfFD(JNIEnv* env, jobject fileDescriptor, int value) {
    jniSetFileDescriptorOfFD(&env->functions, fileDescriptor, value);
}

inline jlong jniGetOwnerIdFromFileDescriptor(JNIEnv* env, jobject fileDescriptor) {
    return jniGetOwnerIdFromFileDescriptor(&env->functions, fileDescriptor);
}

inline jarray jniGetNioBufferBaseArray(JNIEnv* env, jobject nioBuffer) {
    return jniGetNioBufferBaseArray(&env->functions, nioBuffer);
}

inline jint jniGetNioBufferBaseArrayOffset(JNIEnv* env, jobject nioBuffer) {
    return jniGetNioBufferBaseArrayOffset(&env->functions, nioBuffer);
}

inline jlong jniGetNioBufferFields(JNIEnv* env, jobject nioBuffer,
                                   jint* position, jint* limit, jint* elementSizeShift) {
    return jniGetNioBufferFields(&env->functions, nioBuffer,
                                 position, limit, elementSizeShift);
}

inline jlong jniGetNioBufferPointer(JNIEnv* env, jobject nioBuffer) {
    return jniGetNioBufferPointer(&env->functions, nioBuffer);
}

inline jobject jniGetReferent(JNIEnv* env, jobject ref) {
    return jniGetReferent(&env->functions, ref);
}

inline jstring jniCreateString(JNIEnv* env, const jchar* unicodeChars, jsize len) {
    return jniCreateString(&env->functions, unicodeChars, len);
}

inline jstring jniCreateString(JNIEnv* env, const char16_t* unicodeChars, jsize len) {
    return jniCreateString(&env->functions, reinterpret_cast<const jchar*>(unicodeChars), len);
}

inline void jniLogException(JNIEnv* env, int priority, const char* tag, jthrowable exception = NULL) {
    jniLogException(&env->functions, priority, tag, exception);
}

#endif  // defined(__cplusplus)

#endif  // LIBNATIVEHELPER_INCLUDE_NATIVEHELPER_JNIHELP_H_
