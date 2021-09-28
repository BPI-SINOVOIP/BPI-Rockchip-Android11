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

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_CALLBACK_DELEGATE_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_CALLBACK_DELEGATE_H_

#include "utils/base/integral_types.h"
#include "utils/grammar/match.h"
#include "utils/grammar/rules_generated.h"
#include "utils/grammar/types.h"

namespace libtextclassifier3::grammar {

class Matcher;

// CallbackDelegate is an interface and default implementation used by the
// grammar matcher to dispatch rule matches.
class CallbackDelegate {
 public:
  virtual ~CallbackDelegate() = default;

  // This is called by the matcher whenever it finds a match for a rule to
  // which a  callback is attached.
  virtual void MatchFound(const Match* match, const CallbackId callback_id,
                          const int64 callback_param, Matcher* matcher) {}
};

}  // namespace libtextclassifier3::grammar

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_CALLBACK_DELEGATE_H_
