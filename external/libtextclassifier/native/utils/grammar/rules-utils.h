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

// Auxiliary methods for using rules.

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_RULES_UTILS_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_RULES_UTILS_H_

#include <unordered_map>
#include <vector>

#include "utils/grammar/match.h"
#include "utils/grammar/rules_generated.h"
#include "utils/i18n/locale.h"

namespace libtextclassifier3::grammar {

// Parses the locales of each rules shard.
std::vector<std::vector<Locale>> ParseRulesLocales(const RulesSet* rules);

// Selects rules shards that match on any locale.
std::vector<const grammar::RulesSet_::Rules*> SelectLocaleMatchingShards(
    const RulesSet* rules,
    const std::vector<std::vector<Locale>>& shard_locales,
    const std::vector<Locale>& locales);

// Deduplicates rule derivations by containing overlap.
// The grammar system can output multiple candidates for optional parts.
// For example if a rule has an optional suffix, we
// will get two rule derivations when the suffix is present: one with and one
// without the suffix. We therefore deduplicate by containing overlap, viz. from
// two candidates we keep the longer one if it completely contains the shorter.
struct Derivation {
  const Match* match;
  int64 rule_id;
};
std::vector<Derivation> DeduplicateDerivations(
    const std::vector<Derivation>& derivations);

// Checks that all assertions of a match tree are fulfilled.
bool VerifyAssertions(const Match* match);

}  // namespace libtextclassifier3::grammar

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_RULES_UTILS_H_
