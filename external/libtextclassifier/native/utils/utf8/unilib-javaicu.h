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

// An implementation of Unilib that uses Android Java interfaces via JNI. The
// performance critical ops have been re-implemented in C++.
// Specifically, this class must be compatible with API level 14 (ICS).

#ifndef LIBTEXTCLASSIFIER_UTILS_UTF8_UNILIB_JAVAICU_H_
#define LIBTEXTCLASSIFIER_UTILS_UTF8_UNILIB_JAVAICU_H_

#include <jni.h>

#include <memory>
#include <mutex>  // NOLINT
#include <string>

#include "utils/base/integral_types.h"
#include "utils/java/jni-base.h"
#include "utils/java/jni-cache.h"
#include "utils/java/jni-helper.h"
#include "utils/java/string_utils.h"
#include "utils/utf8/unicodetext.h"

namespace libtextclassifier3 {

class UniLibBase {
 public:
  UniLibBase();
  explicit UniLibBase(const std::shared_ptr<JniCache>& jni_cache);

  bool ParseInt32(const UnicodeText& text, int32* result) const;
  bool ParseInt64(const UnicodeText& text, int64* result) const;
  bool ParseDouble(const UnicodeText& text, double* result) const;

  bool IsOpeningBracket(char32 codepoint) const;
  bool IsClosingBracket(char32 codepoint) const;
  bool IsWhitespace(char32 codepoint) const;
  bool IsDigit(char32 codepoint) const;
  bool IsLower(char32 codepoint) const;
  bool IsUpper(char32 codepoint) const;
  bool IsPunctuation(char32 codepoint) const;

  char32 ToLower(char32 codepoint) const;
  char32 ToUpper(char32 codepoint) const;
  char32 GetPairedBracket(char32 codepoint) const;

  // Forward declaration for friend.
  class RegexPattern;

  class RegexMatcher {
   public:
    static constexpr int kError = -1;
    static constexpr int kNoError = 0;

    // Checks whether the input text matches the pattern exactly.
    bool Matches(int* status) const;

    // Approximate Matches() implementation implemented using Find(). It uses
    // the first Find() result and then checks that it spans the whole input.
    // NOTE: Unlike Matches() it can result in false negatives.
    // NOTE: Resets the matcher, so the current Find() state will be lost.
    bool ApproximatelyMatches(int* status);

    // Finds occurrences of the pattern in the input text.
    // Can be called repeatedly to find all occurrences. A call will update
    // internal state, so that 'Start', 'End' and 'Group' can be called to get
    // information about the match.
    // NOTE: Any call to ApproximatelyMatches() in between Find() calls will
    // modify the state.
    bool Find(int* status);

    // Gets the start offset of the last match (from  'Find').
    // Sets status to 'kError' if 'Find'
    // was not called previously.
    int Start(int* status) const;

    // Gets the start offset of the specified group of the last match.
    // (from  'Find').
    // Sets status to 'kError' if an invalid group was specified or if 'Find'
    // was not called previously.
    int Start(int group_idx, int* status) const;

    // Gets the end offset of the last match (from  'Find').
    // Sets status to 'kError' if 'Find'
    // was not called previously.
    int End(int* status) const;

    // Gets the end offset of the specified group of the last match.
    // (from  'Find').
    // Sets status to 'kError' if an invalid group was specified or if 'Find'
    // was not called previously.
    int End(int group_idx, int* status) const;

    // Gets the text of the last match (from 'Find').
    // Sets status to 'kError' if 'Find' was not called previously.
    UnicodeText Group(int* status) const;

    // Gets the text of the specified group of the last match (from 'Find').
    // Sets status to 'kError' if an invalid group was specified or if 'Find'
    // was not called previously.
    UnicodeText Group(int group_idx, int* status) const;

    // Returns the matched text (the 0th capturing group).
    std::string Text() const {
      ScopedStringChars text_str =
          GetScopedStringChars(jni_cache_->GetEnv(), text_.get());
      return text_str.get();
    }

   private:
    friend class RegexPattern;
    RegexMatcher(const JniCache* jni_cache, ScopedGlobalRef<jobject> matcher,
                 ScopedGlobalRef<jstring> text);
    bool UpdateLastFindOffset() const;

    const JniCache* jni_cache_;
    ScopedGlobalRef<jobject> matcher_;
    ScopedGlobalRef<jstring> text_;
    mutable int last_find_offset_ = 0;
    mutable int last_find_offset_codepoints_ = 0;
    mutable bool last_find_offset_dirty_ = true;
  };

  class RegexPattern {
   public:
    std::unique_ptr<RegexMatcher> Matcher(const UnicodeText& context) const;

   private:
    friend class UniLibBase;
    RegexPattern(const JniCache* jni_cache, const UnicodeText& pattern,
                 bool lazy);
    Status LockedInitializeIfNotAlready() const;

    const JniCache* jni_cache_;

    // These members need to be mutable because of the lazy initialization.
    // NOTE: The Matcher method first ensures (using a lock) that the
    // initialization was attempted (by using LockedInitializeIfNotAlready) and
    // then can access them without locking.
    mutable std::mutex mutex_;
    mutable ScopedGlobalRef<jobject> pattern_;
    mutable bool initialized_;
    mutable bool initialization_failure_;
    mutable UnicodeText pattern_text_;
  };

  class BreakIterator {
   public:
    int Next();

    static constexpr int kDone = -1;

   private:
    friend class UniLibBase;
    BreakIterator(const JniCache* jni_cache, const UnicodeText& text);

    const JniCache* jni_cache_;
    ScopedGlobalRef<jstring> text_;
    ScopedGlobalRef<jobject> iterator_;
    int last_break_index_;
    int last_unicode_index_;
  };

  std::unique_ptr<RegexPattern> CreateRegexPattern(
      const UnicodeText& regex) const;
  std::unique_ptr<RegexPattern> CreateLazyRegexPattern(
      const UnicodeText& regex) const;
  std::unique_ptr<BreakIterator> CreateBreakIterator(
      const UnicodeText& text) const;

 private:
  template <class T>
  bool ParseInt(const UnicodeText& text, T* result) const;

  std::shared_ptr<JniCache> jni_cache_;
};

template <class T>
bool UniLibBase::ParseInt(const UnicodeText& text, T* result) const {
  if (!jni_cache_) {
    return false;
  }

  JNIEnv* env = jni_cache_->GetEnv();
  TC3_ASSIGN_OR_RETURN_FALSE(const ScopedLocalRef<jstring> text_java,
                             jni_cache_->ConvertToJavaString(text));
  TC3_ASSIGN_OR_RETURN_FALSE(
      *result, JniHelper::CallStaticIntMethod<T>(
                   env, jni_cache_->integer_class.get(),
                   jni_cache_->integer_parse_int, text_java.get()));
  return true;
}

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_UTF8_UNILIB_JAVAICU_H_
