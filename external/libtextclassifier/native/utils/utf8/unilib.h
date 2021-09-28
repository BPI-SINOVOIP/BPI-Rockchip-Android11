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

#ifndef LIBTEXTCLASSIFIER_UTILS_UTF8_UNILIB_H_
#define LIBTEXTCLASSIFIER_UTILS_UTF8_UNILIB_H_

#include "utils/base/integral_types.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib-common.h"

#if defined TC3_UNILIB_ICU
#include "utils/utf8/unilib-icu.h"
#define INIT_UNILIB_FOR_TESTING(VAR) VAR()
#elif defined TC3_UNILIB_JAVAICU
#include "utils/utf8/unilib-javaicu.h"
#define INIT_UNILIB_FOR_TESTING(VAR) VAR(nullptr)
#elif defined TC3_UNILIB_APPLE
#include "utils/utf8/unilib-apple.h"
#define INIT_UNILIB_FOR_TESTING(VAR) VAR()
#elif defined TC3_UNILIB_DUMMY
#include "utils/utf8/unilib-dummy.h"
#define INIT_UNILIB_FOR_TESTING(VAR) VAR()
#else
#error No TC3_UNILIB implementation specified.
#endif

namespace libtextclassifier3 {

class UniLib : public UniLibBase {
 public:
  using UniLibBase::UniLibBase;

  // Lowercase a unicode string.
  UnicodeText ToLowerText(const UnicodeText& text) const {
    UnicodeText result;
    for (const char32 codepoint : text) {
      result.push_back(ToLower(codepoint));
    }
    return result;
  }

  // Uppercase a unicode string.
  UnicodeText ToUpperText(const UnicodeText& text) const {
    UnicodeText result;
    for (const char32 codepoint : text) {
      result.push_back(UniLibBase::ToUpper(codepoint));
    }
    return result;
  }

  bool IsLowerText(const UnicodeText& text) const {
    for (const char32 codepoint : text) {
      if (!IsLower(codepoint)) {
        return false;
      }
    }
    return true;
  }

  bool IsUpperText(const UnicodeText& text) const {
    for (const char32 codepoint : text) {
      if (!IsUpper(codepoint)) {
        return false;
      }
    }
    return true;
  }

  bool IsDigits(const UnicodeText& text) const {
    for (const char32 codepoint : text) {
      if (!IsDigit(codepoint)) {
        return false;
      }
    }
    return true;
  }

  bool IsPercentage(char32 codepoint) const {
    return libtextclassifier3::IsPercentage(codepoint);
  }

  bool IsSlash(char32 codepoint) const {
    return libtextclassifier3::IsSlash(codepoint);
  }

  bool IsMinus(char32 codepoint) const {
    return libtextclassifier3::IsMinus(codepoint);
  }

  bool IsNumberSign(char32 codepoint) const {
    return libtextclassifier3::IsNumberSign(codepoint);
  }

  bool IsDot(char32 codepoint) const {
    return libtextclassifier3::IsDot(codepoint);
  }

  bool IsLatinLetter(char32 codepoint) const {
    return libtextclassifier3::IsLatinLetter(codepoint);
  }

  bool IsArabicLetter(char32 codepoint) const {
    return libtextclassifier3::IsArabicLetter(codepoint);
  }

  bool IsCyrillicLetter(char32 codepoint) const {
    return libtextclassifier3::IsCyrillicLetter(codepoint);
  }

  bool IsChineseLetter(char32 codepoint) const {
    return libtextclassifier3::IsChineseLetter(codepoint);
  }

  bool IsJapaneseLetter(char32 codepoint) const {
    return libtextclassifier3::IsJapaneseLetter(codepoint);
  }

  bool IsKoreanLetter(char32 codepoint) const {
    return libtextclassifier3::IsKoreanLetter(codepoint);
  }

  bool IsThaiLetter(char32 codepoint) const {
    return libtextclassifier3::IsThaiLetter(codepoint);
  }

  bool IsCJTletter(char32 codepoint) const {
    return libtextclassifier3::IsCJTletter(codepoint);
  }

  bool IsLetter(char32 codepoint) const {
    return libtextclassifier3::IsLetter(codepoint);
  }
};

}  // namespace libtextclassifier3
#endif  // LIBTEXTCLASSIFIER_UTILS_UTF8_UNILIB_H_
