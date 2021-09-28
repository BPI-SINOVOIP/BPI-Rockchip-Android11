/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef LIBTEXTCLASSIFIER_UTILS_JAVA_JNI_BASE_H_
#define LIBTEXTCLASSIFIER_UTILS_JAVA_JNI_BASE_H_

#include <jni.h>

#include <string>

#include "utils/base/statusor.h"

// When we use a macro as an argument for a macro, an additional level of
// indirection is needed, if the macro argument is used with # or ##.
#define TC3_ADD_QUOTES_HELPER(TOKEN) #TOKEN
#define TC3_ADD_QUOTES(TOKEN) TC3_ADD_QUOTES_HELPER(TOKEN)

#ifndef TC3_PACKAGE_NAME
#define TC3_PACKAGE_NAME com_google_android_textclassifier
#endif

#ifndef TC3_PACKAGE_PATH
#define TC3_PACKAGE_PATH \
  "com/google/android/textclassifier/"
#endif

#define TC3_JNI_METHOD_NAME_INTERNAL(package_name, class_name, method_name) \
  Java_##package_name##_##class_name##_##method_name

#define TC3_JNI_METHOD_PRIMITIVE(return_type, package_name, class_name, \
                                 method_name)                           \
  JNIEXPORT return_type JNICALL TC3_JNI_METHOD_NAME_INTERNAL(           \
      package_name, class_name, method_name)

// The indirection is needed to correctly expand the TC3_PACKAGE_NAME macro.
// See the explanation near TC3_ADD_QUOTES macro.
#define TC3_JNI_METHOD2(return_type, package_name, class_name, method_name) \
  TC3_JNI_METHOD_PRIMITIVE(return_type, package_name, class_name, method_name)

#define TC3_JNI_METHOD(return_type, class_name, method_name) \
  TC3_JNI_METHOD2(return_type, TC3_PACKAGE_NAME, class_name, method_name)

#define TC3_JNI_METHOD_NAME2(package_name, class_name, method_name) \
  TC3_JNI_METHOD_NAME_INTERNAL(package_name, class_name, method_name)

#define TC3_JNI_METHOD_NAME(class_name, method_name) \
  TC3_JNI_METHOD_NAME2(TC3_PACKAGE_NAME, class_name, method_name)

namespace libtextclassifier3 {

// Returns true if the requested capacity is available.
bool EnsureLocalCapacity(JNIEnv* env, int capacity);

// Returns true if there was an exception. Also it clears the exception.
bool JniExceptionCheckAndClear(JNIEnv* env);

StatusOr<std::string> ToStlString(JNIEnv* env, const jstring& str);

// A deleter to be used with std::unique_ptr to delete JNI global references.
class GlobalRefDeleter {
 public:
  explicit GlobalRefDeleter(JavaVM* jvm) : jvm_(jvm) {}

  GlobalRefDeleter(const GlobalRefDeleter& orig) = default;

  // Copy assignment to allow move semantics in ScopedGlobalRef.
  GlobalRefDeleter& operator=(const GlobalRefDeleter& rhs) {
    TC3_CHECK_EQ(jvm_, rhs.jvm_);
    return *this;
  }

  // The delete operator.
  void operator()(jobject object) const {
    JNIEnv* env;
    if (object != nullptr && jvm_ != nullptr &&
        JNI_OK ==
            jvm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_4)) {
      env->DeleteGlobalRef(object);
    }
  }

 private:
  // The jvm_ stashed to use for deletion.
  JavaVM* const jvm_;
};

// A deleter to be used with std::unique_ptr to delete JNI local references.
class LocalRefDeleter {
 public:
  explicit LocalRefDeleter(JNIEnv* env)
      : env_(env) {}  // NOLINT(runtime/explicit)

  LocalRefDeleter(const LocalRefDeleter& orig) = default;

