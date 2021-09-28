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

#ifndef LIBNATIVEHELPER_INCLUDE_NATIVEHELPER_LIBNATIVEHELPER_API_H_
#define LIBNATIVEHELPER_INCLUDE_NATIVEHELPER_LIBNATIVEHELPER_API_H_

#include <stddef.h>

#include "jni.h"

// This is the stable C API exported by libnativehelper.

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/* ------------------------------------ C API for JNIHelp.h ------------------------------------- */

/*
 * Register one or more native methods with a particular class.  "className" looks like
 * "java/lang/String". Aborts on failure, returns 0 on success.
 */
int jniRegisterNativeMethods(C_JNIEnv* env,
                             const char* className,
                             const JNINativeMethod* gMethods,
                             int numMethods);

/*
 * Throw an exception with the specified class and an optional message.
 *
 * The "className" argument will be passed directly to FindClass, which
 * takes strings with slashes (e.g. "java/lang/Object").
 *
 * If an exception is currently pending, we log a warning message and
 * clear it.
 *
 * Returns 0 on success, nonzero if something failed (e.g. the exception
 * class couldn't be found, so *an* exception will still be pending).
 *
 * Currently aborts the VM if it can't throw the exception.
 */
int jniThrowException(C_JNIEnv* env, const char* className, const char* msg);

/*
 * Throw an exception with the specified class and formatted error message.
 *
 * The "className" argument will be passed directly to FindClass, which
 * takes strings with slashes (e.g. "java/lang/Object").
 *
 * If an exception is currently pending, we log a warning message and
 * clear it.
 *
 * Returns 0 on success, nonzero if something failed (e.g. the exception
 * class couldn't be found, so *an* exception will still be pending).
 *
 * Currently aborts the VM if it can't throw the exception.
 */
int jniThrowExceptionFmt(C_JNIEnv* env, const char* className, const char* fmt, va_list args);

/*
 * Throw a java.lang.NullPointerException, with an optional message.
 */
int jniThrowNullPointerException(C_JNIEnv* env, const char* msg);

/*
 * Throw a java.lang.RuntimeException, with an optional message.
 */
int jniThrowRuntimeException(C_JNIEnv* env, const char* msg);

/*
 * Throw a java.io.IOException, generating the message from errno.
 */
int jniThrowIOException(C_JNIEnv* env, int errnum);

/*
 * Return a pointer to a locale-dependent error string explaining errno
 * value 'errnum'. The returned pointer may or may not be equal to 'buf'.
 * This function is thread-safe (unlike strerror) and portable (unlike
 * strerror_r).
 */
const char* jniStrError(int errnum, char* buf, size_t buflen);

/*
 * Returns a new java.io.FileDescriptor for the given int fd.
 */
jobject jniCreateFileDescriptor(C_JNIEnv* env, int fd);

/*
 * Returns the int fd from a java.io.FileDescriptor.
 */
int jniGetFDFromFileDescriptor(C_JNIEnv* env, jobject fileDescriptor);

/*
 * Sets the int fd in a java.io.FileDescriptor.  Throws java.lang.NullPointerException
 * if fileDescriptor is null.
 */
void jniSetFileDescriptorOfFD(C_JNIEnv* env, jobject fileDescriptor, int value);

/*
 * Returns the long ownerId from a java.io.FileDescriptor.
 */
jlong jniGetOwnerIdFromFileDescriptor(C_JNIEnv* env, jobject fileDescriptor);

/*
 * Gets the managed heap array backing a java.nio.Buffer instance.
 *
 * Returns nullptr if there is no array backing.
 *
 * This method performs a JNI call to java.nio.NIOAccess.getBaseArray().
 */
jarray jniGetNioBufferBaseArray(C_JNIEnv* env, jobject nioBuffer);

/*
 * Gets the offset in bytes from the start of the managed heap array backing the buffer.
 *
 * Returns 0 if there is no array backing.
 *
 * This method performs a JNI call to java.nio.NIOAccess.getBaseArrayOffset().
 */
jint jniGetNioBufferBaseArrayOffset(C_JNIEnv* env, jobject nioBuffer);

/*
 * Gets field information from a java.nio.Buffer instance.
 *
 * Reads the |position|, |limit|, and |elementSizeShift| fields from the buffer instance.
 *
 * Returns the |address| field of the java.nio.Buffer instance which is only valid (non-zero) when
 * the buffer is backed by a direct buffer.
 */
