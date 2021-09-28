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

#include "utils/test-utils.h"

#include <iterator>

#include "utils/codepoint-range.h"
#include "utils/strings/utf8.h"
#include "utils/utf8/unicodetext.h"

namespace libtextclassifier3 {

using libtextclassifier3::Token;

std::vector<Token> TokenizeOnSpace(const std::string& text) {
  return TokenizeOnDelimiters(text, {' '});
}

std::vector<Token> TokenizeOnDelimiters(
    const std::string& text, const std::unordered_set<char32>& delimiters) {
  const UnicodeText unicode_text = UTF8ToUnicodeText(text, /*do_copy=*/false);

  std::vector<Token> result;

  int token_start_codepoint = 0;
  auto token_start_it = unicode_text.begin();
  int codepoint_idx = 0;

  UnicodeText::const_iterator it;
  for (it = unicode_text.begin(); it < unicode_text.end(); it++) {
    if (delimiters.find(*it) != delimiters.end()) {
      // Only add a token when the string is non-empty.
      if (token_start_it != it) {
        result.push_back(Token{UnicodeText::UTF8Substring(token_start_it, it),
                               token_start_codepoint, codepoint_idx});
      }

      token_start_codepoint = codepoint_idx + 1;
      token_start_it = it;
      token_start_it++;
    }

    codepoint_idx++;
  }
  // Only add a token when the string is non-empty.
  if (token_start_it != it) {
    result.push_back(Token{UnicodeText::UTF8Substring(token_start_it, it),
                           token_start_codepoint, codepoint_idx});
  }

  return result;
}

}  // namespace  libtextclassifier3
