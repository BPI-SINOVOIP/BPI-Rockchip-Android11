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

#ifndef LIBTEXTCLASSIFIER_UTILS_STRINGS_UTF8_H_
#define LIBTEXTCLASSIFIER_UTILS_STRINGS_UTF8_H_

#include "utils/base/integral_types.h"

namespace libtextclassifier3 {

// Returns the length (number of bytes) of the Unicode code point starting at
// src, based on inspecting just that one byte.  Preconditions: src != NULL,
// *src can be read.
static inline int GetNumBytesForUTF8Char(const char *src) {
  // On most platforms, char is unsigned by default, but iOS is an exception.
  // The cast below makes sure we always interpret *src as an unsigned char.
  return "\1\1\1\1\1\1\1\1\1\1\1\1\2\2\3\4"
      [(*(reinterpret_cast<const unsigned char *>(src)) & 0xFF) >> 4];
}

// Returns true if this byte is a trailing UTF-8 byte (10xx xxxx)
static inline bool IsTrailByte(char x) {
  // return (x & 0xC0) == 0x80;
  // Since trail bytes are always in [0x80, 0xBF], we can optimize:
  return static_cast<signed char>(x) < -0x40;
}

// Returns true iff src points to a well-formed UTF-8 string.
bool IsValidUTF8(const char *src, int size);

// Returns byte length of the first valid codepoint in the string, otherwise -1
// if pointing to an ill-formed UTF-8 character.
int ValidUTF8CharLength(const char *src, int size);

// Helper to ensure that strings are not truncated in the middle of
// multi-byte UTF-8 characters.
// Given a string, and a position at which to truncate, returns the
// last position not after the provided cut point, that would truncate a
// full character.
int SafeTruncateLength(const char *str, int truncate_at);

// Gets a unicode codepoint from a valid utf8 encoding.
char32 ValidCharToRune(const char *str);

// Converts a valid codepoint to utf8.
// Returns the length of the encoding.
int ValidRuneToChar(const char32 rune, char *dest);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_STRINGS_UTF8_H_
