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

#ifndef LIBTEXTCLASSIFIER_ACTIONS_TEST_UTILS_H_
#define LIBTEXTCLASSIFIER_ACTIONS_TEST_UTILS_H_

#include <string>

#include "actions/actions_model_generated.h"
#include "utils/flatbuffers.h"
#include "gmock/gmock.h"

namespace libtextclassifier3 {

using testing::ExplainMatchResult;
using testing::Value;

// Create test entity data schema.
std::string TestEntityDataSchema();
void SetTestEntityDataSchema(ActionsModelT* test_model);

MATCHER_P(IsActionOfType, type, "") { return Value(arg.type, type); }
MATCHER_P(IsSmartReply, response_text, "") {
  return ExplainMatchResult(IsActionOfType("text_reply"), arg,
                            result_listener) &&
         Value(arg.response_text, response_text);
}
MATCHER_P(IsSpan, span, "") {
  return Value(arg.first, span.first) && Value(arg.second, span.second);
}
MATCHER_P3(IsActionSuggestionAnnotation, name, text, span, "") {
  return Value(arg.name, name) && Value(arg.span.text, text) &&
         ExplainMatchResult(IsSpan(span), arg.span.span, result_listener);
}

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ACTIONS_TEST_UTILS_H_
