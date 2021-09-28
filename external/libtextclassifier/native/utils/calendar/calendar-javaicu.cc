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

#include "utils/calendar/calendar-javaicu.h"

#include "annotator/types.h"
#include "utils/base/statusor.h"
#include "utils/java/jni-base.h"
#include "utils/java/jni-helper.h"

namespace libtextclassifier3 {
namespace {

// Generic version of icu::Calendar::add with error checking.
bool CalendarAdd(JniCache* jni_cache, JNIEnv* jenv, jobject calendar,
                 jint field, jint value) {
  return JniHelper::CallVoidMethod(jenv, calendar, jni_cache->calendar_add,
                                   field, value)
      .ok();
}

// Generic version of icu::Calendar::get with error checking.
bool CalendarGet(JniCache* jni_cache, JNIEnv* jenv, jobject calendar,
                 jint field, jint* value) {
  TC3_ASSIGN_OR_RETURN_FALSE(
      *value,
      JniHelper::CallIntMethod(jenv, calendar, jni_cache->calendar_get, field));
  return true;
}

// Generic version of icu::Calendar::set with error checking.
bool CalendarSet(JniCache* jni_cache, JNIEnv* jenv, jobject calendar,
                 jint field, jint value) {
  return JniHelper::CallVoidMethod(jenv, calendar, jni_cache->calendar_set,
                                   field, value)
      .ok();
}

// Extracts the first tag from a BCP47 tag (e.g. "en" for "en-US").
std::string GetFirstBcp47Tag(const std::string& tag) {
  for (size_t i = 0; i < tag.size(); ++i) {
    if (tag[i] == '_' || tag[i] == '-') {
      return std::string(tag, 0, i);
    }
  }
  return tag;
}

}  // anonymous namespace

Calendar::Calendar(JniCache* jni_cache)
    : jni_cache_(jni_cache),
      jenv_(jni_cache_ ? jni_cache->GetEnv() : nullptr),
      calendar_(nullptr, jenv_) {}

bool Calendar::Initialize(const std::string& time_zone,
                          const std::string& locale, int64 time_ms_utc) {
  if (!jni_cache_ || !jenv_) {
    TC3_LOG(ERROR) << "Initialize without env";
    return false;
  }

  // We'll assume the day indices match later on, so verify it here.
  if (jni_cache_->calendar_sunday != kSunday ||
      jni_cache_->calendar_monday != kMonday ||
      jni_cache_->calendar_tuesday != kTuesday ||
      jni_cache_->calendar_wednesday != kWednesday ||
      jni_cache_->calendar_thursday != kThursday ||
      jni_cache_->calendar_friday != kFriday ||
      jni_cache_->calendar_saturday != kSaturday) {
    TC3_LOG(ERROR) << "day of the week indices mismatch";
    return false;
  }

  // Get the time zone.
  TC3_ASSIGN_OR_RETURN_FALSE(ScopedLocalRef<jstring> java_time_zone_str,
                             JniHelper::NewStringUTF(jenv_, time_zone.c_str()));
  TC3_ASSIGN_OR_RETURN_FALSE(
      ScopedLocalRef<jobject> java_time_zone,
      JniHelper::CallStaticObjectMethod(jenv_, jni_cache_->timezone_class.get(),
                                        jni_cache_->timezone_get_timezone,
                                        java_time_zone_str.get()));
  if (java_time_zone == nullptr) {
    TC3_LOG(ERROR) << "failed to get timezone";
    return false;
  }

  // Get the locale.
  ScopedLocalRef<jobject> java_locale(nullptr, jenv_);
  if (jni_cache_->locale_for_language_tag) {
    // API level 21+, we can actually parse language tags.
    TC3_ASSIGN_OR_RETURN_FALSE(ScopedLocalRef<jstring> java_locale_str,
                               JniHelper::NewStringUTF(jenv_, locale.c_str()));

    TC3_ASSIGN_OR_RETURN_FALSE(
        java_locale,
        JniHelper::CallStaticObjectMethod(jenv_, jni_cache_->locale_class.get(),
                                          jni_cache_->locale_for_language_tag,
                                          java_locale_str.get()));
  } else {
    // API level <21. We can't parse tags, so we just use the language.
    TC3_ASSIGN_OR_RETURN_FALSE(
        ScopedLocalRef<jstring> java_language_str,
        JniHelper::NewStringUTF(jenv_, GetFirstBcp47Tag(locale).c_str()));

    TC3_ASSIGN_OR_RETURN_FALSE(
        java_locale, JniHelper::NewObject(jenv_, jni_cache_->locale_class.get(),
                                          jni_cache_->locale_init_string,
                                          java_language_str.get()));
  }
  if (java_locale == nullptr) {
    TC3_LOG(ERROR) << "failed to get locale";
    return false;
  }

  // Get the calendar.
  TC3_ASSIGN_OR_RETURN_FALSE(
      calendar_, JniHelper::CallStaticObjectMethod(
                     jenv_, jni_cache_->calendar_class.get(),
                     jni_cache_->calendar_get_instance, java_time_zone.get(),
                     java_locale.get()));
  if (calendar_ == nullptr) {
    TC3_LOG(ERROR) << "failed to get calendar";
    return false;
  }

  // Set the time.
  if (!JniHelper::CallVoidMethod(jenv_, calendar_.get(),
                                 jni_cache_->calendar_set_time_in_millis,
                                 time_ms_utc)
           .ok()) {
    TC3_LOG(ERROR) << "failed to set time";
    return false;
  }
  return true;
}

bool Calendar::GetFirstDayOfWeek(int* value) const {
  if (!jni_cache_ || !jenv_ || !calendar_) return false;

  TC3_ASSIGN_OR_RETURN_FALSE(
      *value,
      JniHelper::CallIntMethod(jenv_, calendar_.get(),
                               jni_cache_->calendar_get_first_day_of_week));
  return true;
}

bool Calendar::GetTimeInMillis(int64* value) const {
  if (!jni_cache_ || !jenv_ || !calendar_) return false;

  TC3_ASSIGN_OR_RETURN_FALSE(
      *value,
      JniHelper::CallLongMethod(jenv_, calendar_.get(),
                                jni_cache_->calendar_get_time_in_millis));

  return true;
}

CalendarLib::CalendarLib() {
  TC3_LOG(FATAL) << "Java ICU CalendarLib must be initialized with a JniCache.";
}

CalendarLib::CalendarLib(const std::shared_ptr<JniCache>& jni_cache)
    : jni_cache_(jni_cache) {}

// Below is the boilerplate code for implementing the specialisations of
// get/set/add for the various field types.
#define TC3_DEFINE_FIELD_ACCESSOR(NAME, FIELD, KIND, TYPE)      \
  bool Calendar::KIND##NAME(TYPE value) const {                 \
    if (!jni_cache_ || !jenv_ || !calendar_) return false;      \
    return Calendar##KIND(jni_cache_, jenv_, calendar_.get(),   \
                          jni_cache_->calendar_##FIELD, value); \
  }
