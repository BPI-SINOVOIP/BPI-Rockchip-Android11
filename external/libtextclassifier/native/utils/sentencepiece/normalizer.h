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

#ifndef LIBTEXTCLASSIFIER_UTILS_SENTENCEPIECE_NORMALIZER_H_
#define LIBTEXTCLASSIFIER_UTILS_SENTENCEPIECE_NORMALIZER_H_

#include <memory>
#include <string>

#include "utils/container/double-array-trie.h"
#include "utils/strings/stringpiece.h"

namespace libtextclassifier3 {

// Normalizer implements a simple text normalizer with user-defined
// string-to-string rules and leftmost longest matching.
class SentencePieceNormalizer {
 public:
  // charsmap_trie and charsmap_normalized specify the normalization/replacement
  // string-to-string rules in the following way:
  // A match in the trie for a string will return the offset in
  // charsmap_normalized that contains the replacement string.
  //
  // add_dummy_prefix: Whether to add dummy whitespace at the beginning of the
  //   text in order to treat "world" in "world" and "hello world" uniformly.
  //
  // remove_extra_whitespaces: Whether to remove leading, trailing and duplicate
  //   internal whitespace.
  //
  // escape_whitespaces: Whether to replace whitespace with a meta symbol.
  SentencePieceNormalizer(const DoubleArrayTrie& charsmap_trie,
                          StringPiece charsmap_normalized,
                          bool add_dummy_prefix = true,
                          bool remove_extra_whitespaces = true,
                          bool escape_whitespaces = true)
      : charsmap_trie_(charsmap_trie),
        charsmap_normalized_(charsmap_normalized),
        add_dummy_prefix_(add_dummy_prefix),
        remove_extra_whitespaces_(remove_extra_whitespaces),
        escape_whitespaces_(escape_whitespaces) {}

  // Normalizes a plain utf8 string into an internal representation for
  // Sentencepiece model.
  bool Normalize(StringPiece input, std::string* normalized_input) const;

 private:
  // Normalizes the prefix of `input` and returns the pair of
  // normalized prefix and the length of the prefix of `input` processed in the
  // normalization.
  bool NormalizePrefix(StringPiece input,
                       std::pair<StringPiece, int>* prefix) const;

  // Internal trie for efficient longest prefix string matching.
  DoubleArrayTrie charsmap_trie_;

  // "\0" delimitered concatenated normalized strings.
  // the value of `charsmap_trie_` stores offsets into this string.
  StringPiece charsmap_normalized_;

  const bool add_dummy_prefix_;
  const bool remove_extra_whitespaces_;
  const bool escape_whitespaces_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_SENTENCEPIECE_NORMALIZER_H_
