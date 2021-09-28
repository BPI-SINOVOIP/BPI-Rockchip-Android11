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

// Methods for string normalization.

#ifndef LIBTEXTCLASSIFIER_UTILS_NORMALIZATION_H_
#define LIBTEXTCLASSIFIER_UTILS_NORMALIZATION_H_

#include "utils/base/integral_types.h"
#include "utils/normalization_generated.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3 {

// Normalizes a text according to the options.
UnicodeText NormalizeText(const UniLib& unilib,
                          const NormalizationOptions* normalization_options,
                          const UnicodeText& text);

// Normalizes a text codepoint wise by applying each codepoint wise op in
// `codepointwise_ops` that is interpreted as a set of
// `CodepointwiseNormalizationOp`.
UnicodeText NormalizeTextCodepointWise(const UniLib& unilib,
                                       const uint32 codepointwise_ops,
                                       const UnicodeText& text);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_NORMALIZATION_H_
