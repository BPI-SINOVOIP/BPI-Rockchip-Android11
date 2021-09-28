/*
 * Copyright (C) 2006 The Android Open Source Project
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

#include "nativehelper/JNIHelp.h"

#include <string>

#define LOG_TAG "JNIHelp"
#include "ALog-priv.h"

#include "jni.h"
#include "JniConstants.h"

namespace {

/**
 * Equivalent to ScopedLocalRef, but for C_JNIEnv instead. (And slightly more powerful.)
 */
template<typename T>
class scoped_local_ref final {
public:
    explicit scoped_local_ref(C_JNIEnv* env, T localRef = NULL)
    : mEnv(env), mLocalRef(localRef)
    {
    }

    ~scoped_local_ref() {
        reset();
    }

    void reset(T localRef = NULL) {
        if (mLocalRef != NULL) {
            (*mEnv)->DeleteLocalRef(reinterpret_cast<JNIEnv*>(mEnv), mLocalRef);
            mLocalRef = localRef;
        }
    }

    T get() const {
        return mLocalRef;
    }

private:
    // scoped_local_ref does not support copy or move semantics.
    scoped_local_ref(const scoped_local_ref&) = delete;
    scoped_local_ref(scoped_local_ref&&) = delete;
    scoped_local_ref& operator=(const scoped_local_ref&) = delete;
    scoped_local_ref& operator=(scoped_local_ref&&) = delete;

private:
    C_JNIEnv* const mEnv;
    T mLocalRef;
};

jclass findClass(C_JNIEnv* env, const char* className) {
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);
    return (*env)->FindClass(e, className);
}

/*
 * Returns a human-readable summary of an exception object.  The buffer will
 * be populated with the "binary" class name and, if present, the
 * exception message.
 */
bool getExceptionSummary(C_JNIEnv* env, jthrowable exception, std::string& result) {
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);

    /* get the name of the exception's class */
    scoped_local_ref<jclass> exceptionClass(env, (*env)->GetObjectClass(e, exception)); // can't fail
    scoped_local_ref<jclass> classClass(env,
            (*env)->GetObjectClass(e, exceptionClass.get())); // java.lang.Class, can't fail
    jmethodID classGetNameMethod =
            (*env)->GetMethodID(e, classClass.get(), "getName", "()Ljava/lang/String;");
    scoped_local_ref<jstring> classNameStr(env,
            (jstring) (*env)->CallObjectMethod(e, exceptionClass.get(), classGetNameMethod));
    if (classNameStr.get() == NULL) {
        (*env)->ExceptionClear(e);
        result = "<error getting class name>";
        return false;
    }
    const char* classNameChars = (*env)->GetStringUTFChars(e, classNameStr.get(), NULL);
    if (classNameChars == NULL) {
        (*env)->ExceptionClear(e);
        result = "<error getting class name UTF-8>";
        return false;
    }
    result += classNameChars;
    (*env)->ReleaseStringUTFChars(e, classNameStr.get(), classNameChars);

    /* if the exception has a detail message, get that */
    jmethodID getMessage =
            (*env)->GetMethodID(e, exceptionClass.get(), "getMessage", "()Ljava/lang/String;");
    scoped_local_ref<jstring> messageStr(env,
            (jstring) (*env)->CallObjectMethod(e, exception, getMessage));
    if (messageStr.get() == NULL) {
        return true;
    }

    result += ": ";

    const char* messageChars = (*env)->GetStringUTFChars(e, messageStr.get(), NULL);
    if (messageChars != NULL) {
        result += messageChars;
        (*env)->ReleaseStringUTFChars(e, messageStr.get(), messageChars);
    } else {
        result += "<error getting message>";
        (*env)->ExceptionClear(e); // clear OOM
    }

    return true;
}

/*
 * Returns an exception (with stack trace) as a string.
 */
bool getStackTrace(C_JNIEnv* env, jthrowable exception, std::string& result) {
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);

    scoped_local_ref<jclass> stringWriterClass(env, findClass(env, "java/io/StringWriter"));
    if (stringWriterClass.get() == NULL) {
        return false;
    }

    jmethodID stringWriterCtor = (*env)->GetMethodID(e, stringWriterClass.get(), "<init>", "()V");
    jmethodID stringWriterToStringMethod =
            (*env)->GetMethodID(e, stringWriterClass.get(), "toString", "()Ljava/lang/String;");

    scoped_local_ref<jclass> printWriterClass(env, findClass(env, "java/io/PrintWriter"));
    if (printWriterClass.get() == NULL) {
        return false;
    }

    jmethodID printWriterCtor =
            (*env)->GetMethodID(e, printWriterClass.get(), "<init>", "(Ljava/io/Writer;)V");

    scoped_local_ref<jobject> stringWriter(env,
            (*env)->NewObject(e, stringWriterClass.get(), stringWriterCtor));
    if (stringWriter.get() == NULL) {
        return false;
    }

    scoped_local_ref<jobject> printWriter(env,
            (*env)->NewObject(e, printWriterClass.get(), printWriterCtor, stringWriter.get()));
    if (printWriter.get() == NULL) {
        return false;
    }

    scoped_local_ref<jclass> exceptionClass(env, (*env)->GetObjectClass(e, exception)); // can't fail
    jmethodID printStackTraceMethod =
            (*env)->GetMethodID(e, exceptionClass.get(), "printStackTrace", "(Ljava/io/PrintWriter;)V");
    (*env)->CallVoidMethod(e, exception, printStackTraceMethod, printWriter.get());

    if ((*env)->ExceptionCheck(e)) {
        return false;
    }

    scoped_local_ref<jstring> messageStr(env,
            (jstring) (*env)->CallObjectMethod(e, stringWriter.get(), stringWriterToStringMethod));
    if (messageStr.get() == NULL) {
        return false;
    }

    const char* utfChars = (*env)->GetStringUTFChars(e, messageStr.get(), NULL);
    if (utfChars == NULL) {
        return false;
    }

    result = utfChars;

    (*env)->ReleaseStringUTFChars(e, messageStr.get(), utfChars);
    return true;
}

