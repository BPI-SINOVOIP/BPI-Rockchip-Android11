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

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_MATCH_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_MATCH_H_

#include <functional>
#include <vector>

#include "annotator/types.h"
#include "utils/grammar/types.h"
#include "utils/strings/stringpiece.h"

namespace libtextclassifier3::grammar {

// Represents a single match that was found for a particular nonterminal.
// Instances should be created by calling Matcher::AllocateMatch().
// This uses an arena to allocate matches (and subclasses thereof).
struct Match {
  static constexpr int16 kUnknownType = 0;
  static constexpr int16 kTokenType = -1;
  static constexpr int16 kDigitsType = -2;
  static constexpr int16 kBreakType = -3;
  static constexpr int16 kAssertionMatch = -4;
  static constexpr int16 kMappingMatch = -5;
  static constexpr int16 kExclusionMatch = -6;
  static constexpr int16 kAnnotationMatch = -7;

  void Init(const Nonterm arg_lhs, const CodepointSpan arg_codepoint_span,
            const int arg_match_offset, const int arg_type = kUnknownType) {
    lhs = arg_lhs;
    codepoint_span = arg_codepoint_span;
    match_offset = arg_match_offset;
    type = arg_type;
    rhs1 = nullptr;
    rhs2 = nullptr;
  }

  void Init(const Match& other) { *this = other; }

  // For binary rule matches:  rhs1 != NULL and rhs2 != NULL
  //      unary rule matches:  rhs1 == NULL and rhs2 != NULL
  //   terminal rule matches:  rhs1 != NULL and rhs2 == NULL
  //           custom leaves:  rhs1 == NULL and rhs2 == NULL
  bool IsInteriorNode() const { return rhs2 != nullptr; }
  bool IsLeaf() const { return !rhs2; }

  bool IsBinaryRule() const { return rhs1 && rhs2; }
  bool IsUnaryRule() const { return !rhs1 && rhs2; }
  bool IsTerminalRule() const { return rhs1 && !rhs2; }
  bool HasLeadingWhitespace() const {
    return codepoint_span.first != match_offset;
  }

  const Match* unary_rule_rhs() const { return rhs2; }

  // Used in singly-linked queue of matches for processing.
  Match* next = nullptr;

  // Nonterminal we found a match for.
  Nonterm lhs = kUnassignedNonterm;

  // Type of the match.
  int16 type = kUnknownType;

  // The span in codepoints.
  CodepointSpan codepoint_span;

  // The begin codepoint offset used during matching.
  // This is usually including any prefix whitespace.
  int match_offset;

  union {
    // The first sub match for binary rules.
    const Match* rhs1 = nullptr;

    // The terminal, for terminal rules.
    const char* terminal;
  };
  // First or second sub-match for interior nodes.
  const Match* rhs2 = nullptr;
};

// Match type to keep track of associated values.
struct MappingMatch : public Match {
  // The associated id or value.
  int64 id;
};

// Match type to keep track of assertions.
struct AssertionMatch : public Match {
  // If true, the assertion is negative and will be valid if the input doesn't
  // match.
  bool negative;
};

// Match type to define exclusions.
struct ExclusionMatch : public Match {
  // The nonterminal that denotes matches to exclude from a successful match.
  // So the match is only valid if there is no match of `exclusion_nonterm`
  // spanning the same text range.
  Nonterm exclusion_nonterm;
};

// Match to represent an annotator annotated span in the grammar.
struct AnnotationMatch : public Match {
  const ClassificationResult* annotation;
};

// Utility functions for parse tree traversal.

// Does a preorder traversal, calling `node_fn` on each node.
// `node_fn` is expected to return whether to continue expanding a node.
void Traverse(const Match* root,
              const std::function<bool(const Match*)>& node_fn);

// Does a preorder traversal, calling `pred_fn` and returns the first node
// on which `pred_fn` returns true.
const Match* SelectFirst(const Match* root,
                         const std::function<bool(const Match*)>& pred_fn);

// Does a preorder traversal, selecting all nodes where `pred_fn` returns true.
std::vector<const Match*> SelectAll(
    const Match* root, const std::function<bool(const Match*)>& pred_fn);

// Selects all terminals from a parse tree.
inline std::vector<const Match*> SelectTerminals(const Match* root) {
  return SelectAll(root, &Match::IsTerminalRule);
}

// Selects all leaves from a parse tree.
inline std::vector<const Match*> SelectLeaves(const Match* root) {
  return SelectAll(root, &Match::IsLeaf);
}

// Retrieves the first child node of a given type.
template <typename T>
const T* SelectFirstOfType(const Match* root, const int16 type) {
  return static_cast<const T*>(SelectFirst(
      root, [type](const Match* node) { return node->type == type; }));
}

// Retrieves all nodes of a given type.
template <typename T>
const std::vector<const T*> SelectAllOfType(const Match* root,
                                            const int16 type) {
  std::vector<const T*> result;
  Traverse(root, [&result, type](const Match* node) {
    if (node->type == type) {
      result.push_back(static_cast<const T*>(node));
    }
    return true;
  });
  return result;
}

}  // namespace libtextclassifier3::grammar

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_MATCH_H_
