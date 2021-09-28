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

// A token matcher based on context-free grammars.
//
// A lexer passes token to the matcher: literal terminal strings and token
// types. It passes tokens to the matcher by calling AddTerminal() and
// AddMatch() for literal terminals and token types, respectively.
// The lexer passes each token along with the [begin, end) position range
// in which it occurs.  So for an input string "Groundhog February 2, 2007", the
// lexer would tell the matcher that:
//
//     "Groundhog" occurs at [0, 9)
//     <space> occurs at [9, 10)
//     "February" occurs at [10, 18)
//     <space> occurs at [18, 19)
//     <string_of_digits> occurs at [19, 20)
//     "," occurs at [20, 21)
//     <space> occurs at [21, 22)
//     <string_of_digits> occurs at [22, 26)
//
// The lexer passes tokens to the matcher by calling AddTerminal() and
// AddMatch() for literal terminals and token types, respectively.
//
// Although it is unnecessary for this example grammar, a lexer can
// output multiple tokens for the same input range.  So our lexer could
// additionally output:
//     "2" occurs at [19, 20)        // a second token for [19, 20)
//     "2007" occurs at [22, 26)
//     <syllable> occurs at [0, 6)   // overlaps with (Groundhog [0, 9))
//     <syllable> occurs at [6, 9)
// The only constraint on the lexer's output is that it has to pass tokens
// to the matcher in left-to-right order, strictly speaking, their "end"
// positions must be nondecreasing.  (This constraint allows a more
// efficient matching algorithm.)  The "begin" positions can be in any
// order.
//
// There are two kinds of supported callbacks:
// (1) OUTPUT:  Callbacks are the only output mechanism a matcher has.  For each
// "top-level" rule in your grammar, like the rule for <date> above -- something
// you're trying to find instances of -- you use a callback which the matcher
// will invoke every time it finds an instance of <date>.
// (2) FILTERS:
// Callbacks allow you to put extra conditions on when a grammar rule
// applies.  In the example grammar, the rule
//
//       <day> ::= <string_of_digits>     // must be between 1 and 31
//
// should only apply for *some* <string_of_digits> tokens, not others.
// By using a filter callback on this rule, you can tell the matcher that
// an instance of the rule's RHS is only *sometimes* considered an
// instance of its LHS.  The filter callback will get invoked whenever
// the matcher finds an instance of <string_of_digits>.  The callback can
// look at the digits and decide whether they represent a number between
// 1 and 31.  If so, the callback calls Matcher::AddMatch() to tell the
// matcher there's a <day> there.  If not, the callback simply exits
// without calling AddMatch().
//
// Technically, a FILTER callback can make any number of calls to
// AddMatch() or even AddTerminal().  But the expected usage is to just
// make zero or one call to AddMatch().  OUTPUT callbacks are not expected
// to call either of these -- output callbacks are invoked merely as a
// side-effect, not in order to decide whether a rule applies or not.
//
// In the above example, you would probably use three callbacks.  Filter
// callbacks on the rules for <day> and <year> would check the numeric
// value of the <string_of_digits>.  An output callback on the rule for
// <date> would simply increment the counter of dates found on the page.
//
// Note that callbacks are attached to rules, not to nonterminals.  You
// could have two alternative rules for <date> and use a different
// callback for each one.

#ifndef LIBTEXTCLASSIFIER_UTILS_GRAMMAR_MATCHER_H_
#define LIBTEXTCLASSIFIER_UTILS_GRAMMAR_MATCHER_H_

#include <array>
#include <functional>
#include <vector>

#include "annotator/types.h"
#include "utils/base/arena.h"
#include "utils/grammar/callback-delegate.h"
#include "utils/grammar/match.h"
#include "utils/grammar/rules_generated.h"
#include "utils/strings/stringpiece.h"
#include "utils/utf8/unilib.h"

namespace libtextclassifier3::grammar {

class Matcher {
 public:
  explicit Matcher(const UniLib* unilib, const RulesSet* rules,
                   const std::vector<const RulesSet_::Rules*> rules_shards,
                   CallbackDelegate* delegate)
      : state_(STATE_DEFAULT),
        unilib_(*unilib),
        arena_(kBlocksize),
        rules_(rules),
        rules_shards_(rules_shards),
        delegate_(delegate) {
    TC3_CHECK(rules_ != nullptr);
    Reset();
  }
  explicit Matcher(const UniLib* unilib, const RulesSet* rules,
                   CallbackDelegate* delegate)
      : Matcher(unilib, rules, {}, delegate) {
    rules_shards_.reserve(rules->rules()->size());
    rules_shards_.insert(rules_shards_.end(), rules->rules()->begin(),
                         rules->rules()->end());
  }

