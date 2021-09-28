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

#include "utils/grammar/rules-utils.h"

#include <vector>

#include "utils/grammar/match.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace libtextclassifier3::grammar {
namespace {

using testing::ElementsAre;
using testing::Value;

// Create test match object.
Match CreateMatch(const CodepointIndex begin, const CodepointIndex end) {
  Match match;
  match.Init(0, CodepointSpan{begin, end},
             /*arg_match_offset=*/begin);
  return match;
}

MATCHER_P(IsDerivation, candidate, "") {
  return Value(arg.rule_id, candidate.rule_id) &&
         Value(arg.match, candidate.match);
}

TEST(UtilsTest, DeduplicatesMatches) {
  // Overlapping matches from the same rule.
  Match matches[] = {CreateMatch(0, 1), CreateMatch(1, 2), CreateMatch(0, 2)};
  const std::vector<Derivation> candidates = {{&matches[0], /*rule_id=*/0},
                                              {&matches[1], /*rule_id=*/0},
                                              {&matches[2], /*rule_id=*/0}};

  // Keep longest.
  EXPECT_THAT(DeduplicateDerivations(candidates),
              ElementsAre(IsDerivation(candidates[2])));
}

TEST(UtilsTest, DeduplicatesMatchesPerRule) {
  // Overlapping matches from different rules.
  Match matches[] = {CreateMatch(0, 1), CreateMatch(1, 2), CreateMatch(0, 2)};
  const std::vector<Derivation> candidates = {{&matches[0], /*rule_id=*/0},
                                              {&matches[1], /*rule_id=*/0},
                                              {&matches[2], /*rule_id=*/0},
                                              {&matches[0], /*rule_id=*/1}};

  // Keep longest for rule 0, but also keep match from rule 1.
  EXPECT_THAT(
      DeduplicateDerivations(candidates),
      ElementsAre(IsDerivation(candidates[2]), IsDerivation(candidates[3])));
}

TEST(UtilsTest, KeepNonoverlapping) {
  // Non-overlapping matches.
  Match matches[] = {CreateMatch(0, 1), CreateMatch(1, 2), CreateMatch(2, 3)};
  const std::vector<Derivation> candidates = {{&matches[0], /*rule_id=*/0},
                                              {&matches[1], /*rule_id=*/0},
                                              {&matches[2], /*rule_id=*/0}};

  // Keep all matches.
  EXPECT_THAT(
      DeduplicateDerivations(candidates),
      ElementsAre(IsDerivation(candidates[0]), IsDerivation(candidates[1]),
                  IsDerivation(candidates[2])));
}

}  // namespace
}  // namespace libtextclassifier3::grammar