#define TC3_DEFINE_ADD(NAME, CONST) \
  TC3_DEFINE_FIELD_ACCESSOR(NAME, CONST, Add, int)
#define TC3_DEFINE_SET(NAME, CONST) \
  TC3_DEFINE_FIELD_ACCESSOR(NAME, CONST, Set, int)
#define TC3_DEFINE_GET(NAME, CONST) \
  TC3_DEFINE_FIELD_ACCESSOR(NAME, CONST, Get, int*)

TC3_DEFINE_ADD(Second, second)
TC3_DEFINE_ADD(Minute, minute)
TC3_DEFINE_ADD(HourOfDay, hour_of_day)
TC3_DEFINE_ADD(DayOfMonth, day_of_month)
TC3_DEFINE_ADD(Year, year)
TC3_DEFINE_ADD(Month, month)
TC3_DEFINE_GET(DayOfWeek, day_of_week)
TC3_DEFINE_SET(ZoneOffset, zone_offset)
TC3_DEFINE_SET(DstOffset, dst_offset)
TC3_DEFINE_SET(Year, year)
TC3_DEFINE_SET(Month, month)
TC3_DEFINE_SET(DayOfYear, day_of_year)
TC3_DEFINE_SET(DayOfMonth, day_of_month)
TC3_DEFINE_SET(DayOfWeek, day_of_week)
TC3_DEFINE_SET(HourOfDay, hour_of_day)
TC3_DEFINE_SET(Minute, minute)
TC3_DEFINE_SET(Second, second)
TC3_DEFINE_SET(Millisecond, millisecond)

#undef TC3_DEFINE_FIELD_ACCESSOR
#undef TC3_DEFINE_ADD
#undef TC3_DEFINE_SET
#undef TC3_DEFINE_GET

}  // namespace libtextclassifier3
