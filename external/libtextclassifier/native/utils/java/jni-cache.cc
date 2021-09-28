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

#include "utils/java/jni-cache.h"

#include "utils/base/logging.h"
#include "utils/java/jni-base.h"
#include "utils/java/jni-helper.h"

namespace libtextclassifier3 {

JniCache::JniCache(JavaVM* jvm)
    : jvm(jvm),
      string_class(nullptr, jvm),
      string_utf8(nullptr, jvm),
      pattern_class(nullptr, jvm),
      matcher_class(nullptr, jvm),
      locale_class(nullptr, jvm),
      locale_us(nullptr, jvm),
      breakiterator_class(nullptr, jvm),
      integer_class(nullptr, jvm),
      calendar_class(nullptr, jvm),
      timezone_class(nullptr, jvm),
      urlencoder_class(nullptr, jvm)
#ifdef __ANDROID__
      ,
      context_class(nullptr, jvm),
      uri_class(nullptr, jvm),
      usermanager_class(nullptr, jvm),
      bundle_class(nullptr, jvm),
      resources_class(nullptr, jvm)
#endif
{
}

// The macros below are intended to reduce the boilerplate in Create and avoid
// easily introduced copy/paste errors.
#define TC3_CHECK_JNI_PTR(PTR) TC3_CHECK((PTR) != nullptr)
#define TC3_CHECK_JNI_RESULT(RESULT) TC3_CHECK(RESULT)

#define TC3_GET_CLASS_OR_RETURN_NULL(FIELD, NAME)                 \
  {                                                               \
    TC3_ASSIGN_OR_RETURN_NULL(ScopedLocalRef<jclass> clazz,       \
                              JniHelper::FindClass(env, NAME));   \
    result->FIELD##_class = MakeGlobalRef(clazz.get(), env, jvm); \
    if (result->FIELD##_class == nullptr) {                       \
      TC3_LOG(ERROR) << "Error finding class: " << NAME;          \
      return nullptr;                                             \
    }                                                             \
  }

#define TC3_GET_OPTIONAL_CLASS(FIELD, NAME)                         \
  {                                                                 \
    StatusOr<ScopedLocalRef<jclass>> status_or_class =              \
        JniHelper::FindClass(env, NAME);                            \
    if (status_or_class.ok()) {                                     \
      result->FIELD##_class = MakeGlobalRef(                        \
          std::move(status_or_class).ValueOrDie().get(), env, jvm); \
    }                                                               \
  }

