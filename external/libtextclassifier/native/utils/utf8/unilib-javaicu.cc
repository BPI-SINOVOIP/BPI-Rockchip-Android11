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

#include "utils/utf8/unilib-javaicu.h"

#include <math.h>

#include <cassert>
#include <cctype>
#include <map>

#include "utils/base/logging.h"
#include "utils/base/statusor.h"
#include "utils/java/jni-base.h"
#include "utils/java/string_utils.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib-common.h"

namespace libtextclassifier3 {

UniLibBase::UniLibBase() {
  TC3_LOG(FATAL) << "Java ICU UniLib must be initialized with a JniCache.";
}

UniLibBase::UniLibBase(const std::shared_ptr<JniCache>& jni_cache)
    : jni_cache_(jni_cache) {}

bool UniLibBase::IsOpeningBracket(char32 codepoint) const {
  return libtextclassifier3::IsOpeningBracket(codepoint);
}

bool UniLibBase::IsClosingBracket(char32 codepoint) const {
  return libtextclassifier3::IsClosingBracket(codepoint);
}

bool UniLibBase::IsWhitespace(char32 codepoint) const {
  return libtextclassifier3::IsWhitespace(codepoint);
}

bool UniLibBase::IsDigit(char32 codepoint) const {
  return libtextclassifier3::IsDigit(codepoint);
}

bool UniLibBase::IsLower(char32 codepoint) const {
  return libtextclassifier3::IsLower(codepoint);
}

bool UniLibBase::IsUpper(char32 codepoint) const {
  return libtextclassifier3::IsUpper(codepoint);
}

bool UniLibBase::IsPunctuation(char32 codepoint) const {
  return libtextclassifier3::IsPunctuation(codepoint);
}

char32 UniLibBase::ToLower(char32 codepoint) const {
  return libtextclassifier3::ToLower(codepoint);
}

char32 UniLibBase::ToUpper(char32 codepoint) const {
  return libtextclassifier3::ToUpper(codepoint);
}

char32 UniLibBase::GetPairedBracket(char32 codepoint) const {
  return libtextclassifier3::GetPairedBracket(codepoint);
}

// -----------------------------------------------------------------------------
// Implementations that call out to JVM. Behold the beauty.
// -----------------------------------------------------------------------------

bool UniLibBase::ParseInt32(const UnicodeText& text, int32* result) const {
  return ParseInt(text, result);
}

bool UniLibBase::ParseInt64(const UnicodeText& text, int64* result) const {
  return ParseInt(text, result);
}

bool UniLibBase::ParseDouble(const UnicodeText& text, double* result) const {
  if (!jni_cache_) {
    return false;
  }

  JNIEnv* env = jni_cache_->GetEnv();
  auto it_dot = text.begin();
  for (; it_dot != text.end() && !IsDot(*it_dot); it_dot++) {
  }

  int64 integer_part;
  if (!ParseInt(UnicodeText::Substring(text.begin(), it_dot, /*do_copy=*/false),
                &integer_part)) {
    return false;
  }

  int64 fractional_part = 0;
  if (it_dot != text.end()) {
    std::string fractional_part_str =
        UnicodeText::UTF8Substring(++it_dot, text.end());
    TC3_ASSIGN_OR_RETURN_FALSE(
        const ScopedLocalRef<jstring> fractional_text_java,
        jni_cache_->ConvertToJavaString(fractional_part_str));
    TC3_ASSIGN_OR_RETURN_FALSE(
        fractional_part,
        JniHelper::CallStaticIntMethod<int64>(
            env, jni_cache_->integer_class.get(), jni_cache_->integer_parse_int,
            fractional_text_java.get()));
  }

  double factional_part_double = fractional_part;
  while (factional_part_double >= 1) {
    factional_part_double /= 10;
  }
  *result = integer_part + factional_part_double;

  return true;
}

std::unique_ptr<UniLibBase::RegexPattern> UniLibBase::CreateRegexPattern(
    const UnicodeText& regex) const {
  return std::unique_ptr<UniLibBase::RegexPattern>(
      new UniLibBase::RegexPattern(jni_cache_.get(), regex, /*lazy=*/false));
}

std::unique_ptr<UniLibBase::RegexPattern> UniLibBase::CreateLazyRegexPattern(
    const UnicodeText& regex) const {
  return std::unique_ptr<UniLibBase::RegexPattern>(
      new UniLibBase::RegexPattern(jni_cache_.get(), regex, /*lazy=*/true));
}

UniLibBase::RegexPattern::RegexPattern(const JniCache* jni_cache,
                                       const UnicodeText& pattern, bool lazy)
    : jni_cache_(jni_cache),
      pattern_(nullptr, jni_cache ? jni_cache->jvm : nullptr),
      initialized_(false),
      initialization_failure_(false),
      pattern_text_(pattern) {
  if (!lazy) {
    LockedInitializeIfNotAlready();
  }
}

Status UniLibBase::RegexPattern::LockedInitializeIfNotAlready() const {
  std::lock_guard<std::mutex> guard(mutex_);
  if (initialized_ || initialization_failure_) {
    return Status::OK;
  }

  if (jni_cache_) {
    JNIEnv* jenv = jni_cache_->GetEnv();
    initialization_failure_ = true;
    TC3_ASSIGN_OR_RETURN(ScopedLocalRef<jstring> regex_java,
                         jni_cache_->ConvertToJavaString(pattern_text_));
    TC3_ASSIGN_OR_RETURN(ScopedLocalRef<jobject> pattern,
                         JniHelper::CallStaticObjectMethod(
                             jenv, jni_cache_->pattern_class.get(),
                             jni_cache_->pattern_compile, regex_java.get()));
    pattern_ = MakeGlobalRef(pattern.get(), jenv, jni_cache_->jvm);
    if (pattern_ == nullptr) {
      return Status::UNKNOWN;
    }

    initialization_failure_ = false;
    initialized_ = true;
    pattern_text_.clear();  // We don't need this anymore.
  }
  return Status::OK;
}

constexpr int UniLibBase::RegexMatcher::kError;
constexpr int UniLibBase::RegexMatcher::kNoError;

std::unique_ptr<UniLibBase::RegexMatcher> UniLibBase::RegexPattern::Matcher(
    const UnicodeText& context) const {
  LockedInitializeIfNotAlready();  // Possibly lazy initialization.
  if (initialization_failure_) {
    return nullptr;
  }

  if (jni_cache_) {
    JNIEnv* env = jni_cache_->GetEnv();
    const StatusOr<ScopedLocalRef<jstring>> status_or_context_java =
        jni_cache_->ConvertToJavaString(context);
    if (!status_or_context_java.ok() || !status_or_context_java.ValueOrDie()) {
      return nullptr;
    }
    const StatusOr<ScopedLocalRef<jobject>> status_or_matcher =
        JniHelper::CallObjectMethod(env, pattern_.get(),
                                    jni_cache_->pattern_matcher,
                                    status_or_context_java.ValueOrDie().get());
    if (jni_cache_->ExceptionCheckAndClear() || !status_or_matcher.ok() ||
        !status_or_matcher.ValueOrDie()) {
      return nullptr;
    }
    return std::unique_ptr<UniLibBase::RegexMatcher>(new RegexMatcher(
        jni_cache_,
        MakeGlobalRef(status_or_matcher.ValueOrDie().get(), env,
                      jni_cache_->jvm),
        MakeGlobalRef(status_or_context_java.ValueOrDie().get(), env,
                      jni_cache_->jvm)));
  } else {
    // NOTE: A valid object needs to be created here to pass the interface
    // tests.
    return std::unique_ptr<UniLibBase::RegexMatcher>(
        new RegexMatcher(jni_cache_, {}, {}));
  }
}

UniLibBase::RegexMatcher::RegexMatcher(const JniCache* jni_cache,
                                       ScopedGlobalRef<jobject> matcher,
                                       ScopedGlobalRef<jstring> text)
    : jni_cache_(jni_cache),
      matcher_(std::move(matcher)),
      text_(std::move(text)) {}

bool UniLibBase::RegexMatcher::Matches(int* status) const {
  if (jni_cache_) {
    *status = kNoError;
    const bool result = jni_cache_->GetEnv()->CallBooleanMethod(
        matcher_.get(), jni_cache_->matcher_matches);
    if (jni_cache_->ExceptionCheckAndClear()) {
      *status = kError;
      return false;
    }
    return result;
  } else {
    *status = kError;
    return false;
  }
}

bool UniLibBase::RegexMatcher::ApproximatelyMatches(int* status) {
  *status = kNoError;

  jni_cache_->GetEnv()->CallObjectMethod(matcher_.get(),
                                         jni_cache_->matcher_reset);
  if (jni_cache_->ExceptionCheckAndClear()) {
    *status = kError;
    return kError;
  }

  if (!Find(status) || *status != kNoError) {
    return false;
  }

  const int found_start = jni_cache_->GetEnv()->CallIntMethod(
      matcher_.get(), jni_cache_->matcher_start_idx, 0);
  if (jni_cache_->ExceptionCheckAndClear()) {
    *status = kError;
    return kError;
  }

  const int found_end = jni_cache_->GetEnv()->CallIntMethod(
      matcher_.get(), jni_cache_->matcher_end_idx, 0);
  if (jni_cache_->ExceptionCheckAndClear()) {
    *status = kError;
    return kError;
  }

  int context_length_bmp = jni_cache_->GetEnv()->CallIntMethod(
      text_.get(), jni_cache_->string_length);
  if (jni_cache_->ExceptionCheckAndClear()) {
    *status = kError;
    return false;
  }

  if (found_start != 0 || found_end != context_length_bmp) {
    return false;
  }

  return true;
}

bool UniLibBase::RegexMatcher::UpdateLastFindOffset() const {
  if (!last_find_offset_dirty_) {
    return true;
  }

  const int find_offset = jni_cache_->GetEnv()->CallIntMethod(
      matcher_.get(), jni_cache_->matcher_start_idx, 0);
  if (jni_cache_->ExceptionCheckAndClear()) {
    return false;
  }

  const int codepoint_count = jni_cache_->GetEnv()->CallIntMethod(
      text_.get(), jni_cache_->string_code_point_count, last_find_offset_,
      find_offset);
  if (jni_cache_->ExceptionCheckAndClear()) {
    return false;
  }

  last_find_offset_codepoints_ += codepoint_count;
  last_find_offset_ = find_offset;
  last_find_offset_dirty_ = false;

  return true;
}

bool UniLibBase::RegexMatcher::Find(int* status) {
  if (jni_cache_) {
    const bool result = jni_cache_->GetEnv()->CallBooleanMethod(
        matcher_.get(), jni_cache_->matcher_find);
    if (jni_cache_->ExceptionCheckAndClear()) {
      *status = kError;
      return false;
    }

    last_find_offset_dirty_ = true;
    *status = kNoError;
    return result;
  } else {
    *status = kError;
    return false;
  }
}

int UniLibBase::RegexMatcher::Start(int* status) const {
  return Start(/*group_idx=*/0, status);
}

int UniLibBase::RegexMatcher::Start(int group_idx, int* status) const {
  if (jni_cache_) {
    *status = kNoError;

    if (!UpdateLastFindOffset()) {
      *status = kError;
      return kError;
    }

    const int java_index = jni_cache_->GetEnv()->CallIntMethod(
        matcher_.get(), jni_cache_->matcher_start_idx, group_idx);
    if (jni_cache_->ExceptionCheckAndClear()) {
      *status = kError;
      return kError;
    }

    // If the group didn't participate in the match the index is -1.
    if (java_index == -1) {
      return -1;
    }

    const int unicode_index = jni_cache_->GetEnv()->CallIntMethod(
        text_.get(), jni_cache_->string_code_point_count, last_find_offset_,
        java_index);
    if (jni_cache_->ExceptionCheckAndClear()) {
      *status = kError;
      return kError;
    }

    return unicode_index + last_find_offset_codepoints_;
  } else {
    *status = kError;
    return kError;
  }
}

int UniLibBase::RegexMatcher::End(int* status) const {
  return End(/*group_idx=*/0, status);
}

int UniLibBase::RegexMatcher::End(int group_idx, int* status) const {
  if (jni_cache_) {
    *status = kNoError;

    if (!UpdateLastFindOffset()) {
      *status = kError;
      return kError;
    }

    const int java_index = jni_cache_->GetEnv()->CallIntMethod(
        matcher_.get(), jni_cache_->matcher_end_idx, group_idx);
    if (jni_cache_->ExceptionCheckAndClear()) {
      *status = kError;
      return kError;
    }

    // If the group didn't participate in the match the index is -1.
    if (java_index == -1) {
      return -1;
    }

    const int unicode_index = jni_cache_->GetEnv()->CallIntMethod(
        text_.get(), jni_cache_->string_code_point_count, last_find_offset_,
        java_index);
    if (jni_cache_->ExceptionCheckAndClear()) {
      *status = kError;
      return kError;
    }

    return unicode_index + last_find_offset_codepoints_;
  } else {
    *status = kError;
    return kError;
  }
}

UnicodeText UniLibBase::RegexMatcher::Group(int* status) const {
  if (jni_cache_) {
    JNIEnv* jenv = jni_cache_->GetEnv();
    StatusOr<ScopedLocalRef<jstring>> status_or_java_result =
        JniHelper::CallObjectMethod<jstring>(jenv, matcher_.get(),
                                             jni_cache_->matcher_group);

    if (jni_cache_->ExceptionCheckAndClear() || !status_or_java_result.ok() ||
        !status_or_java_result.ValueOrDie()) {
      *status = kError;
      return UTF8ToUnicodeText("", /*do_copy=*/false);
    }

    std::string result;
    if (!JStringToUtf8String(jenv, status_or_java_result.ValueOrDie().get(),
                             &result)) {
      *status = kError;
      return UTF8ToUnicodeText("", /*do_copy=*/false);
    }
    *status = kNoError;
    return UTF8ToUnicodeText(result, /*do_copy=*/true);
  } else {
    *status = kError;
    return UTF8ToUnicodeText("", /*do_copy=*/false);
  }
}

UnicodeText UniLibBase::RegexMatcher::Group(int group_idx, int* status) const {
  if (jni_cache_) {
    JNIEnv* jenv = jni_cache_->GetEnv();

    StatusOr<ScopedLocalRef<jstring>> status_or_java_result =
        JniHelper::CallObjectMethod<jstring>(
            jenv, matcher_.get(), jni_cache_->matcher_group_idx, group_idx);
    if (jni_cache_->ExceptionCheckAndClear() || !status_or_java_result.ok()) {
      *status = kError;
      TC3_LOG(ERROR) << "Exception occurred";
      return UTF8ToUnicodeText("", /*do_copy=*/false);
    }

    // java_result is nullptr when the group did not participate in the match.
    // For these cases other UniLib implementations return empty string, and
    // the participation can be checked by checking if Start() == -1.
    if (!status_or_java_result.ValueOrDie()) {
      *status = kNoError;
      return UTF8ToUnicodeText("", /*do_copy=*/false);
    }

    std::string result;
    if (!JStringToUtf8String(jenv, status_or_java_result.ValueOrDie().get(),
                             &result)) {
      *status = kError;
      return UTF8ToUnicodeText("", /*do_copy=*/false);
    }
    *status = kNoError;
    return UTF8ToUnicodeText(result, /*do_copy=*/true);
  } else {
    *status = kError;
    return UTF8ToUnicodeText("", /*do_copy=*/false);
  }
}

constexpr int UniLibBase::BreakIterator::kDone;

UniLibBase::BreakIterator::BreakIterator(const JniCache* jni_cache,
                                         const UnicodeText& text)
    : jni_cache_(jni_cache),
      text_(nullptr, jni_cache ? jni_cache->jvm : nullptr),
      iterator_(nullptr, jni_cache ? jni_cache->jvm : nullptr),
      last_break_index_(0),
      last_unicode_index_(0) {
  if (jni_cache_) {
    JNIEnv* jenv = jni_cache_->GetEnv();
    StatusOr<ScopedLocalRef<jstring>> status_or_text =
        jni_cache_->ConvertToJavaString(text);
    if (!status_or_text.ok()) {
      return;
    }
    text_ =
        MakeGlobalRef(status_or_text.ValueOrDie().get(), jenv, jni_cache->jvm);
    if (!text_) {
      return;
    }

    StatusOr<ScopedLocalRef<jobject>> status_or_iterator =
        JniHelper::CallStaticObjectMethod(
            jenv, jni_cache->breakiterator_class.get(),
            jni_cache->breakiterator_getwordinstance,
            jni_cache->locale_us.get());
    if (!status_or_iterator.ok()) {
      return;
    }
    iterator_ = MakeGlobalRef(status_or_iterator.ValueOrDie().get(), jenv,
                              jni_cache->jvm);
    if (!iterator_) {
      return;
    }
    JniHelper::CallVoidMethod(jenv, iterator_.get(),
                              jni_cache->breakiterator_settext, text_.get());
  }
}

int UniLibBase::BreakIterator::Next() {
  if (jni_cache_) {
    const int break_index = jni_cache_->GetEnv()->CallIntMethod(
        iterator_.get(), jni_cache_->breakiterator_next);
    if (jni_cache_->ExceptionCheckAndClear() ||
        break_index == BreakIterator::kDone) {
      return BreakIterator::kDone;
    }

    const int token_unicode_length = jni_cache_->GetEnv()->CallIntMethod(
        text_.get(), jni_cache_->string_code_point_count, last_break_index_,
        break_index);
    if (jni_cache_->ExceptionCheckAndClear()) {
      return BreakIterator::kDone;
    }

    last_break_index_ = break_index;
    return last_unicode_index_ += token_unicode_length;
  }
  return BreakIterator::kDone;
}

std::unique_ptr<UniLibBase::BreakIterator> UniLibBase::CreateBreakIterator(
    const UnicodeText& text) const {
  return std::unique_ptr<UniLibBase::BreakIterator>(
      new UniLibBase::BreakIterator(jni_cache_.get(), text));
}

}  // namespace libtextclassifier3
