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

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_UTILS_IR_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_UTILS_IR_H_

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "utils/base/integral_types.h"
#include "utils/grammar/rules_generated.h"
#include "utils/grammar/types.h"

namespace libtextclassifier3::grammar {

// Pre-defined nonterminal classes that the lexer can handle.
constexpr const char* kStartNonterm = "<^>";
constexpr const char* kEndNonterm = "<$>";
constexpr const char* kWordBreakNonterm = "<\b>";
constexpr const char* kTokenNonterm = "<token>";
constexpr const char* kUppercaseTokenNonterm = "<uppercase_token>";
constexpr const char* kDigitsNonterm = "<digits>";
constexpr const char* kNDigitsNonterm = "<%d_digits>";
constexpr const int kMaxNDigitsNontermLength = 20;

// Low-level intermediate rules representation.
// In this representation, nonterminals are specified simply as integers
// (Nonterms), rather than strings which is more efficient.
// Rule set optimizations are done on this representation.
//
// Rules are represented in (mostly) Chomsky Normal Form, where all rules are
// of the following form, either:
//   * <nonterm> ::= term
//   * <nonterm> ::= <nonterm>
//   * <nonterm> ::= <nonterm> <nonterm>
class Ir {
 public:
  // A rule callback as a callback id and parameter pair.
  struct Callback {
    bool operator==(const Callback& other) const {
      return std::tie(id, param) == std::tie(other.id, other.param);
    }

    CallbackId id = kNoCallback;
    int64 param = 0;
  };

  // Constraints for triggering a rule.
  struct Preconditions {
    bool operator==(const Preconditions& other) const {
      return max_whitespace_gap == other.max_whitespace_gap;
    }

    // The maximum allowed whitespace between parts of the rule.
    // The default of -1 allows for unbounded whitespace.
    int8 max_whitespace_gap = -1;
  };

  struct Lhs {
    bool operator==(const Lhs& other) const {
      return std::tie(nonterminal, callback, preconditions) ==
             std::tie(other.nonterminal, other.callback, other.preconditions);
    }

    Nonterm nonterminal = kUnassignedNonterm;
    Callback callback;
    Preconditions preconditions;
  };
  using LhsSet = std::vector<Lhs>;

  // A rules shard.
  struct RulesShard {
    // Terminal rules.
    std::unordered_map<std::string, LhsSet> terminal_rules;
    std::unordered_map<std::string, LhsSet> lowercase_terminal_rules;

    // Unary rules.
    std::unordered_map<Nonterm, LhsSet> unary_rules;

    // Binary rules.
    std::unordered_map<TwoNonterms, LhsSet, BinaryRuleHasher> binary_rules;
  };

  explicit Ir(const std::unordered_set<CallbackId>& filters = {},
              const int num_shards = 1)
      : num_nonterminals_(0), filters_(filters), shards_(num_shards) {}

  // Adds a new non-terminal.
  Nonterm AddNonterminal(const std::string& name = "") {
    const Nonterm nonterminal = ++num_nonterminals_;
    if (!name.empty()) {
      // Record debug information.
      nonterminal_names_[nonterminal] = name;
      nonterminal_ids_[name] = nonterminal;
    }
    return nonterminal;
  }

  // Defines a nonterminal if not yet defined.
  Nonterm DefineNonterminal(Nonterm nonterminal) {
    return (nonterminal != kUnassignedNonterm) ? nonterminal : AddNonterminal();
  }

  // Defines a new non-terminal that cannot be shared internally.
  Nonterm AddUnshareableNonterminal(const std::string& name = "") {
    const Nonterm nonterminal = AddNonterminal(name);
    nonshareable_.insert(nonterminal);
    return nonterminal;
  }

  // Gets the non-terminal for a given name, if it was previously defined.
  Nonterm GetNonterminalForName(const std::string& name) const {
    const auto it = nonterminal_ids_.find(name);
    if (it == nonterminal_ids_.end()) {
      return kUnassignedNonterm;
    }
    return it->second;
  }

