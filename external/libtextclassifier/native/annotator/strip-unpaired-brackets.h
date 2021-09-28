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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_STRIP_UNPAIRED_BRACKETS_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_STRIP_UNPAIRED_BRACKETS_H_

#include <string>

#include "annotator/types.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3 {
// If the first or the last codepoint of the given span is a bracket, the
// bracket is stripped if the span does not contain its corresponding paired
// version.
CodepointSpan StripUnpairedBrackets(const std::string& context,
                                    CodepointSpan span, const UniLib& unilib);

// Same as above but takes UnicodeText instance directly.
CodepointSpan StripUnpairedBrackets(const UnicodeText& context_unicode,
                                    CodepointSpan span, const UniLib& unilib);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_STRIP_UNPAIRED_BRACKETS_H_
