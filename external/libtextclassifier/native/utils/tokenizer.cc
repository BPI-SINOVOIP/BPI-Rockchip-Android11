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

#include "utils/tokenizer.h"

#include <algorithm>

#include "utils/base/logging.h"
#include "utils/base/macros.h"
#include "utils/strings/utf8.h"
#include "utils/utf8/unicodetext.h"

namespace libtextclassifier3 {

Tokenizer::Tokenizer(
    const TokenizationType type, const UniLib* unilib,
    const std::vector<const TokenizationCodepointRange*>& codepoint_ranges,
    const std::vector<const CodepointRange*>&
        internal_tokenizer_codepoint_ranges,
    const bool split_on_script_change,
    const bool icu_preserve_whitespace_tokens,
    const bool preserve_floating_numbers)
    : type_(type),
      unilib_(unilib),
      split_on_script_change_(split_on_script_change),
      icu_preserve_whitespace_tokens_(icu_preserve_whitespace_tokens),
      preserve_floating_numbers_(preserve_floating_numbers) {
  for (const TokenizationCodepointRange* range : codepoint_ranges) {
    codepoint_ranges_.emplace_back(range->UnPack());
  }

  std::sort(codepoint_ranges_.begin(), codepoint_ranges_.end(),
            [](const std::unique_ptr<const TokenizationCodepointRangeT>& a,
               const std::unique_ptr<const TokenizationCodepointRangeT>& b) {
              return a->start < b->start;
            });

  SortCodepointRanges(internal_tokenizer_codepoint_ranges,
                      &internal_tokenizer_codepoint_ranges_);
}

const TokenizationCodepointRangeT* Tokenizer::FindTokenizationRange(
    int codepoint) const {
  auto it = std::lower_bound(
      codepoint_ranges_.begin(), codepoint_ranges_.end(), codepoint,
      [](const std::unique_ptr<const TokenizationCodepointRangeT>& range,
         int codepoint) {
        // This function compares range with the codepoint for the purpose of
        // finding the first greater or equal range. Because of the use of
        // std::lower_bound it needs to return true when range < codepoint;
        // the first time it will return false the lower bound is found and
        // returned.
        //
        // It might seem weird that the condition is range.end <= codepoint
        // here but when codepoint == range.end it means it's actually just
        // outside of the range, thus the range is less than the codepoint.
        return range->end <= codepoint;
      });
  if (it != codepoint_ranges_.end() && (*it)->start <= codepoint &&
      (*it)->end > codepoint) {
    return it->get();
  } else {
    return nullptr;
  }
}

void Tokenizer::GetScriptAndRole(char32 codepoint,
                                 TokenizationCodepointRange_::Role* role,
                                 int* script) const {
  const TokenizationCodepointRangeT* range = FindTokenizationRange(codepoint);
  if (range) {
    *role = range->role;
    *script = range->script_id;
  } else {
    *role = TokenizationCodepointRange_::Role_DEFAULT_ROLE;
    *script = kUnknownScript;
  }
}

std::vector<Token> Tokenizer::Tokenize(const std::string& text) const {
  UnicodeText text_unicode = UTF8ToUnicodeText(text, /*do_copy=*/false);
  return Tokenize(text_unicode);
}

std::vector<Token> Tokenizer::Tokenize(const UnicodeText& text_unicode) const {
  switch (type_) {
    case TokenizationType_INTERNAL_TOKENIZER:
      return InternalTokenize(text_unicode);
    case TokenizationType_ICU:
      TC3_FALLTHROUGH_INTENDED;
    case TokenizationType_MIXED: {
      std::vector<Token> result;
      if (!ICUTokenize(text_unicode, &result)) {
        return {};
      }
      if (type_ == TokenizationType_MIXED) {
        InternalRetokenize(text_unicode, &result);
      }
      return result;
    }
    case TokenizationType_LETTER_DIGIT: {
      std::vector<Token> result;
      if (!NumberTokenize(text_unicode, &result)) {
        return {};
      }
      return result;
    }
    default:
      TC3_LOG(ERROR) << "Unknown tokenization type specified. Using internal.";
      return InternalTokenize(text_unicode);
  }
}

void AppendCodepointToToken(UnicodeText::const_iterator it, Token* token) {
  token->value += std::string(
      it.utf8_data(), it.utf8_data() + GetNumBytesForUTF8Char(it.utf8_data()));
}

std::vector<Token> Tokenizer::InternalTokenize(
    const UnicodeText& text_unicode) const {
  std::vector<Token> result;
  Token new_token("", 0, 0);
  int codepoint_index = 0;

  int last_script = kInvalidScript;
  for (auto it = text_unicode.begin(); it != text_unicode.end();
       ++it, ++codepoint_index) {
    TokenizationCodepointRange_::Role role;
    int script;
    GetScriptAndRole(*it, &role, &script);

    if (role & TokenizationCodepointRange_::Role_SPLIT_BEFORE ||
        (split_on_script_change_ && last_script != kInvalidScript &&
         last_script != script)) {
      if (!new_token.value.empty()) {
        result.push_back(new_token);
      }
      new_token = Token("", codepoint_index, codepoint_index);
    }
    if (!(role & TokenizationCodepointRange_::Role_DISCARD_CODEPOINT)) {
      new_token.end += 1;
      AppendCodepointToToken(it, &new_token);
    }
    if (role & TokenizationCodepointRange_::Role_SPLIT_AFTER) {
      if (!new_token.value.empty()) {
        result.push_back(new_token);
      }
      new_token = Token("", codepoint_index + 1, codepoint_index + 1);
    }

    last_script = script;
  }
  if (!new_token.value.empty()) {
    result.push_back(new_token);
  }

  return result;
}

void Tokenizer::TokenizeSubstring(const UnicodeText& unicode_text,
                                  CodepointSpan span,
                                  std::vector<Token>* result) const {
  if (span.first < 0) {
    // There is no span to tokenize.
    return;
  }

  // Extract the substring.
  UnicodeText text = UnicodeText::Substring(unicode_text, span.first,
                                            span.second, /*do_copy=*/false);

  // Run the tokenizer and update the token bounds to reflect the offset of the
  // substring.
  std::vector<Token> tokens = InternalTokenize(text);

  // Avoids progressive capacity increases in the for loop.
  result->reserve(result->size() + tokens.size());
  for (Token& token : tokens) {
    token.start += span.first;
    token.end += span.first;
    result->emplace_back(std::move(token));
  }
}

void Tokenizer::InternalRetokenize(const UnicodeText& unicode_text,
                                   std::vector<Token>* tokens) const {
  std::vector<Token> result;
  CodepointSpan span(-1, -1);
  for (Token& token : *tokens) {
    const UnicodeText unicode_token_value =
        UTF8ToUnicodeText(token.value, /*do_copy=*/false);
    bool should_retokenize = true;
    for (const int codepoint : unicode_token_value) {
      if (!IsCodepointInRanges(codepoint,
                               internal_tokenizer_codepoint_ranges_)) {
        should_retokenize = false;
        break;
      }
    }

    if (should_retokenize) {
      if (span.first < 0) {
        span.first = token.start;
      }
      span.second = token.end;
    } else {
      TokenizeSubstring(unicode_text, span, &result);
      span.first = -1;
      result.emplace_back(std::move(token));
    }
  }
  TokenizeSubstring(unicode_text, span, &result);

  *tokens = std::move(result);
}

bool Tokenizer::ICUTokenize(const UnicodeText& context_unicode,
                            std::vector<Token>* result) const {
  std::unique_ptr<UniLib::BreakIterator> break_iterator =
      unilib_->CreateBreakIterator(context_unicode);
  if (!break_iterator) {
    return false;
  }
  int last_break_index = 0;
  int break_index = 0;
  int last_unicode_index = 0;
  int unicode_index = 0;
  auto token_begin_it = context_unicode.begin();
  while ((break_index = break_iterator->Next()) !=
         UniLib::BreakIterator::kDone) {
    const int token_length = break_index - last_break_index;
    unicode_index = last_unicode_index + token_length;

    auto token_end_it = token_begin_it;
    std::advance(token_end_it, token_length);

    // Determine if the whole token is whitespace.
    bool is_whitespace = true;
    for (auto char_it = token_begin_it; char_it < token_end_it; ++char_it) {
      if (!unilib_->IsWhitespace(*char_it)) {
        is_whitespace = false;
        break;
      }
    }

    const std::string token =
        context_unicode.UTF8Substring(token_begin_it, token_end_it);

    if (!is_whitespace || icu_preserve_whitespace_tokens_) {
      result->push_back(Token(token, last_unicode_index, unicode_index,
                              /*is_padding=*/false, is_whitespace));
    }

    last_break_index = break_index;
    last_unicode_index = unicode_index;
    token_begin_it = token_end_it;
  }

  return true;
}

bool Tokenizer::NumberTokenize(const UnicodeText& text_unicode,
                               std::vector<Token>* result) const {
  Token new_token("", 0, 0);
  NumberTokenType current_token_type = NOT_SET;
  int codepoint_index = 0;

  auto PushToken = [&new_token, result]() {
    if (!new_token.value.empty()) {
      result->push_back(new_token);
    }
  };

  auto MaybeResetTokenAndAddChar =
      [&new_token, PushToken, &current_token_type](
          int codepoint_index, NumberTokenType token_type,
          UnicodeText::const_iterator it, bool is_whitespace = false) {
        if (current_token_type != token_type) {
          PushToken();
          new_token = Token("", codepoint_index, codepoint_index,
                            /*is_padding=*/false, is_whitespace);
        }
        new_token.end += 1;
        AppendCodepointToToken(it, &new_token);
        current_token_type = token_type;
      };

  auto FinishTokenAndAddSeparator =
      [&new_token, result, &current_token_type, PushToken](
          int codepoint_index, UnicodeText::const_iterator it) {
        PushToken();

        result->emplace_back("", codepoint_index, codepoint_index + 1);
        AppendCodepointToToken(it, &result->back());

        new_token = Token("", codepoint_index + 1, codepoint_index + 1);
        current_token_type = NOT_SET;
      };

  for (auto it = text_unicode.begin(); it != text_unicode.end();
       ++it, ++codepoint_index) {
    if (unilib_->IsDigit(*it)) {
      MaybeResetTokenAndAddChar(codepoint_index, NUMERICAL, it);
    } else if (unilib_->IsLetter(*it)) {
      MaybeResetTokenAndAddChar(codepoint_index, TERM, it);
    } else if (unilib_->IsWhitespace(*it)) {
      MaybeResetTokenAndAddChar(codepoint_index, WHITESPACE, it,
                                /*is_whitespace=*/true);
    } else if (unilib_->IsDot(*it) && preserve_floating_numbers_) {
      auto it_next = std::next(it);
      if (current_token_type == NUMERICAL && it_next != text_unicode.end() &&
          unilib_->IsDigit(*it_next)) {
        new_token.end += 1;
        AppendCodepointToToken(it, &new_token);
      } else {
        // If the current token is not a number or dot at the end or followed
        // by a non digit => separate token
        FinishTokenAndAddSeparator(codepoint_index, it);
      }
    } else {
      FinishTokenAndAddSeparator(codepoint_index, it);
    }
  }
  PushToken();

  return true;
}

}  // namespace libtextclassifier3
