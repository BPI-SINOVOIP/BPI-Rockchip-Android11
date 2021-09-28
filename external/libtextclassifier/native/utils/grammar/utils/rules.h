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

// Utility functions for pre-processing, creating and testing context free
// grammars.

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_UTILS_RULES_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_UTILS_RULES_H_

#include <unordered_map>
#include <vector>

#include "utils/grammar/types.h"
#include "utils/grammar/utils/ir.h"

namespace libtextclassifier3::grammar {

// Special nonterminals.
constexpr const char* kFiller = "<filler>";

// All rules for a grammar will be collected in a rules object.
//
//    Rules r;
//    CallbackId date_output_callback = 1;
//    CallbackId day_filter_callback = 2;  r.DefineFilter(day_filter_callback);
//    CallbackId year_filter_callback = 3; r.DefineFilter(year_filter_callback);
//    r.Add("<date>", {"<monthname>", "<day>", <year>"},
//          date_output_callback);
//    r.Add("<monthname>", {"January"});
//    ...
//    r.Add("<monthname>", {"December"});
//    r.Add("<day>", {"<string_of_digits>"}, day_filter_callback);
//    r.Add("<year>", {"<string_of_digits>"}, year_filter_callback);
//
// The Add() method adds a rule with a given lhs, rhs, and (optionally)
// callback.  The rhs is just a list of terminals and nonterminals.  Anything
// surrounded in angle brackets is considered a nonterminal.  A "?" can follow
// any element of the RHS, like this:
//
//    r.Add("<date>", {"<monthname>", "<day>?", ",?", "<year>"});
//
// This indicates that the <day> and "," parts of the rhs are optional.
// (This is just notational shorthand for adding a bunch of rules.)
//
// Once you're done adding rules and callbacks to the Rules object,
// call r.Finalize() on it. This lowers the rule set into an internal
// representation.
class Rules {
 public:
  explicit Rules(const int num_shards = 1) : num_shards_(num_shards) {}

  // Represents one item in a right-hand side, a single terminal or nonterminal.
  struct RhsElement {
    RhsElement() {}
    explicit RhsElement(const std::string& terminal, const bool is_optional)
        : is_terminal(true), terminal(terminal), is_optional(is_optional) {}
    explicit RhsElement(const int nonterminal, const bool is_optional)
        : is_terminal(false),
          nonterminal(nonterminal),
          is_optional(is_optional) {}
    bool is_terminal;
    std::string terminal;
    int nonterminal;
    bool is_optional;
  };

  // Represents the right-hand side, and possibly callback, of one rule.
  struct Rule {
    std::vector<RhsElement> rhs;
    CallbackId callback = kNoCallback;
    int64 callback_param = 0;
    int8 max_whitespace_gap = -1;
    bool case_sensitive = false;
    int shard = 0;
  };

  struct NontermInfo {
    // The name of the non-terminal, if defined.
    std::string name;

    // Whether the nonterminal is provided via an annotation.
    bool from_annotation = false;

    // Rules that have this non-terminal as the lhs.
    std::vector<int> rules;

    // Regex rules that have this non-terminal as the lhs.
    std::vector<int> regex_rules;
  };

  // Adds a rule `lhs ::= rhs` with the given callback id and parameter.
  // Note: Nonterminal names are in angle brackets and cannot contain
  // whitespace. The `rhs` is a list of components, each of which is either:
  //  * A nonterminal name (in angle brackets)
  //  * A terminal
  // optionally followed by a `?` which indicates that the component is
  // optional. The `rhs` must contain at least one non-optional component.
  void Add(const std::string& lhs, const std::vector<std::string>& rhs,
           const CallbackId callback = kNoCallback,
           const int64 callback_param = 0, int8 max_whitespace_gap = -1,
           bool case_sensitive = false, int shard = 0);

  // Adds a rule `lhs ::= rhs` with the given callback id and parameter.
  // The `rhs` must contain at least one non-optional component.
  void Add(int lhs, const std::vector<RhsElement>& rhs,
           CallbackId callback = kNoCallback, int64 callback_param = 0,
           int8 max_whitespace_gap = -1, bool case_sensitive = false,
           int shard = 0);