  // Adds a terminal rule <lhs> ::= terminal.
  Nonterm Add(const Lhs& lhs, const std::string& terminal,
              bool case_sensitive = false, int shard = 0);
  Nonterm Add(const Nonterm lhs, const std::string& terminal,
              bool case_sensitive = false, int shard = 0) {
    return Add(Lhs{lhs}, terminal, case_sensitive, shard);
  }

  // Adds a unary rule <lhs> ::= <rhs>.
  Nonterm Add(const Lhs& lhs, Nonterm rhs, int shard = 0) {
    return AddRule(lhs, rhs, &shards_[shard].unary_rules);
  }
  Nonterm Add(Nonterm lhs, Nonterm rhs, int shard = 0) {
    return Add(Lhs{lhs}, rhs, shard);
  }

  // Adds a binary rule <lhs> ::= <rhs_1> <rhs_2>.
  Nonterm Add(const Lhs& lhs, Nonterm rhs_1, Nonterm rhs_2, int shard = 0) {
    return AddRule(lhs, {rhs_1, rhs_2}, &shards_[shard].binary_rules);
  }
  Nonterm Add(Nonterm lhs, Nonterm rhs_1, Nonterm rhs_2, int shard = 0) {
    return Add(Lhs{lhs}, rhs_1, rhs_2, shard);
  }

  // Adds a rule <lhs> ::= <rhs_1> <rhs_2> ... <rhs_k>
  //
  // If k > 2, we internally create a series of Nonterms representing prefixes
  // of the full rhs.
  //     <temp_1> ::= <RHS_1> <RHS_2>
  //     <temp_2> ::= <temp_1> <RHS_3>
  //     ...
  //     <LHS> ::= <temp_(k-1)> <RHS_k>
  Nonterm Add(const Lhs& lhs, const std::vector<Nonterm>& rhs, int shard = 0);
  Nonterm Add(Nonterm lhs, const std::vector<Nonterm>& rhs, int shard = 0) {
    return Add(Lhs{lhs}, rhs, shard);
  }

  // Adds a regex rule <lhs> ::= <regex_pattern>.
  Nonterm AddRegex(Nonterm lhs, const std::string& regex_pattern);

  // Adds a definition for a nonterminal provided by a text annotation.
  void AddAnnotation(Nonterm lhs, const std::string& annotation);

  // Serializes a rule set in the intermediate representation into the
  // memory mappable inference format.
  void Serialize(bool include_debug_information, RulesSetT* output) const;

  std::string SerializeAsFlatbuffer(
      bool include_debug_information = false) const;

  const std::vector<RulesShard>& shards() const { return shards_; }

 private:
  template <typename R, typename H>
  Nonterm AddRule(const Lhs& lhs, const R& rhs,
                  std::unordered_map<R, LhsSet, H>* rules) {
    const auto it = rules->find(rhs);

    // Rhs was not yet used.
    if (it == rules->end()) {
      const Nonterm nonterminal = DefineNonterminal(lhs.nonterminal);
      rules->insert(it,
                    {rhs, {Lhs{nonterminal, lhs.callback, lhs.preconditions}}});
      return nonterminal;
    }

    return AddToSet(lhs, &it->second);
  }

  // Adds a new callback to an lhs set, potentially sharing nonterminal ids and
  // existing callbacks.
  Nonterm AddToSet(const Lhs& lhs, LhsSet* lhs_set);

  // Serializes the sharded terminal rules.
  void SerializeTerminalRules(
      RulesSetT* rules_set,
      std::vector<std::unique_ptr<RulesSet_::RulesT>>* rules_shards) const;

  // The defined non-terminals.
  Nonterm num_nonterminals_;
  std::unordered_set<Nonterm> nonshareable_;

  // The set of callbacks that should be treated as filters.
  std::unordered_set<CallbackId> filters_;

  // The sharded rules.
  std::vector<RulesShard> shards_;

  // The regex rules.
  std::vector<std::pair<std::string, Nonterm>> regex_rules_;

  // Mapping from annotation name to nonterminal.
  std::vector<std::pair<std::string, Nonterm>> annotations_;

  // Debug information.
  std::unordered_map<Nonterm, std::string> nonterminal_names_;
  std::unordered_map<std::string, Nonterm> nonterminal_ids_;
};

}  // namespace libtextclassifier3::grammar

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_UTILS_IR_H_
