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

#include "utils/sentencepiece/normalizer.h"

#include "utils/base/logging.h"
#include "utils/strings/utf8.h"

namespace libtextclassifier3 {

bool SentencePieceNormalizer::Normalize(StringPiece input,
                                        std::string* normalized_input) const {
  // Ignores heading space.
  if (remove_extra_whitespaces_) {
    while (!input.empty()) {
      std::pair<StringPiece, int> suffix_and_length;
      if (!NormalizePrefix(input, &suffix_and_length)) {
        TC3_LOG(ERROR) << "Couldn't find match in normalization table.";
        return false;
      }
      if (suffix_and_length.second <= 0) {
        TC3_LOG(ERROR) << "Consumed string is empty.";
        return false;
      }
      if (suffix_and_length.first.size() != 1 ||
          suffix_and_length.first[0] != ' ') {
        break;
      }
      input.RemovePrefix(suffix_and_length.second);
    }
  }

  if (input.empty()) {
    *normalized_input = "";
    return true;
  }

  // Reserves the output buffer to avoid re-allocations.
  const int kReservedSize = input.size() * 3;
  normalized_input->reserve(kReservedSize);

  // Replaces white space with U+2581 (LOWER ONE EIGHT BLOCK)
  // if escape_whitespaces() is set (default = true).
  const StringPiece kSpaceSymbol = "\xe2\x96\x81";

  // Adds a space symbol as a prefix (default is true)
  // With this prefix, "world" and "hello world" are converted into
  // "_world" and "_hello_world", which help the trainer to extract
  // "_world" as one symbol.
  if (add_dummy_prefix_) {
    if (escape_whitespaces_) {
      normalized_input->append(kSpaceSymbol.data(), kSpaceSymbol.size());
    } else {
      normalized_input->append(" ");
    }
  }

  bool is_prev_space = remove_extra_whitespaces_;
  while (!input.empty()) {
    std::pair<StringPiece, int> p;
    if (!NormalizePrefix(input, &p)) {
      TC3_LOG(ERROR) << "Couldn't normalize string.";
      return false;
    }
    if (p.second <= 0) {
      TC3_LOG(ERROR) << "Consumed string is empty.";
      return false;
    }

    StringPiece sp = p.first;

    // Removes heading spaces in sentence piece,
    // if the previous sentence piece ends with whitespace.
    while (is_prev_space && ConsumePrefix(&sp, " ")) {
    }

    if (!sp.empty()) {
      const char* data = sp.data();
      for (int n = 0; n < sp.size(); ++n) {
        if (escape_whitespaces_ && data[n] == ' ') {
          normalized_input->append(kSpaceSymbol.data(), kSpaceSymbol.size());
        } else {
          *normalized_input += data[n];
        }
      }
      // Checks whether the last character of sp is whitespace.
      is_prev_space = EndsWith(sp, " ");
    }
    input.RemovePrefix(p.second);
    is_prev_space = is_prev_space && remove_extra_whitespaces_;
  }

  // Ignores tailing space.
  if (remove_extra_whitespaces_) {
    const StringPiece space = escape_whitespaces_ ? kSpaceSymbol : " ";
    while (EndsWith(*normalized_input, space)) {
      const int length = normalized_input->size() - space.size();
      normalized_input->resize(length);
    }
  }
  return true;
}

bool SentencePieceNormalizer::NormalizePrefix(
    StringPiece input, std::pair<StringPiece, int>* prefix) const {
  if (input.empty()) return true;
  StringSet::Match match;
  if (!charsmap_trie_.LongestPrefixMatch(input, &match)) {
    TC3_LOG(ERROR) << "Couldn't find match in normalization table.";
    return false;
  }
  const bool no_match = match.match_length <= 0;
  if (no_match) {
    const int char_length = ValidUTF8CharLength(input.data(), input.size());
    if (char_length <= 0) {
      // Found a malformed utf8.
      // The rune is set to be 0xFFFD (REPLACEMENT CHARACTER),
      // which is a valid Unicode of three bytes in utf8,
      // but here we only consume one byte.
      static const char kReplacementChar[] = "\xEF\xBF\xBD";
      prefix->first = StringPiece(kReplacementChar, 3);
      prefix->second = 1;  // Consumes 1 byte, buts emit 0xFFFD.
    } else {
      prefix->first = StringPiece(input.data(), char_length);
      prefix->second = char_length;
    }
  } else {
    if (match.id < 0 || match.id >= charsmap_normalized_.size()) {
      TC3_LOG(ERROR) << "Invalid entry in normalization table.";
      return false;
    }
    prefix->first = StringPiece(&charsmap_normalized_.data()[match.id]);
    prefix->second = match.match_length;
  }
  return true;
}

}  // namespace libtextclassifier3