  // Adds a rule `lhs ::= rhs` with exclusion.
  // The rule only matches, if `excluded_nonterminal` doesn't match the same
  // span.
  void AddWithExclusion(const std::string& lhs,
                        const std::vector<std::string>& rhs,
                        const std::string& excluded_nonterminal,
                        int8 max_whitespace_gap = -1,
                        bool case_sensitive = false, int shard = 0);

  // Adds an assertion callback.
  void AddAssertion(const std::string& lhs, const std::vector<std::string>& rhs,
                    bool negative = true, int8 max_whitespace_gap = -1,
                    bool case_sensitive = false, int shard = 0);

  // Adds a mapping callback.
  void AddValueMapping(const std::string& lhs,
                       const std::vector<std::string>& rhs, int64 value,
                       int8 max_whitespace_gap = -1,
                       bool case_sensitive = false, int shard = 0);

  // Adds a regex rule.
  void AddRegex(const std::string& lhs, const std::string& regex_pattern);
  void AddRegex(int lhs, const std::string& regex_pattern);

  // Creates a nonterminal with the given name, if one doesn't already exist.
  int AddNonterminal(const std::string& nonterminal_name);

  // Creates a new nonterminal.
  int AddNewNonterminal();

  // Defines a nonterminal for an externally provided annotation.
  int AddAnnotation(const std::string& annotation_name);

  // Defines a nonterminal for an externally provided annotation.
  void BindAnnotation(const std::string& nonterminal_name,
                      const std::string& annotation_name);

  // Adds an alias for a nonterminal. This is a separate name for the same
  // nonterminal.
  void AddAlias(const std::string& nonterminal_name, const std::string& alias);

  // Defines a new filter id.
  void DefineFilter(const CallbackId filter_id) { filters_.insert(filter_id); }

  // Lowers the rule set into the intermediate representation.
  // Treats nonterminals given by the argument `predefined_nonterminals` as
  // defined externally. This allows to define rules that are dependent on
  // non-terminals produced by e.g. existing text annotations and that will be
  // fed to the matcher by the lexer.
  Ir Finalize(const std::set<std::string>& predefined_nonterminals = {}) const;

 private:
  void ExpandOptionals(
      int lhs, const std::vector<RhsElement>& rhs, CallbackId callback,
      int64 callback_param, int8 max_whitespace_gap, bool case_sensitive,
      int shard, std::vector<int>::const_iterator optional_element_indices,
      std::vector<int>::const_iterator optional_element_indices_end,
      std::vector<bool>* omit_these);

  // Applies optimizations to the right hand side of a rule.
  std::vector<RhsElement> OptimizeRhs(const std::vector<RhsElement>& rhs);

  // Removes start and end anchors in case they are followed (respectively
  // preceded) by unbounded filler.
  std::vector<RhsElement> ResolveAnchors(
      const std::vector<RhsElement>& rhs) const;

  // Rewrites fillers in a rule.
  // Fillers in a rule such as `lhs ::= <a> <filler> <b>` could be lowered as
  // <tokens> ::= <token>
  // <tokens> ::= <tokens> <token>
  // This has the disadvantage that it will produce a match for each possible
  // span in the text, which is quadratic in the number of tokens.
  // It can be more efficiently written as:
  // `lhs ::= <a_with_tokens> <b>` with
  // `<a_with_tokens> ::= <a>`
  // `<a_with_tokens> ::= <a_with_tokens> <token>`
  // In this each occurrence of `<a>` can start a sequence of tokens.
  std::vector<RhsElement> ResolveFillers(const std::vector<RhsElement>& rhs);

  // Checks whether an element denotes a specific nonterminal.
  bool IsNonterminalOfName(const RhsElement& element,
                           const std::string& nonterminal) const;

  const int num_shards_;

  // Non-terminal to id map.
  std::unordered_map<std::string, int> nonterminal_names_;
  std::vector<NontermInfo> nonterminals_;
  std::unordered_map<std::string, std::string> nonterminal_alias_;
  std::unordered_map<std::string, int> annotation_nonterminals_;

  // Rules.
  std::vector<Rule> rules_;
  std::vector<std::string> regex_rules_;

  // Ids of callbacks that should be treated as filters.
  std::unordered_set<CallbackId> filters_;
};

}  // namespace libtextclassifier3::grammar

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_UTILS_RULES_H_
