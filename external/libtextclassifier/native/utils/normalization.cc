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

#include "utils/normalization.h"

#include "utils/base/logging.h"
#include "utils/normalization_generated.h"

namespace libtextclassifier3 {

UnicodeText NormalizeText(const UniLib& unilib,
                          const NormalizationOptions* normalization_options,
                          const UnicodeText& text) {
  return NormalizeTextCodepointWise(
      unilib, normalization_options->codepointwise_normalization(), text);
}

UnicodeText NormalizeTextCodepointWise(const UniLib& unilib,
                                       const uint32 codepointwise_ops,
                                       const UnicodeText& text) {
  // Sanity check.
  TC3_CHECK(!((codepointwise_ops &
               NormalizationOptions_::CodepointwiseNormalizationOp_LOWERCASE) &&
              (codepointwise_ops &
               NormalizationOptions_::CodepointwiseNormalizationOp_UPPERCASE)));

  UnicodeText result;
  for (const char32 codepoint : text) {
    // Skip whitespace.
    if ((codepointwise_ops &
         NormalizationOptions_::CodepointwiseNormalizationOp_DROP_WHITESPACE) &&
        unilib.IsWhitespace(codepoint)) {
      continue;
    }

    // Skip punctuation.
    if ((codepointwise_ops &
         NormalizationOptions_::
             CodepointwiseNormalizationOp_DROP_PUNCTUATION) &&
        unilib.IsPunctuation(codepoint)) {
      continue;
    }

    int32 normalized_codepoint = codepoint;

    // Lower case.
    if (codepointwise_ops &
        NormalizationOptions_::CodepointwiseNormalizationOp_LOWERCASE) {
      normalized_codepoint = unilib.ToLower(normalized_codepoint);

      // Upper case.
    } else if (codepointwise_ops &
               NormalizationOptions_::CodepointwiseNormalizationOp_UPPERCASE) {
      normalized_codepoint = unilib.ToUpper(normalized_codepoint);
    }

    result.push_back(normalized_codepoint);
  }
  return result;
}

}  // namespace libtextclassifier3
