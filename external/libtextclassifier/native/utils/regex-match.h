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

#ifndef LIBTEXTCLASSIFIER_UTILS_REGEX_MATCH_H_
#define LIBTEXTCLASSIFIER_UTILS_REGEX_MATCH_H_

#include "utils/optional.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3 {

// Returns text of a capturing group if the capturing group was fulfilled in
// the regex match.
Optional<std::string> GetCapturingGroupText(const UniLib::RegexMatcher* matcher,
                                            const int group_id);

// Post-checks a regular expression match with a lua verifier script.
// The verifier can access:
//   * `context`: The context as a string.
//   * `match`: The groups of the regex match as an array, each group gives
//       * `begin`: span start
//       * `end`: span end
//       * `text`: the text
// The verifier is expected to return a boolean, indicating whether the
// verification succeeded or not.
// Returns true if the verification was successful, false if not.
bool VerifyMatch(const std::string& context,
                 const UniLib::RegexMatcher* matcher,
                 const std::string& lua_verifier_code);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_REGEX_MATCH_H_
