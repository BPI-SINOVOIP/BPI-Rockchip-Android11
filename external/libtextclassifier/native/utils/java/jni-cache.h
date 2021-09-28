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

#ifndef LIBTEXTCLASSIFIER_UTILS_JAVA_JNI_CACHE_H_
#define LIBTEXTCLASSIFIER_UTILS_JAVA_JNI_CACHE_H_

#include <jni.h>

#include "utils/base/statusor.h"
#include "utils/java/jni-base.h"
#include "utils/strings/stringpiece.h"
#include "utils/utf8/unicodetext.h"

namespace libtextclassifier3 {

// A helper class to cache class and method pointers for calls from JNI to Java.
// (for implementations such as Java ICU that need to make calls from C++ to
// Java)
struct JniCache {
  static std::unique_ptr<JniCache> Create(JNIEnv* env);

  JNIEnv* GetEnv() const;
  bool ExceptionCheckAndClear() const;

  JavaVM* jvm = nullptr;

  // java.lang.String
  ScopedGlobalRef<jclass> string_class;
  jmethodID string_init_bytes_charset = nullptr;
  jmethodID string_code_point_count = nullptr;
  jmethodID string_length = nullptr;
  ScopedGlobalRef<jstring> string_utf8;

  // java.util.regex.Pattern
  ScopedGlobalRef<jclass> pattern_class;
  jmethodID pattern_compile = nullptr;
  jmethodID pattern_matcher = nullptr;

  // java.util.regex.Matcher
  ScopedGlobalRef<jclass> matcher_class;
  jmethodID matcher_matches = nullptr;
  jmethodID matcher_find = nullptr;
  jmethodID matcher_reset = nullptr;
  jmethodID matcher_start_idx = nullptr;
  jmethodID matcher_end_idx = nullptr;
  jmethodID matcher_group = nullptr;
  jmethodID matcher_group_idx = nullptr;

  // java.util.Locale
  ScopedGlobalRef<jclass> locale_class;
  ScopedGlobalRef<jobject> locale_us;
  jmethodID locale_init_string = nullptr;
  jmethodID locale_for_language_tag = nullptr;

  // java.text.BreakIterator
  ScopedGlobalRef<jclass> breakiterator_class;
  jmethodID breakiterator_getwordinstance = nullptr;
  jmethodID breakiterator_settext = nullptr;
  jmethodID breakiterator_next = nullptr;

  // java.lang.Integer
  ScopedGlobalRef<jclass> integer_class;
  jmethodID integer_parse_int = nullptr;

  // java.util.Calendar
  ScopedGlobalRef<jclass> calendar_class;
  jmethodID calendar_get_instance = nullptr;
  jmethodID calendar_get_first_day_of_week = nullptr;
  jmethodID calendar_get_time_in_millis = nullptr;
  jmethodID calendar_set_time_in_millis = nullptr;
  jmethodID calendar_add = nullptr;
  jmethodID calendar_get = nullptr;
  jmethodID calendar_set = nullptr;
  jint calendar_zone_offset;
  jint calendar_dst_offset;
  jint calendar_year;
  jint calendar_month;
  jint calendar_day_of_year;
  jint calendar_day_of_month;
  jint calendar_day_of_week;
  jint calendar_hour_of_day;
  jint calendar_minute;
  jint calendar_second;
  jint calendar_millisecond;
  jint calendar_sunday;
  jint calendar_monday;
  jint calendar_tuesday;
  jint calendar_wednesday;
  jint calendar_thursday;
  jint calendar_friday;
  jint calendar_saturday;

  // java.util.TimeZone
  ScopedGlobalRef<jclass> timezone_class;
  jmethodID timezone_get_timezone = nullptr;

  // java.net.URLEncoder
  ScopedGlobalRef<jclass> urlencoder_class;
  jmethodID urlencoder_encode = nullptr;

  // android.content.Context
  ScopedGlobalRef<jclass> context_class;
  jmethodID context_get_package_name = nullptr;
  jmethodID context_get_system_service = nullptr;

  // android.net.Uri
  ScopedGlobalRef<jclass> uri_class;
  jmethodID uri_parse = nullptr;
  jmethodID uri_get_scheme = nullptr;
  jmethodID uri_get_host = nullptr;

  // android.os.UserManager
  ScopedGlobalRef<jclass> usermanager_class;
  jmethodID usermanager_get_user_restrictions = nullptr;

  // android.os.Bundle
  ScopedGlobalRef<jclass> bundle_class;
  jmethodID bundle_get_boolean = nullptr;

  // android.content.res.Resources
  ScopedGlobalRef<jclass> resources_class;
  jmethodID resources_get_system = nullptr;
  jmethodID resources_get_identifier = nullptr;
  jmethodID resources_get_string = nullptr;

  // Helper to convert lib3 UnicodeText to Java strings.
  StatusOr<ScopedLocalRef<jstring>> ConvertToJavaString(
      const char* utf8_text, const int utf8_text_size_bytes) const;
  StatusOr<ScopedLocalRef<jstring>> ConvertToJavaString(
      StringPiece utf8_text) const;
  StatusOr<ScopedLocalRef<jstring>> ConvertToJavaString(
      const UnicodeText& text) const;

 private:
  explicit JniCache(JavaVM* jvm);
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_JAVA_JNI_CACHE_H_