jlong jniGetNioBufferFields(C_JNIEnv* env,
                            jobject nioBuffer,
                            /*out*/jint* position,
                            /*out*/jint* limit,
                            /*out*/jint* elementSizeShift);

/*
 * Gets the current position from a java.nio.Buffer as a pointer to memory in a fixed buffer.
 *
 * Returns 0 if |nioBuffer| is not backed by a direct buffer.
 *
 * This method reads the |address|, |position|, and |elementSizeShift| fields from the
 * java.nio.Buffer instance to calculate the pointer address for the current position.
 */
jlong jniGetNioBufferPointer(C_JNIEnv* env, jobject nioBuffer);

/*
 * Returns the reference from a java.lang.ref.Reference.
 */
jobject jniGetReferent(C_JNIEnv* env, jobject ref);

/*
 * Returns a Java String object created from UTF-16 data either from jchar or,
 * if called from C++11, char16_t (a bitwise identical distinct type).
 */
jstring jniCreateString(C_JNIEnv* env, const jchar* unicodeChars, jsize len);

/*
 * Log a message and an exception.
 * If exception is NULL, logs the current exception in the JNI environment.
 */
void jniLogException(C_JNIEnv* env, int priority, const char* tag, jthrowable exception);

/*
 * Clear the cache of constants libnativehelper is using.
 */
void jniUninitializeConstants();

/* ---------------------------------- C API for JniInvocation.h --------------------------------- */

/*
 * The JNI invocation API exists to allow a choice of library responsible for managing virtual
 * machines.
 */

/*
 * Opaque structure used to hold JNI invocation internal state.
 */
struct JniInvocationImpl;

/*
 * Creates an instance of a JniInvocationImpl.
 */
struct JniInvocationImpl* JniInvocationCreate();

/*
 * Associates a library with a JniInvocationImpl instance. The library should export C symbols for
 * JNI_GetDefaultJavaVMInitArgs, JNI_CreateJavaVM and JNI_GetDefaultJavaVMInitArgs.
 *
 * The specified |library| should be the filename of a shared library. The |library| is opened with
 * dlopen(3).
 *
 * If there is an error opening the specified |library|, then function will fallback to the
 * default library "libart.so". If the fallback library is successfully used then a warning is
 * written to the Android log buffer. Use of the fallback library is not considered an error.
 *
 * If the fallback library cannot be opened or the expected symbols are not found in the library
 * opened, then an error message is written to the Android log buffer and the function returns 0.
 *
 * Returns 1 on success, 0 otherwise.
 */
int JniInvocationInit(struct JniInvocationImpl* instance, const char* library);

/*
 * Release resources associated with JniInvocationImpl instance.
 */
void JniInvocationDestroy(struct JniInvocationImpl* instance);

/*
 * Gets the default library for JNI invocation. The default library is "libart.so". This value may
 * be overridden for debuggable builds using the persist.sys.dalvik.vm.lib.2 system property.
 *
 * The |library| argument is the preferred library to use on debuggable builds (when
 * ro.debuggable=1). If the |library| argument is nullptr, then the system preferred value will be
 * queried from persist.sys.dalvik.vm.lib.2 if the caller has provided |buffer| argument.
 *
 * The |buffer| argument is used for reading system properties in debuggable builds. It is
 * optional, but should be provisioned to be PROP_VALUE_MAX bytes if provided to ensure it is
 * large enough to hold a system property.
 *
 * Returns the filename of the invocation library determined from the inputs and system
 * properties. The returned value may be |library|, |buffer|, or a pointer to a string constant
 * "libart.so".
 */
const char* JniInvocationGetLibrary(const char* library, char* buffer);

/* ---------------------------------- C API for toStringArray.h --------------------------------- */

/*
 * Allocates a new array for java/lang/String instances with space for |count| elements. Elements
 * are initially null.
 *
 * Returns a new array on success or nullptr in case of failure. This method raises an
 * OutOfMemoryError exception if allocation fails.
 */
jobjectArray newStringArray(JNIEnv* env, size_t count);

/*
 * Converts an array of C strings into a managed array of Java strings. The size of the C array is
 * determined by the presence of a final element containing a nullptr.
 *
 * Returns a new array on success or nullptr in case of failure. This method raises an
 * OutOfMemoryError exception if allocation fails.
 */
jobjectArray toStringArray(JNIEnv* env, const char* const* strings);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // LIBNATIVEHELPER_INCLUDE_NATIVEHELPER_LIBNATIVEHELPER_API_H_
