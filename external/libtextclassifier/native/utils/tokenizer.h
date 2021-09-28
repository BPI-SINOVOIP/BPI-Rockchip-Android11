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

#ifndef LIBTEXTCLASSIFIER_UTILS_TOKENIZER_H_
#define LIBTEXTCLASSIFIER_UTILS_TOKENIZER_H_

#include <string>
#include <vector>

#include "annotator/types.h"
#include "utils/base/integral_types.h"
#include "utils/codepoint-range.h"
#include "utils/tokenizer_generated.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3 {

const int kInvalidScript = -1;
const int kUnknownScript = -2;

// Tokenizer splits the input string into a sequence of tokens, according to
// the configuration.
class Tokenizer {
 public:
  // `codepoint_ranges`: Codepoint ranges that determine how different
  //      codepoints are tokenized. The ranges must not overlap.
  // `internal_tokenizer_codepoint_ranges`: Codepoint ranges that define which
  //      tokens should be re-tokenized with the internal tokenizer in the mixed
  //      tokenization mode.
  // `split_on_script_change`: Whether to consider a change of codepoint script
  //      in a sequence of characters as a token boundary. If True, will treat
  //      script change as a token boundary.
  // `icu_preserve_whitespace_tokens`: If true, will include empty tokens in the
  // output (in the ICU tokenization mode).
  // `preserve_floating_numbers`: If true (default), will keep dots between
  // digits together, not making separate tokens (in the LETTER_DIGIT
  // tokenization mode).
  Tokenizer(
      const TokenizationType type, const UniLib* unilib,
      const std::vector<const TokenizationCodepointRange*>& codepoint_ranges,
      const std::vector<const CodepointRange*>&
          internal_tokenizer_codepoint_ranges,
      const bool split_on_script_change,
      const bool icu_preserve_whitespace_tokens,
      const bool preserve_floating_numbers);

  Tokenizer(
      const TokenizationType type, const UniLib* unilib,
      const std::vector<const TokenizationCodepointRange*>& codepoint_ranges,
      const std::vector<const CodepointRange*>&
          internal_tokenizer_codepoint_ranges,
      const bool split_on_script_change,
      const bool icu_preserve_whitespace_tokens)
      : Tokenizer(type, unilib, codepoint_ranges,
                  internal_tokenizer_codepoint_ranges, split_on_script_change,
                  icu_preserve_whitespace_tokens,
                  /*preserve_floating_numbers=*/true) {}

  Tokenizer(
      const std::vector<const TokenizationCodepointRange*>& codepoint_ranges,
      const bool split_on_script_change)
      : Tokenizer(TokenizationType_INTERNAL_TOKENIZER, /*unilib=*/nullptr,
                  codepoint_ranges, /*internal_tokenizer_codepoint_ranges=*/{},
                  split_on_script_change,
                  /*icu_preserve_whitespace_tokens=*/false,
                  /*preserve_floating_numbers=*/true) {}

  // Describes the type of tokens used in the NumberTokenizer.
  enum NumberTokenType {
    INVALID_TOKEN_TYPE,
    NUMERICAL,
    TERM,
    WHITESPACE,
    SEPARATOR,
    NOT_SET
  };

  // Tokenizes the input string using the selected tokenization method.
  std::vector<Token> Tokenize(const std::string& text) const;

  // Same as above but takes UnicodeText.
  std::vector<Token> Tokenize(const UnicodeText& text_unicode) const;

 protected:
  // Finds the tokenization codepoint range config for given codepoint.
  // Internally uses binary search so should be O(log(# of codepoint_ranges)).
  const TokenizationCodepointRangeT* FindTokenizationRange(int codepoint) const;

  // Finds the role and script for given codepoint. If not found, DEFAULT_ROLE
  // and kUnknownScript are assigned.
  void GetScriptAndRole(char32 codepoint,
                        TokenizationCodepointRange_::Role* role,
                        int* script) const;

  // Tokenizes a substring of the unicode string, appending the resulting tokens
  // to the output vector. The resulting tokens have bounds relative to the full
  // string. Does nothing if the start of the span is negative.
  void TokenizeSubstring(const UnicodeText& unicode_text, CodepointSpan span,
                         std::vector<Token>* result) const;

  std::vector<Token> InternalTokenize(const UnicodeText& text_unicode) const;

  // Takes the result of ICU tokenization and retokenizes stretches of tokens
  // made of a specific subset of characters using the internal tokenizer.
  void InternalRetokenize(const UnicodeText& unicode_text,
                          std::vector<Token>* tokens) const;

  // Tokenizes the input text using ICU tokenizer.
  bool ICUTokenize(const UnicodeText& context_unicode,
                   std::vector<Token>* result) const;

  // Tokenizes the input in number, word and separator tokens.
  bool NumberTokenize(const UnicodeText& text_unicode,
                      std::vector<Token>* result) const;

 private:
  const TokenizationType type_;

  const UniLib* unilib_;

  // Codepoint ranges that determine how different codepoints are tokenized.
  // The ranges must not overlap.
  std::vector<std::unique_ptr<const TokenizationCodepointRangeT>>
      codepoint_ranges_;

  // Codepoint ranges that define which tokens (consisting of which codepoints)
  // should be re-tokenized with the internal tokenizer in the mixed
  // tokenization mode.
  // NOTE: Must be sorted.
  std::vector<CodepointRangeStruct> internal_tokenizer_codepoint_ranges_;

  // If true, tokens will be additionally split when the codepoint's script_id
  // changes.
  const bool split_on_script_change_;

  const bool icu_preserve_whitespace_tokens_;
  const bool preserve_floating_numbers_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_TOKENIZER_H_
