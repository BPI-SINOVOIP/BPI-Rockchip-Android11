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

#include "utils/grammar/utils/rules.h"

#include <set>

#include "utils/grammar/utils/ir.h"
#include "utils/strings/append.h"
#include "utils/strings/stringpiece.h"

namespace libtextclassifier3::grammar {
namespace {

// Returns whether a nonterminal is a pre-defined one.
bool IsPredefinedNonterminal(const std::string& nonterminal_name) {
  if (nonterminal_name == kStartNonterm || nonterminal_name == kEndNonterm ||
      nonterminal_name == kTokenNonterm || nonterminal_name == kDigitsNonterm ||
      nonterminal_name == kWordBreakNonterm) {
    return true;
  }
  for (int digits = 1; digits <= kMaxNDigitsNontermLength; digits++) {
    if (nonterminal_name == strings::StringPrintf(kNDigitsNonterm, digits)) {
      return true;
    }
  }
  return false;
}

// Gets an assigned Nonterm for a nonterminal or kUnassignedNonterm if not yet
// assigned.
Nonterm GetAssignedIdForNonterminal(
    const int nonterminal, const std::unordered_map<int, Nonterm>& assignment) {
  const auto it = assignment.find(nonterminal);
  if (it == assignment.end()) {
    return kUnassignedNonterm;
  }
  return it->second;
}

// Checks whether all the nonterminals in the rhs of a rule have already been
// assigned Nonterm values.
bool IsRhsAssigned(const Rules::Rule& rule,
                   const std::unordered_map<int, Nonterm>& nonterminals) {
  for (const Rules::RhsElement& element : rule.rhs) {
    // Terminals are always considered assigned, check only for non-terminals.
    if (element.is_terminal) {
      continue;
    }
    if (GetAssignedIdForNonterminal(element.nonterminal, nonterminals) ==
        kUnassignedNonterm) {
      return false;
    }
  }

  // Check that all parts of an exclusion are defined.
  if (rule.callback == static_cast<CallbackId>(DefaultCallback::kExclusion)) {
    if (GetAssignedIdForNonterminal(rule.callback_param, nonterminals) ==
        kUnassignedNonterm) {
      return false;
    }
  }

  return true;
}

// Lowers a single high-level rule down into the intermediate representation.
void LowerRule(const int lhs_index, const Rules::Rule& rule,
               std::unordered_map<int, Nonterm>* nonterminals, Ir* ir) {
  const CallbackId callback = rule.callback;
  int64 callback_param = rule.callback_param;

  // Resolve id of excluded nonterminal in exclusion rules.
  if (callback == static_cast<CallbackId>(DefaultCallback::kExclusion)) {
    callback_param = GetAssignedIdForNonterminal(callback_param, *nonterminals);
    TC3_CHECK_NE(callback_param, kUnassignedNonterm);
  }

  // Special case for terminal rules.
  if (rule.rhs.size() == 1 && rule.rhs.front().is_terminal) {
    (*nonterminals)[lhs_index] =
        ir->Add(Ir::Lhs{GetAssignedIdForNonterminal(lhs_index, *nonterminals),
                        /*callback=*/{callback, callback_param},
                        /*preconditions=*/{rule.max_whitespace_gap}},
                rule.rhs.front().terminal, rule.case_sensitive, rule.shard);
    return;
  }

  // Nonterminal rules.
  std::vector<Nonterm> rhs_nonterms;
  for (const Rules::RhsElement& element : rule.rhs) {
    if (element.is_terminal) {
      rhs_nonterms.push_back(ir->Add(Ir::Lhs{kUnassignedNonterm},
                                     element.terminal, rule.case_sensitive,
                                     rule.shard));
    } else {
      Nonterm nonterminal_id =
          GetAssignedIdForNonterminal(element.nonterminal, *nonterminals);
      TC3_CHECK_NE(nonterminal_id, kUnassignedNonterm);
      rhs_nonterms.push_back(nonterminal_id);
    }
  }
  (*nonterminals)[lhs_index] =
      ir->Add(Ir::Lhs{GetAssignedIdForNonterminal(lhs_index, *nonterminals),
                      /*callback=*/{callback, callback_param},
                      /*preconditions=*/{rule.max_whitespace_gap}},
              rhs_nonterms, rule.shard);
}
// Check whether this component is a non-terminal.
bool IsNonterminal(StringPiece rhs_component) {
  return rhs_component[0] == '<' &&
         rhs_component[rhs_component.size() - 1] == '>';
}

// Sanity check for common typos -- '<' or '>' in a terminal.
void ValidateTerminal(StringPiece rhs_component) {
  TC3_CHECK_EQ(rhs_component.find('<'), std::string::npos)
      << "Rhs terminal `" << rhs_component << "` contains an angle bracket.";
  TC3_CHECK_EQ(rhs_component.find('>'), std::string::npos)
      << "Rhs terminal `" << rhs_component << "` contains an angle bracket.";
  TC3_CHECK_EQ(rhs_component.find('?'), std::string::npos)
      << "Rhs terminal `" << rhs_component << "` contains a question mark.";
}

}  // namespace

int Rules::AddNonterminal(const std::string& nonterminal_name) {
  std::string key = nonterminal_name;
  auto alias_it = nonterminal_alias_.find(key);
  if (alias_it != nonterminal_alias_.end()) {
    key = alias_it->second;
  }
  auto it = nonterminal_names_.find(key);
  if (it != nonterminal_names_.end()) {
    return it->second;
  }
  const int index = nonterminals_.size();
  nonterminals_.push_back(NontermInfo{key});
  nonterminal_names_.insert(it, {key, index});
  return index;
}

int Rules::AddNewNonterminal() {
  const int index = nonterminals_.size();
  nonterminals_.push_back(NontermInfo{});
  return index;
}

void Rules::AddAlias(const std::string& nonterminal_name,
                     const std::string& alias) {
  TC3_CHECK_EQ(nonterminal_alias_.insert_or_assign(alias, nonterminal_name)
                   .first->second,
               nonterminal_name)
      << "Cannot redefine alias: " << alias;
}

// Defines a nonterminal for an externally provided annotation.
int Rules::AddAnnotation(const std::string& annotation_name) {
  auto [it, inserted] =
      annotation_nonterminals_.insert({annotation_name, nonterminals_.size()});
  if (inserted) {
    nonterminals_.push_back(NontermInfo{});
  }
  return it->second;
}

void Rules::BindAnnotation(const std::string& nonterminal_name,
                           const std::string& annotation_name) {
  auto [_, inserted] = annotation_nonterminals_.insert(
      {annotation_name, AddNonterminal(nonterminal_name)});
  TC3_CHECK(inserted);
}

bool Rules::IsNonterminalOfName(const RhsElement& element,
                                const std::string& nonterminal) const {
  if (element.is_terminal) {
    return false;
  }
  return (nonterminals_[element.nonterminal].name == nonterminal);
}

// Note: For k optional components this creates 2^k rules, but it would be
// possible to be smarter about this and only use 2k rules instead.
// However that might be slower as it requires an extra rule firing at match
// time for every omitted optional element.
void Rules::ExpandOptionals(
    const int lhs, const std::vector<RhsElement>& rhs,
    const CallbackId callback, const int64 callback_param,
    const int8 max_whitespace_gap, const bool case_sensitive, const int shard,
    std::vector<int>::const_iterator optional_element_indices,
    std::vector<int>::const_iterator optional_element_indices_end,
    std::vector<bool>* omit_these) {
  if (optional_element_indices == optional_element_indices_end) {
    // Nothing is optional, so just generate a rule.
    Rule r;
    for (uint32 i = 0; i < rhs.size(); i++) {
      if (!omit_these->at(i)) {
        r.rhs.push_back(rhs[i]);
      }
    }
    r.callback = callback;
    r.callback_param = callback_param;
    r.max_whitespace_gap = max_whitespace_gap;
    r.case_sensitive = case_sensitive;
    r.shard = shard;
    nonterminals_[lhs].rules.push_back(rules_.size());
    rules_.push_back(r);
    return;
  }

  const int next_optional_part = *optional_element_indices;
  ++optional_element_indices;

  // Recursive call 1: The optional part is omitted.
  (*omit_these)[next_optional_part] = true;
  ExpandOptionals(lhs, rhs, callback, callback_param, max_whitespace_gap,
                  case_sensitive, shard, optional_element_indices,
                  optional_element_indices_end, omit_these);

  // Recursive call 2: The optional part is required.
  (*omit_these)[next_optional_part] = false;
  ExpandOptionals(lhs, rhs, callback, callback_param, max_whitespace_gap,
                  case_sensitive, shard, optional_element_indices,
                  optional_element_indices_end, omit_these);
}

std::vector<Rules::RhsElement> Rules::ResolveAnchors(
    const std::vector<RhsElement>& rhs) const {
  if (rhs.size() <= 2) {
    return rhs;
  }
  auto begin = rhs.begin();
  auto end = rhs.end();
  if (IsNonterminalOfName(rhs.front(), kStartNonterm) &&
      IsNonterminalOfName(rhs[1], kFiller)) {
    // Skip start anchor and filler.
    begin += 2;
  }
  if (IsNonterminalOfName(rhs.back(), kEndNonterm) &&
      IsNonterminalOfName(rhs[rhs.size() - 2], kFiller)) {
    // Skip filler and end anchor.
    end -= 2;
  }
  return std::vector<Rules::RhsElement>(begin, end);
}

std::vector<Rules::RhsElement> Rules::ResolveFillers(
    const std::vector<RhsElement>& rhs) {
  std::vector<RhsElement> result;
  for (int i = 0; i < rhs.size();) {
    if (i == rhs.size() - 1 || IsNonterminalOfName(rhs[i], kFiller) ||
        rhs[i].is_optional || !IsNonterminalOfName(rhs[i + 1], kFiller)) {
      result.push_back(rhs[i]);
      i++;
      continue;
    }

    // We have the case:
    // <a> <filler>
    // rewrite as:
    // <a_with_tokens> ::= <a>
    // <a_with_tokens> ::= <a_with_tokens> <token>
    const int with_tokens_nonterminal = AddNewNonterminal();
    const RhsElement token(AddNonterminal(kTokenNonterm),
                           /*is_optional=*/false);
    if (rhs[i + 1].is_optional) {
      // <a_with_tokens> ::= <a>
      Add(with_tokens_nonterminal, {rhs[i]});
    } else {
      // <a_with_tokens> ::= <a> <token>
      Add(with_tokens_nonterminal, {rhs[i], token});
    }
    // <a_with_tokens> ::= <a_with_tokens> <token>
    const RhsElement with_tokens(with_tokens_nonterminal,
                                 /*is_optional=*/false);
    Add(with_tokens_nonterminal, {with_tokens, token});
    result.push_back(with_tokens);
    i += 2;
  }
  return result;
}

std::vector<Rules::RhsElement> Rules::OptimizeRhs(
    const std::vector<RhsElement>& rhs) {
  return ResolveFillers(ResolveAnchors(rhs));
}

void Rules::Add(const int lhs, const std::vector<RhsElement>& rhs,
                const CallbackId callback, const int64 callback_param,
                const int8 max_whitespace_gap, const bool case_sensitive,
                const int shard) {
  // Resolve anchors and fillers.
  const std::vector optimized_rhs = OptimizeRhs(rhs);

  std::vector<int> optional_element_indices;
  TC3_CHECK_LT(optional_element_indices.size(), optimized_rhs.size())
      << "Rhs must contain at least one non-optional element.";
  for (int i = 0; i < optimized_rhs.size(); i++) {
    if (optimized_rhs[i].is_optional) {
      optional_element_indices.push_back(i);
    }
  }
  std::vector<bool> omit_these(optimized_rhs.size(), false);
  ExpandOptionals(lhs, optimized_rhs, callback, callback_param,
                  max_whitespace_gap, case_sensitive, shard,
                  optional_element_indices.begin(),
                  optional_element_indices.end(), &omit_these);
}

void Rules::Add(const std::string& lhs, const std::vector<std::string>& rhs,
                const CallbackId callback, const int64 callback_param,
                const int8 max_whitespace_gap, const bool case_sensitive,
                const int shard) {
  TC3_CHECK(!rhs.empty()) << "Rhs cannot be empty (Lhs=" << lhs << ")";
  TC3_CHECK(!IsPredefinedNonterminal(lhs));
  std::vector<RhsElement> rhs_elements;
  rhs_elements.reserve(rhs.size());
  for (StringPiece rhs_component : rhs) {
    // Check whether this component is optional.
    bool is_optional = false;
    if (rhs_component[rhs_component.size() - 1] == '?') {
      rhs_component.RemoveSuffix(1);
      is_optional = true;
    }
    // Check whether this component is a non-terminal.
    if (IsNonterminal(rhs_component)) {
      rhs_elements.push_back(
          RhsElement(AddNonterminal(rhs_component.ToString()), is_optional));
    } else {
      // A terminal.
      // Sanity check for common typos -- '<' or '>' in a terminal.
      ValidateTerminal(rhs_component);
      rhs_elements.push_back(RhsElement(rhs_component.ToString(), is_optional));
    }
  }
  Add(AddNonterminal(lhs), rhs_elements, callback, callback_param,
      max_whitespace_gap, case_sensitive, shard);
}

void Rules::AddWithExclusion(const std::string& lhs,
                             const std::vector<std::string>& rhs,
                             const std::string& excluded_nonterminal,
                             const int8 max_whitespace_gap,
                             const bool case_sensitive, const int shard) {
  Add(lhs, rhs,
      /*callback=*/static_cast<CallbackId>(DefaultCallback::kExclusion),
      /*callback_param=*/AddNonterminal(excluded_nonterminal),
      max_whitespace_gap, case_sensitive, shard);
}

void Rules::AddAssertion(const std::string& lhs,
                         const std::vector<std::string>& rhs,
                         const bool negative, const int8 max_whitespace_gap,
                         const bool case_sensitive, const int shard) {
  Add(lhs, rhs,
      /*callback=*/static_cast<CallbackId>(DefaultCallback::kAssertion),
      /*callback_param=*/negative, max_whitespace_gap, case_sensitive, shard);
}

void Rules::AddValueMapping(const std::string& lhs,
                            const std::vector<std::string>& rhs,
                            const int64 value, const int8 max_whitespace_gap,
                            const bool case_sensitive, const int shard) {
  Add(lhs, rhs,
      /*callback=*/static_cast<CallbackId>(DefaultCallback::kMapping),
      /*callback_param=*/value, max_whitespace_gap, case_sensitive, shard);
}

void Rules::AddRegex(const std::string& lhs, const std::string& regex_pattern) {
  AddRegex(AddNonterminal(lhs), regex_pattern);
}

void Rules::AddRegex(int lhs, const std::string& regex_pattern) {
  nonterminals_[lhs].regex_rules.push_back(regex_rules_.size());
  regex_rules_.push_back(regex_pattern);
}

Ir Rules::Finalize(const std::set<std::string>& predefined_nonterminals) const {
  Ir rules(filters_, num_shards_);
  std::unordered_map<int, Nonterm> nonterminal_ids;

  // Pending rules to process.
  std::set<std::pair<int, int>> scheduled_rules;

  // Define all used predefined nonterminals.
  for (const auto& it : nonterminal_names_) {
    if (IsPredefinedNonterminal(it.first) ||
        predefined_nonterminals.find(it.first) !=
            predefined_nonterminals.end()) {
      nonterminal_ids[it.second] = rules.AddUnshareableNonterminal(it.first);
    }
  }

  // Assign (unmergeable) Nonterm values to any nonterminals that have
  // multiple rules or that have a filter callback on some rule.
  for (int i = 0; i < nonterminals_.size(); i++) {
    const NontermInfo& nonterminal = nonterminals_[i];
    bool unmergeable =
        (nonterminal.from_annotation || nonterminal.rules.size() > 1 ||
         !nonterminal.regex_rules.empty());
    for (const int rule_index : nonterminal.rules) {
      const Rule& rule = rules_[rule_index];

      // Schedule rule.
      scheduled_rules.insert({i, rule_index});

      if (rule.callback != kNoCallback &&
          filters_.find(rule.callback) != filters_.end()) {
        unmergeable = true;
      }
    }

    if (unmergeable) {
      // Define unique nonterminal id.
      nonterminal_ids[i] = rules.AddUnshareableNonterminal(nonterminal.name);
    } else {
      nonterminal_ids[i] = rules.AddNonterminal(nonterminal.name);
    }

    // Define regex rules.
    for (const int regex_rule : nonterminal.regex_rules) {
      rules.AddRegex(nonterminal_ids[i], regex_rules_[regex_rule]);
    }
  }

  // Define annotations.
  for (const auto& [annotation, nonterminal] : annotation_nonterminals_) {
    rules.AddAnnotation(nonterminal_ids[nonterminal], annotation);
  }

  // Now, keep adding eligible rules (rules whose rhs is completely assigned)
  // until we can't make any more progress.
  // Note: The following code is quadratic in the worst case.
  // This seems fine as this will only run as part of the compilation of the
  // grammar rules during model assembly.
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto nt_and_rule = scheduled_rules.begin();
         nt_and_rule != scheduled_rules.end();) {
      const Rule& rule = rules_[nt_and_rule->second];
      if (IsRhsAssigned(rule, nonterminal_ids)) {
        // Compile the rule.
        LowerRule(/*lhs_index=*/nt_and_rule->first, rule, &nonterminal_ids,
                  &rules);
        scheduled_rules.erase(
            nt_and_rule++);  // Iterator is advanced before erase.
        changed = true;
        break;
      } else {
        nt_and_rule++;
      }
    }
  }
  TC3_CHECK(scheduled_rules.empty());
  return rules;
}

}  // namespace libtextclassifier3::grammar
