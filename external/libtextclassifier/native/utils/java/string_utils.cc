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

#include "utils/java/string_utils.h"

#include "utils/base/logging.h"

namespace libtextclassifier3 {

bool JByteArrayToString(JNIEnv* env, const jbyteArray& array,
                        std::string* result) {
  jbyte* const array_bytes = env->GetByteArrayElements(array, JNI_FALSE);
  if (array_bytes == nullptr) {
    return false;
  }

  const int array_length = env->GetArrayLength(array);
  *result = std::string(reinterpret_cast<char*>(array_bytes), array_length);

  env->ReleaseByteArrayElements(array, array_bytes, JNI_ABORT);

  return true;
}

bool JStringToUtf8String(JNIEnv* env, const jstring& jstr,
                         std::string* result) {
  if (jstr == nullptr) {
    *result = std::string();
    return true;
  }

  jclass string_class = env->FindClass("java/lang/String");
  if (!string_class) {
    TC3_LOG(ERROR) << "Can't find String class";
    return false;
  }

  jmethodID get_bytes_id =
      env->GetMethodID(string_class, "getBytes", "(Ljava/lang/String;)[B");

  jstring encoding = env->NewStringUTF("UTF-8");

  jbyteArray array = reinterpret_cast<jbyteArray>(
      env->CallObjectMethod(jstr, get_bytes_id, encoding));

  JByteArrayToString(env, array, result);

  // Release the array.
  env->DeleteLocalRef(array);
  env->DeleteLocalRef(string_class);
  env->DeleteLocalRef(encoding);

  return true;
}

ScopedStringChars GetScopedStringChars(JNIEnv* env, jstring string,
                                       jboolean* is_copy) {
  return ScopedStringChars(env->GetStringUTFChars(string, is_copy),
                           StringCharsReleaser(env, string));
}

}  // namespace libtextclassifier3