#define TC3_GET_METHOD(CLASS, FIELD, NAME, SIGNATURE)                 \
  result->CLASS##_##FIELD =                                           \
      env->GetMethodID(result->CLASS##_class.get(), NAME, SIGNATURE); \
  TC3_CHECK_JNI_RESULT(result->CLASS##_##FIELD)                       \
      << "Error finding method: " << NAME;

#define TC3_GET_OPTIONAL_METHOD(CLASS, FIELD, NAME, SIGNATURE)          \
  if (result->CLASS##_class != nullptr) {                               \
    result->CLASS##_##FIELD =                                           \
        env->GetMethodID(result->CLASS##_class.get(), NAME, SIGNATURE); \
    env->ExceptionClear();                                              \
  }

#define TC3_GET_OPTIONAL_STATIC_METHOD(CLASS, FIELD, NAME, SIGNATURE)         \
  if (result->CLASS##_class != nullptr) {                                     \
    result->CLASS##_##FIELD =                                                 \
        env->GetStaticMethodID(result->CLASS##_class.get(), NAME, SIGNATURE); \
    env->ExceptionClear();                                                    \
  }

#define TC3_GET_STATIC_METHOD(CLASS, FIELD, NAME, SIGNATURE)                \
  result->CLASS##_##FIELD =                                                 \
      env->GetStaticMethodID(result->CLASS##_class.get(), NAME, SIGNATURE); \
  TC3_CHECK_JNI_RESULT(result->CLASS##_##FIELD)                             \
      << "Error finding method: " << NAME;

#define TC3_GET_STATIC_OBJECT_FIELD_OR_RETURN_NULL(CLASS, FIELD, NAME,       \
                                                   SIGNATURE)                \
  {                                                                          \
    const jfieldID CLASS##_##FIELD##_field =                                 \
        env->GetStaticFieldID(result->CLASS##_class.get(), NAME, SIGNATURE); \
    TC3_CHECK_JNI_RESULT(CLASS##_##FIELD##_field)                            \
        << "Error finding field id: " << NAME;                               \
    TC3_ASSIGN_OR_RETURN_NULL(                                               \
        ScopedLocalRef<jobject> static_object,                               \
        JniHelper::GetStaticObjectField(env, result->CLASS##_class.get(),    \
                                        CLASS##_##FIELD##_field));           \
    result->CLASS##_##FIELD = MakeGlobalRef(static_object.get(), env, jvm);  \
    if (result->CLASS##_##FIELD == nullptr) {                                \
      TC3_LOG(ERROR) << "Error finding field: " << NAME;                     \
      return nullptr;                                                        \
    }                                                                        \
  }

#define TC3_GET_STATIC_INT_FIELD(CLASS, FIELD, NAME)                 \
  const jfieldID CLASS##_##FIELD##_field =                           \
      env->GetStaticFieldID(result->CLASS##_class.get(), NAME, "I"); \
  TC3_CHECK_JNI_RESULT(CLASS##_##FIELD##_field)                      \
      << "Error finding field id: " << NAME;                         \
  result->CLASS##_##FIELD = env->GetStaticIntField(                  \
      result->CLASS##_class.get(), CLASS##_##FIELD##_field);         \
  TC3_CHECK_JNI_RESULT(result->CLASS##_##FIELD)                      \
      << "Error finding field: " << NAME;

std::unique_ptr<JniCache> JniCache::Create(JNIEnv* env) {
  if (env == nullptr) {
    return nullptr;
  }
  JavaVM* jvm = nullptr;
  if (JNI_OK != env->GetJavaVM(&jvm) || jvm == nullptr) {
    return nullptr;
  }
  std::unique_ptr<JniCache> result(new JniCache(jvm));

  // String
  TC3_GET_CLASS_OR_RETURN_NULL(string, "java/lang/String");
  TC3_GET_METHOD(string, init_bytes_charset, "<init>",
                 "([BLjava/lang/String;)V");
  TC3_GET_METHOD(string, code_point_count, "codePointCount", "(II)I");
  TC3_GET_METHOD(string, length, "length", "()I");
  TC3_ASSIGN_OR_RETURN_NULL(ScopedLocalRef<jstring> result_string,
                            JniHelper::NewStringUTF(env, "UTF-8"));
  result->string_utf8 = MakeGlobalRef(result_string.get(), env, jvm);
  TC3_CHECK_JNI_PTR(result->string_utf8);

  // Pattern
  TC3_GET_CLASS_OR_RETURN_NULL(pattern, "java/util/regex/Pattern");
  TC3_GET_STATIC_METHOD(pattern, compile, "compile",
                        "(Ljava/lang/String;)Ljava/util/regex/Pattern;");
  TC3_GET_METHOD(pattern, matcher, "matcher",
                 "(Ljava/lang/CharSequence;)Ljava/util/regex/Matcher;");

  // Matcher
  TC3_GET_CLASS_OR_RETURN_NULL(matcher, "java/util/regex/Matcher");
  TC3_GET_METHOD(matcher, matches, "matches", "()Z");
  TC3_GET_METHOD(matcher, find, "find", "()Z");
  TC3_GET_METHOD(matcher, reset, "reset", "()Ljava/util/regex/Matcher;");
  TC3_GET_METHOD(matcher, start_idx, "start", "(I)I");
  TC3_GET_METHOD(matcher, end_idx, "end", "(I)I");
  TC3_GET_METHOD(matcher, group, "group", "()Ljava/lang/String;");
  TC3_GET_METHOD(matcher, group_idx, "group", "(I)Ljava/lang/String;");

  // Locale
  TC3_GET_CLASS_OR_RETURN_NULL(locale, "java/util/Locale");
  TC3_GET_STATIC_OBJECT_FIELD_OR_RETURN_NULL(locale, us, "US",
                                             "Ljava/util/Locale;");
  TC3_GET_METHOD(locale, init_string, "<init>", "(Ljava/lang/String;)V");
  TC3_GET_OPTIONAL_STATIC_METHOD(locale, for_language_tag, "forLanguageTag",
                                 "(Ljava/lang/String;)Ljava/util/Locale;");

  // BreakIterator
  TC3_GET_CLASS_OR_RETURN_NULL(breakiterator, "java/text/BreakIterator");
  TC3_GET_STATIC_METHOD(breakiterator, getwordinstance, "getWordInstance",
                        "(Ljava/util/Locale;)Ljava/text/BreakIterator;");
  TC3_GET_METHOD(breakiterator, settext, "setText", "(Ljava/lang/String;)V");
  TC3_GET_METHOD(breakiterator, next, "next", "()I");

  // Integer
  TC3_GET_CLASS_OR_RETURN_NULL(integer, "java/lang/Integer");
  TC3_GET_STATIC_METHOD(integer, parse_int, "parseInt",
                        "(Ljava/lang/String;)I");

  // Calendar.
  TC3_GET_CLASS_OR_RETURN_NULL(calendar, "java/util/Calendar");
  TC3_GET_STATIC_METHOD(
      calendar, get_instance, "getInstance",
      "(Ljava/util/TimeZone;Ljava/util/Locale;)Ljava/util/Calendar;");
  TC3_GET_METHOD(calendar, get_first_day_of_week, "getFirstDayOfWeek", "()I");
  TC3_GET_METHOD(calendar, get_time_in_millis, "getTimeInMillis", "()J");
  TC3_GET_METHOD(calendar, set_time_in_millis, "setTimeInMillis", "(J)V");
  TC3_GET_METHOD(calendar, add, "add", "(II)V");
  TC3_GET_METHOD(calendar, get, "get", "(I)I");
  TC3_GET_METHOD(calendar, set, "set", "(II)V");
  TC3_GET_STATIC_INT_FIELD(calendar, zone_offset, "ZONE_OFFSET");
  TC3_GET_STATIC_INT_FIELD(calendar, dst_offset, "DST_OFFSET");
  TC3_GET_STATIC_INT_FIELD(calendar, year, "YEAR");
  TC3_GET_STATIC_INT_FIELD(calendar, month, "MONTH");
  TC3_GET_STATIC_INT_FIELD(calendar, day_of_year, "DAY_OF_YEAR");
  TC3_GET_STATIC_INT_FIELD(calendar, day_of_month, "DAY_OF_MONTH");
  TC3_GET_STATIC_INT_FIELD(calendar, day_of_week, "DAY_OF_WEEK");
  TC3_GET_STATIC_INT_FIELD(calendar, hour_of_day, "HOUR_OF_DAY");
  TC3_GET_STATIC_INT_FIELD(calendar, minute, "MINUTE");
  TC3_GET_STATIC_INT_FIELD(calendar, second, "SECOND");
  TC3_GET_STATIC_INT_FIELD(calendar, millisecond, "MILLISECOND");
  TC3_GET_STATIC_INT_FIELD(calendar, sunday, "SUNDAY");
  TC3_GET_STATIC_INT_FIELD(calendar, monday, "MONDAY");
  TC3_GET_STATIC_INT_FIELD(calendar, tuesday, "TUESDAY");
  TC3_GET_STATIC_INT_FIELD(calendar, wednesday, "WEDNESDAY");
  TC3_GET_STATIC_INT_FIELD(calendar, thursday, "THURSDAY");
  TC3_GET_STATIC_INT_FIELD(calendar, friday, "FRIDAY");
  TC3_GET_STATIC_INT_FIELD(calendar, saturday, "SATURDAY");

  // TimeZone.
  TC3_GET_CLASS_OR_RETURN_NULL(timezone, "java/util/TimeZone");
  TC3_GET_STATIC_METHOD(timezone, get_timezone, "getTimeZone",
                        "(Ljava/lang/String;)Ljava/util/TimeZone;");

  // URLEncoder.
  TC3_GET_CLASS_OR_RETURN_NULL(urlencoder, "java/net/URLEncoder");
  TC3_GET_STATIC_METHOD(
      urlencoder, encode, "encode",
      "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");

#ifdef __ANDROID__
  // Context.
  TC3_GET_CLASS_OR_RETURN_NULL(context, "android/content/Context");
  TC3_GET_METHOD(context, get_package_name, "getPackageName",
                 "()Ljava/lang/String;");
  TC3_GET_METHOD(context, get_system_service, "getSystemService",
                 "(Ljava/lang/String;)Ljava/lang/Object;");

  // Uri.
  TC3_GET_CLASS_OR_RETURN_NULL(uri, "android/net/Uri");
  TC3_GET_STATIC_METHOD(uri, parse, "parse",
                        "(Ljava/lang/String;)Landroid/net/Uri;");
  TC3_GET_METHOD(uri, get_scheme, "getScheme", "()Ljava/lang/String;");
  TC3_GET_METHOD(uri, get_host, "getHost", "()Ljava/lang/String;");

  // UserManager.
  TC3_GET_OPTIONAL_CLASS(usermanager, "android/os/UserManager");
  TC3_GET_OPTIONAL_METHOD(usermanager, get_user_restrictions,
                          "getUserRestrictions", "()Landroid/os/Bundle;");

  // Bundle.
  TC3_GET_CLASS_OR_RETURN_NULL(bundle, "android/os/Bundle");
  TC3_GET_METHOD(bundle, get_boolean, "getBoolean", "(Ljava/lang/String;)Z");

  // String resources.
  TC3_GET_CLASS_OR_RETURN_NULL(resources, "android/content/res/Resources");
  TC3_GET_STATIC_METHOD(resources, get_system, "getSystem",
                        "()Landroid/content/res/Resources;");
  TC3_GET_METHOD(resources, get_identifier, "getIdentifier",
                 "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I");
  TC3_GET_METHOD(resources, get_string, "getString", "(I)Ljava/lang/String;");
#endif

  return result;
}

#undef TC3_GET_STATIC_INT_FIELD
#undef TC3_GET_STATIC_OBJECT_FIELD_OR_RETURN_NULL
#undef TC3_GET_STATIC_METHOD
#undef TC3_GET_METHOD
#undef TC3_GET_CLASS_OR_RETURN_NULL
#undef TC3_GET_OPTIONAL_CLASS
#undef TC3_CHECK_JNI_PTR

JNIEnv* JniCache::GetEnv() const {
  void* env;
  if (JNI_OK == jvm->GetEnv(&env, JNI_VERSION_1_4)) {
    return reinterpret_cast<JNIEnv*>(env);
  } else {
    TC3_LOG(ERROR) << "JavaICU UniLib used on unattached thread";
    return nullptr;
  }
}

bool JniCache::ExceptionCheckAndClear() const {
  return JniExceptionCheckAndClear(GetEnv());
}

StatusOr<ScopedLocalRef<jstring>> JniCache::ConvertToJavaString(
    const char* utf8_text, const int utf8_text_size_bytes) const {
  // Create java byte array.
  JNIEnv* jenv = GetEnv();
  TC3_ASSIGN_OR_RETURN(ScopedLocalRef<jbyteArray> text_java_utf8,
                       JniHelper::NewByteArray(jenv, utf8_text_size_bytes));

  jenv->SetByteArrayRegion(text_java_utf8.get(), 0, utf8_text_size_bytes,
                           reinterpret_cast<const jbyte*>(utf8_text));

  // Create the string with a UTF-8 charset.
  TC3_ASSIGN_OR_RETURN(ScopedLocalRef<jstring> result,
                       JniHelper::NewObject<jstring>(
                           jenv, string_class.get(), string_init_bytes_charset,
                           text_java_utf8.get(), string_utf8.get()));

  return result;
}

StatusOr<ScopedLocalRef<jstring>> JniCache::ConvertToJavaString(
    StringPiece utf8_text) const {
  return ConvertToJavaString(utf8_text.data(), utf8_text.size());
}

StatusOr<ScopedLocalRef<jstring>> JniCache::ConvertToJavaString(
    const UnicodeText& text) const {
  return ConvertToJavaString(text.data(), text.size_bytes());
}

}  // namespace libtextclassifier3
