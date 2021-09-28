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

#include "utils/java/jni-base.h"

#include "utils/base/status.h"
#include "utils/java/string_utils.h"

namespace libtextclassifier3 {

bool EnsureLocalCapacity(JNIEnv* env, int capacity) {
  return env->EnsureLocalCapacity(capacity) == JNI_OK;
}

bool JniExceptionCheckAndClear(JNIEnv* env) {
  TC3_CHECK(env != nullptr);
  const bool result = env->ExceptionCheck();
  if (result) {
    env->ExceptionDescribe();
    env->ExceptionClear();
  }
  return result;
}

StatusOr<std::string> ToStlString(JNIEnv* env, const jstring& str) {
  std::string result;
  if (!JStringToUtf8String(env, str, &result)) {
    return {Status::UNKNOWN};
  }
  return result;
}

}  // namespace libtextclassifier3