std::string jniGetStackTrace(C_JNIEnv* env, jthrowable exception) {
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);

    scoped_local_ref<jthrowable> currentException(env, (*env)->ExceptionOccurred(e));
    if (exception == NULL) {
        exception = currentException.get();
        if (exception == NULL) {
          return "<no pending exception>";
        }
    }

    if (currentException.get() != NULL) {
        (*env)->ExceptionClear(e);
    }

    std::string trace;
    if (!getStackTrace(env, exception, trace)) {
        (*env)->ExceptionClear(e);
        getExceptionSummary(env, exception, trace);
    }

    if (currentException.get() != NULL) {
        (*env)->Throw(e, currentException.get()); // rethrow
    }

    return trace;
}

// Note: glibc has a nonstandard strerror_r that returns char* rather than POSIX's int.
// char *strerror_r(int errnum, char *buf, size_t n);
//
// Some versions of bionic support the glibc style call. Since the set of defines that determine
// which version is used is byzantine in its complexity we will just use this C++ template hack to
// select the correct jniStrError implementation based on the libc being used.

using GNUStrError = char* (*)(int,char*,size_t);
using POSIXStrError = int (*)(int,char*,size_t);

inline const char* realJniStrError(GNUStrError func, int errnum, char* buf, size_t buflen) {
    return func(errnum, buf, buflen);
}

inline const char* realJniStrError(POSIXStrError func, int errnum, char* buf, size_t buflen) {
    int rc = func(errnum, buf, buflen);
    if (rc != 0) {
        // (POSIX only guarantees a value other than 0. The safest
        // way to implement this function is to use C++ and overload on the
        // type of strerror_r to accurately distinguish GNU from POSIX.)
        snprintf(buf, buflen, "errno %d", errnum);
    }
    return buf;
}

}  // namespace

int jniRegisterNativeMethods(C_JNIEnv* env, const char* className,
    const JNINativeMethod* gMethods, int numMethods)
{
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);

    ALOGV("Registering %s's %d native methods...", className, numMethods);

    scoped_local_ref<jclass> c(env, findClass(env, className));
    ALOG_ALWAYS_FATAL_IF(c.get() == NULL,
                         "Native registration unable to find class '%s'; aborting...",
                         className);

    int result = e->RegisterNatives(c.get(), gMethods, numMethods);
    ALOG_ALWAYS_FATAL_IF(result < 0, "RegisterNatives failed for '%s'; aborting...",
                         className);

    return 0;
}

int jniThrowException(C_JNIEnv* env, const char* className, const char* msg) {
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);

    if ((*env)->ExceptionCheck(e)) {
        /* TODO: consider creating the new exception with this as "cause" */
        scoped_local_ref<jthrowable> exception(env, (*env)->ExceptionOccurred(e));
        (*env)->ExceptionClear(e);

        if (exception.get() != NULL) {
            std::string text;
            getExceptionSummary(env, exception.get(), text);
            ALOGW("Discarding pending exception (%s) to throw %s", text.c_str(), className);
        }
    }

    scoped_local_ref<jclass> exceptionClass(env, findClass(env, className));
    if (exceptionClass.get() == NULL) {
        ALOGE("Unable to find exception class %s", className);
        /* ClassNotFoundException now pending */
        return -1;
    }

    if ((*env)->ThrowNew(e, exceptionClass.get(), msg) != JNI_OK) {
        ALOGE("Failed throwing '%s' '%s'", className, msg);
        /* an exception, most likely OOM, will now be pending */
        return -1;
    }

    return 0;
}

int jniThrowExceptionFmt(C_JNIEnv* env, const char* className, const char* fmt, va_list args) {
    char msgBuf[512];
    vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
    return jniThrowException(env, className, msgBuf);
}

int jniThrowNullPointerException(C_JNIEnv* env, const char* msg) {
    return jniThrowException(env, "java/lang/NullPointerException", msg);
}