  // Resets the matcher.
  void Reset();

  // Finish the matching.
  void Finish();

  // Tells the matcher that the given terminal was found occupying position
  // range [begin, end) in the input.
  // The matcher may invoke callback functions before returning, if this
  // terminal triggers any new matches for rules in the grammar.
  // Calls to AddTerminal() and AddMatch() must be in left-to-right order,
  // that is, the sequence of `end` values must be non-decreasing.
  void AddTerminal(const CodepointSpan codepoint_span, const int match_offset,
                   StringPiece terminal);
  void AddTerminal(const CodepointIndex begin, const CodepointIndex end,
                   StringPiece terminal) {
    AddTerminal(CodepointSpan{begin, end}, begin, terminal);
  }

  // Adds a nonterminal match to the chart.
  // This can be invoked by the lexer if the lexer needs to add nonterminals to
  // the chart.
  void AddMatch(Match* match);

  // Allocates memory from an area for a new match.
  // The `size` parameter is there to allow subclassing of the match object
  // with additional fields.
  Match* AllocateMatch(const size_t size) {
    return reinterpret_cast<Match*>(arena_.Alloc(size));
  }

  template <typename T>
  T* AllocateMatch() {
    return reinterpret_cast<T*>(arena_.Alloc(sizeof(T)));
  }

  template <typename T, typename... Args>
  T* AllocateAndInitMatch(Args... args) {
    T* match = AllocateMatch<T>();
    match->Init(args...);
    return match;
  }

  // Returns the current number of bytes allocated for all match objects.
  size_t ArenaSize() const { return arena_.status().bytes_allocated(); }

 private:
  static constexpr int kBlocksize = 16 << 10;

  // The state of the matcher.
  enum State {
    // The matcher is in the default state.
    STATE_DEFAULT = 0,

    // The matcher is currently processing queued match items.
    STATE_PROCESSING = 1,
  };
  State state_;

  // Process matches from lhs set.
  void ExecuteLhsSet(const CodepointSpan codepoint_span, const int match_offset,
                     const int whitespace_gap,
                     const std::function<void(Match*)>& initializer,
                     const RulesSet_::LhsSet* lhs_set,
                     CallbackDelegate* delegate);

  // Queues a newly created match item.
  void QueueForProcessing(Match* item);

  // Queues a match item for later post checking of the exclusion condition.
  // For exclusions we need to check that the `item->excluded_nonterminal`
  // doesn't match the same span. As we cannot know which matches have already
  // been added, we queue the item for later post checking - once all matches
  // up to `item->codepoint_span.second` have been added.
  void QueueForPostCheck(ExclusionMatch* item);

  // Adds pending items to the chart, possibly generating new matches as a
  // result.
  void ProcessPendingSet();

  // Returns whether the chart contains a match for a given nonterminal.
  bool ContainsMatch(const Nonterm nonterm, const CodepointSpan& span) const;

  // Checks all pending exclusion matches that their exclusion condition is
  // fulfilled.
  void ProcessPendingExclusionMatches();

  UniLib unilib_;

  // Memory arena for match allocation.
  UnsafeArena arena_;

  // The end position of the most recent match or terminal, for sanity
  // checking.
  int last_end_;

  // Rules.
  const RulesSet* rules_;

  // The set of items pending to be added to the chart as a singly-linked list.
  Match* pending_items_;

  // The set of items pending to be post-checked as a singly-linked list.
  ExclusionMatch* pending_exclusion_items_;

  // The chart data structure: a hashtable containing all matches, indexed by
  // their end positions.
  static constexpr int kChartHashTableNumBuckets = 1 << 8;
  static constexpr int kChartHashTableBitmask = kChartHashTableNumBuckets - 1;
  std::array<Match*, kChartHashTableNumBuckets> chart_;

  // The active rule shards.
  std::vector<const RulesSet_::Rules*> rules_shards_;

  // The callback handler.
  CallbackDelegate* delegate_;
};

}  // namespace libtextclassifier3::grammar

#endif  // LIBTEXTCLASSIFIER_UTILS_GRAMMAR_MATCHER_H_
