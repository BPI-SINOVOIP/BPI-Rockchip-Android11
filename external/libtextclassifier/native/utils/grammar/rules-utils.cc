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

namespace libtextclassifier3::grammar {

std::vector<std::vector<Locale>> ParseRulesLocales(const RulesSet* rules) {
  if (rules == nullptr || rules->rules() == nullptr) {
    return {};
  }
  std::vector<std::vector<Locale>> locales(rules->rules()->size());
  for (int i = 0; i < rules->rules()->size(); i++) {
    const grammar::RulesSet_::Rules* rules_shard = rules->rules()->Get(i);
    if (rules_shard->locale() == nullptr) {
      continue;
    }
    for (const LanguageTag* tag : *rules_shard->locale()) {
      locales[i].push_back(Locale::FromLanguageTag(tag));
    }
  }
  return locales;
}

std::vector<const grammar::RulesSet_::Rules*> SelectLocaleMatchingShards(
    const RulesSet* rules,
    const std::vector<std::vector<Locale>>& shard_locales,
    const std::vector<Locale>& locales) {
  std::vector<const grammar::RulesSet_::Rules*> shards;
  if (rules->rules() == nullptr) {
    return shards;
  }
  for (int i = 0; i < shard_locales.size(); i++) {
    if (shard_locales[i].empty() ||
        Locale::IsAnyLocaleSupported(locales,
                                     /*supported_locales=*/shard_locales[i],
                                     /*default_value=*/false)) {
      shards.push_back(rules->rules()->Get(i));
    }
  }
  return shards;
}

std::vector<Derivation> DeduplicateDerivations(
    const std::vector<Derivation>& derivations) {
  std::vector<Derivation> sorted_candidates = derivations;
  std::stable_sort(
      sorted_candidates.begin(), sorted_candidates.end(),
      [](const Derivation& a, const Derivation& b) {
        // Sort by id.
        if (a.rule_id != b.rule_id) {
          return a.rule_id < b.rule_id;
        }

        // Sort by increasing start.
        if (a.match->codepoint_span.first != b.match->codepoint_span.first) {
          return a.match->codepoint_span.first < b.match->codepoint_span.first;
        }

        // Sort by decreasing end.
        return a.match->codepoint_span.second > b.match->codepoint_span.second;
      });

  // Deduplicate by overlap.
  std::vector<Derivation> result;
  for (int i = 0; i < sorted_candidates.size(); i++) {
    const Derivation& candidate = sorted_candidates[i];
    bool eliminated = false;

    // Due to the sorting above, the candidate can only be completely
    // intersected by a match before it in the sorted order.
    for (int j = i - 1; j >= 0; j--) {
      if (sorted_candidates[j].rule_id != candidate.rule_id) {
        break;
      }
      if (sorted_candidates[j].match->codepoint_span.first <=
              candidate.match->codepoint_span.first &&
          sorted_candidates[j].match->codepoint_span.second >=
              candidate.match->codepoint_span.second) {
        eliminated = true;
        break;
      }
    }

    if (!eliminated) {
      result.push_back(candidate);
    }
  }
  return result;
}

bool VerifyAssertions(const Match* match) {
  bool result = true;
  grammar::Traverse(match, [&result](const Match* node) {
    if (node->type != Match::kAssertionMatch) {
      // Only validation if all checks so far passed.
      return result;
    }

    // Positive assertions are by definition fulfilled,
    // fail if the assertion is negative.
    if (static_cast<const AssertionMatch*>(node)->negative) {
      result = false;
    }
    return result;
  });
  return result;
}

}  // namespace libtextclassifier3::grammar