  // Copy assignment to allow move semantics in ScopedLocalRef.
  LocalRefDeleter& operator=(const LocalRefDeleter& rhs) {
    env_ = rhs.env_;
    return *this;
  }

  // The delete operator.
  void operator()(jobject object) const {
    if (env_) {
      env_->DeleteLocalRef(object);
    }
  }

 private:
  // The env_ stashed to use for deletion. Thread-local, don't share!
  JNIEnv* env_;
};

// A smart pointer that deletes a reference when it goes out of scope.
//
// Note that this class is not thread-safe since it caches JNIEnv in
// the deleter. Do not use the same jobject across different threads.
template <typename T, typename Env, typename Deleter>
class ScopedRef {
 public:
  ScopedRef() : ptr_(nullptr, Deleter(nullptr)) {}
  ScopedRef(T value, Env* env) : ptr_(value, Deleter(env)) {}

  T get() const { return ptr_.get(); }

  T release() { return ptr_.release(); }

  bool operator!() const { return !ptr_; }

  bool operator==(void* value) const { return ptr_.get() == value; }

  explicit operator bool() const { return ptr_ != nullptr; }

  void reset(T value, Env* env) {
    ptr_.reset(value);
    ptr_.get_deleter() = Deleter(env);
  }

 private:
  std::unique_ptr<typename std::remove_pointer<T>::type, Deleter> ptr_;
};

template <typename T, typename U, typename Env, typename Deleter>
inline bool operator==(const ScopedRef<T, Env, Deleter>& x,
                       const ScopedRef<U, Env, Deleter>& y) {
  return x.get() == y.get();
}

template <typename T, typename Env, typename Deleter>
inline bool operator==(const ScopedRef<T, Env, Deleter>& x, std::nullptr_t) {
  return x.get() == nullptr;
}

template <typename T, typename Env, typename Deleter>
inline bool operator==(std::nullptr_t, const ScopedRef<T, Env, Deleter>& x) {
  return nullptr == x.get();
}

template <typename T, typename U, typename Env, typename Deleter>
inline bool operator!=(const ScopedRef<T, Env, Deleter>& x,
                       const ScopedRef<U, Env, Deleter>& y) {
  return x.get() != y.get();
}

template <typename T, typename Env, typename Deleter>
inline bool operator!=(const ScopedRef<T, Env, Deleter>& x, std::nullptr_t) {
  return x.get() != nullptr;
}

template <typename T, typename Env, typename Deleter>
inline bool operator!=(std::nullptr_t, const ScopedRef<T, Env, Deleter>& x) {
  return nullptr != x.get();
}

template <typename T, typename U, typename Env, typename Deleter>
inline bool operator<(const ScopedRef<T, Env, Deleter>& x,
                      const ScopedRef<U, Env, Deleter>& y) {
  return x.get() < y.get();
}

template <typename T, typename U, typename Env, typename Deleter>
inline bool operator>(const ScopedRef<T, Env, Deleter>& x,
                      const ScopedRef<U, Env, Deleter>& y) {
  return x.get() > y.get();
}

// A smart pointer that deletes a JNI global reference when it goes out
// of scope. Usage is:
// ScopedGlobalRef<jobject> scoped_global(env->JniFunction(), jvm);
template <typename T>
using ScopedGlobalRef = ScopedRef<T, JavaVM, GlobalRefDeleter>;

// Ditto, but usage is:
// ScopedLocalRef<jobject> scoped_local(env->JniFunction(), env);
template <typename T>
using ScopedLocalRef = ScopedRef<T, JNIEnv, LocalRefDeleter>;

// A helper to create global references.
template <typename T>
ScopedGlobalRef<T> MakeGlobalRef(T object, JNIEnv* env, JavaVM* jvm) {
  const jobject global_object = env->NewGlobalRef(object);
  return ScopedGlobalRef<T>(reinterpret_cast<T>(global_object), jvm);
}

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_JAVA_JNI_BASE_H_
