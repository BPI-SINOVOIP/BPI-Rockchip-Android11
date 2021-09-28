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

#ifndef LIBTEXTCLASSIFIER_UTILS_UTF8_UNILIB_COMMON_H_
#define LIBTEXTCLASSIFIER_UTILS_UTF8_UNILIB_COMMON_H_

#include "utils/base/integral_types.h"
#include "utils/utf8/unicodetext.h"

namespace libtextclassifier3 {

bool IsOpeningBracket(char32 codepoint);
bool IsClosingBracket(char32 codepoint);
bool IsWhitespace(char32 codepoint);
bool IsDigit(char32 codepoint);
bool IsLower(char32 codepoint);
bool IsUpper(char32 codepoint);
bool IsPunctuation(char32 codepoint);
bool IsPercentage(char32 codepoint);
bool IsSlash(char32 codepoint);
bool IsMinus(char32 codepoint);
bool IsNumberSign(char32 codepoint);
bool IsDot(char32 codepoint);

bool IsLatinLetter(char32 codepoint);
bool IsArabicLetter(char32 codepoint);
bool IsCyrillicLetter(char32 codepoint);
bool IsChineseLetter(char32 codepoint);
bool IsJapaneseLetter(char32 codepoint);
bool IsKoreanLetter(char32 codepoint);
bool IsThaiLetter(char32 codepoint);
bool IsLetter(char32 codepoint);
bool IsCJTletter(char32 codepoint);

char32 ToLower(char32 codepoint);
char32 ToUpper(char32 codepoint);
char32 GetPairedBracket(char32 codepoint);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_UTF8_UNILIB_COMMON_H_
