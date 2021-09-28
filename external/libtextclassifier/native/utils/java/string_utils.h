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

#ifndef LIBTEXTCLASSIFIER_UTILS_JAVA_STRING_UTILS_H_
#define LIBTEXTCLASSIFIER_UTILS_JAVA_STRING_UTILS_H_

#include <jni.h>
#include <memory>
#include <string>

#include "utils/base/logging.h"

namespace libtextclassifier3 {

bool JByteArrayToString(JNIEnv* env, const jbyteArray& array,
                        std::string* result);
bool JStringToUtf8String(JNIEnv* env, const jstring& jstr, std::string* result);

// A deleter to be used with std::unique_ptr to release Java string chars.
class StringCharsReleaser {
 public:
  StringCharsReleaser() : env_(nullptr) {}

  StringCharsReleaser(JNIEnv* env, jstring jstr) : env_(env), jstr_(jstr) {}

  StringCharsReleaser(const StringCharsReleaser& orig) = default;

  // Copy assignment to allow move semantics in StringCharsReleaser.
  StringCharsReleaser& operator=(const StringCharsReleaser& rhs) {
    // As the releaser and its state are thread-local, it's enough to only
    // ensure the envs are consistent but do nothing.
    TC3_CHECK_EQ(env_, rhs.env_);
    return *this;
  }

  // The delete operator.
  void operator()(const char* chars) const {
    if (env_ != nullptr) {
      env_->ReleaseStringUTFChars(jstr_, chars);
    }
  }

 private:
  // The env_ stashed to use for deletion. Thread-local, don't share!
  JNIEnv* const env_;

  // The referenced jstring.
  jstring jstr_;
};

// A smart pointer that releases string chars when it goes out of scope.
// of scope.
// Note that this class is not thread-safe since it caches JNIEnv in
// the deleter. Do not use the same jobject across different threads.
using ScopedStringChars = std::unique_ptr<const char, StringCharsReleaser>;

// Returns a scoped pointer to the array of Unicode characters of a string.
ScopedStringChars GetScopedStringChars(JNIEnv* env, jstring string,
                                       jboolean* is_copy = nullptr);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_JAVA_STRING_UTILS_H_