int jniThrowRuntimeException(C_JNIEnv* env, const char* msg) {
    return jniThrowException(env, "java/lang/RuntimeException", msg);
}

int jniThrowIOException(C_JNIEnv* env, int errnum) {
    char buffer[80];
    const char* message = jniStrError(errnum, buffer, sizeof(buffer));
    return jniThrowException(env, "java/io/IOException", message);
}

void jniLogException(C_JNIEnv* env, int priority, const char* tag, jthrowable exception) {
    std::string trace(jniGetStackTrace(env, exception));
    __android_log_write(priority, tag, trace.c_str());
}

const char* jniStrError(int errnum, char* buf, size_t buflen) {
#ifdef _WIN32
  strerror_s(buf, buflen, errnum);
  return buf;
#else
  // The magic of C++ overloading selects the correct implementation based on the declared type of
  // strerror_r. The inline will ensure that we don't have any indirect calls.
  return realJniStrError(strerror_r, errnum, buf, buflen);
#endif
}

jobject jniCreateFileDescriptor(C_JNIEnv* env, int fd) {
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);
    jobject fileDescriptor = e->NewObject(JniConstants::GetFileDescriptorClass(e),
                                          JniConstants::GetFileDescriptorInitMethod(e));
    // NOTE: NewObject ensures that an OutOfMemoryError will be seen by the Java
    // caller if the alloc fails, so we just return nullptr when that happens.
    if (fileDescriptor != nullptr)  {
        jniSetFileDescriptorOfFD(env, fileDescriptor, fd);
    }
    return fileDescriptor;
}

int jniGetFDFromFileDescriptor(C_JNIEnv* env, jobject fileDescriptor) {
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);
    if (fileDescriptor != nullptr) {
        return e->GetIntField(fileDescriptor,
                              JniConstants::GetFileDescriptorDescriptorField(e));
    } else {
        return -1;
    }
}

void jniSetFileDescriptorOfFD(C_JNIEnv* env, jobject fileDescriptor, int value) {
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);
    if (fileDescriptor == nullptr) {
        jniThrowNullPointerException(e, "null FileDescriptor");
    } else {
        e->SetIntField(fileDescriptor, JniConstants::GetFileDescriptorDescriptorField(e), value);
    }
}

jlong jniGetOwnerIdFromFileDescriptor(C_JNIEnv* env, jobject fileDescriptor) {
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);
    return e->GetLongField(fileDescriptor, JniConstants::GetFileDescriptorOwnerIdField(e));
}

jarray jniGetNioBufferBaseArray(C_JNIEnv* env, jobject nioBuffer) {
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);
    jclass nioAccessClass = JniConstants::GetNioAccessClass(e);
    jmethodID getBaseArrayMethod = JniConstants::GetNioAccessGetBaseArrayMethod(e);
    jobject object = e->CallStaticObjectMethod(nioAccessClass, getBaseArrayMethod, nioBuffer);
    return static_cast<jarray>(object);
}

int jniGetNioBufferBaseArrayOffset(C_JNIEnv* env, jobject nioBuffer) {
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);
    jclass nioAccessClass = JniConstants::GetNioAccessClass(e);
    jmethodID getBaseArrayOffsetMethod = JniConstants::GetNioAccessGetBaseArrayOffsetMethod(e);
    return e->CallStaticIntMethod(nioAccessClass, getBaseArrayOffsetMethod, nioBuffer);
}

jlong jniGetNioBufferPointer(C_JNIEnv* env, jobject nioBuffer) {
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);
    jlong baseAddress = e->GetLongField(nioBuffer, JniConstants::GetNioBufferAddressField(e));
    if (baseAddress != 0) {
      const int position = e->GetIntField(nioBuffer, JniConstants::GetNioBufferPositionField(e));
      const int shift =
          e->GetIntField(nioBuffer, JniConstants::GetNioBufferElementSizeShiftField(e));
      baseAddress += position << shift;
    }
    return baseAddress;
}

jlong jniGetNioBufferFields(C_JNIEnv* env, jobject nioBuffer,
                            jint* position, jint* limit, jint* elementSizeShift) {
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);
    *position = e->GetIntField(nioBuffer, JniConstants::GetNioBufferPositionField(e));
    *limit = e->GetIntField(nioBuffer, JniConstants::GetNioBufferLimitField(e));
    *elementSizeShift =
        e->GetIntField(nioBuffer, JniConstants::GetNioBufferElementSizeShiftField(e));
    return e->GetLongField(nioBuffer, JniConstants::GetNioBufferAddressField(e));
}

jobject jniGetReferent(C_JNIEnv* env, jobject ref) {
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);
    return e->CallObjectMethod(ref, JniConstants::GetReferenceGetMethod(e));
}

jstring jniCreateString(C_JNIEnv* env, const jchar* unicodeChars, jsize len) {
    JNIEnv* e = reinterpret_cast<JNIEnv*>(env);
    return e->NewString(unicodeChars, len);
}

void jniUninitializeConstants() {
  JniConstants::Uninitialize();
}
