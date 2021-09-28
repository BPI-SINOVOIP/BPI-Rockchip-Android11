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

#include "utils/grammar/matcher.h"

#include <iostream>
#include <limits>

#include "utils/base/endian.h"
#include "utils/base/logging.h"
#include "utils/base/macros.h"
#include "utils/grammar/types.h"
#include "utils/strings/utf8.h"

namespace libtextclassifier3::grammar {
namespace {

// Iterator that just enumerates the bytes in a utf8 text.
struct ByteIterator {
  explicit ByteIterator(StringPiece text)
      : data(text.data()), end(text.data() + text.size()) {}

  inline char Next() {
    TC3_DCHECK(HasNext());
    const char c = data[0];
    data++;
    return c;
  }
  inline bool HasNext() const { return data < end; }

  const char* data;
  const char* end;
};

// Iterator that lowercases a utf8 string on the fly and enumerates the bytes.
struct LowercasingByteIterator {
  LowercasingByteIterator(const UniLib* unilib, StringPiece text)
      : unilib(*unilib),
        data(text.data()),
        end(text.data() + text.size()),
        buffer_pos(0),
        buffer_size(0) {}

  inline char Next() {
    // Queue next character.
    if (buffer_pos >= buffer_size) {
      buffer_pos = 0;
      // Lower-case the next character.
      buffer_size =
          ValidRuneToChar(unilib.ToLower(ValidCharToRune(data)), buffer);
      data += buffer_size;
    }
    TC3_DCHECK_LT(buffer_pos, buffer_size);
    return buffer[buffer_pos++];
  }

  inline bool HasNext() const {
    // Either we are not at the end of the data or didn't consume all bytes of
    // the current character.
    return (data < end || buffer_pos < buffer_size);
  }

  const UniLib& unilib;
  const char* data;
  const char* end;

  // Each unicode codepoint can have up to 4 utf8 encoding bytes.
  char buffer[4];
  int buffer_pos;
  int buffer_size;
};

// Searches a terminal match within a sorted table of terminals.
// Using `LowercasingByteIterator` allows to lower-case the query string on the
// fly.
template <typename T>
const char* FindTerminal(T input_iterator, const char* strings,
                         const uint32* offsets, const int num_terminals,
                         int* terminal_index) {
  int left = 0;
  int right = num_terminals;
  int span_size = right - left;
  int match_length = 0;

  // Loop invariant:
  // At the ith iteration, all strings in the range `left` ... `right` match the
  // input on the first `match_length` characters.
  while (true) {
    const unsigned char c =
        static_cast<const unsigned char>(input_iterator.Next());

    // We find the possible range of strings in `left` ... `right` matching the
    // `match_length` + 1 character with two binary searches:
    //    1) `lower_bound` to find the start of the range of matching strings.
    //    2) `upper_bound` to find the non-inclusive end of the range.
    left =
        (std::lower_bound(
             offsets + left, offsets + right, c,
             [strings, match_length](uint32 string_offset, uint32 c) -> bool {
               return static_cast<unsigned char>(
                          strings[string_offset + match_length]) <
                      LittleEndian::ToHost32(c);
             }) -
         offsets);
    right =
        (std::upper_bound(
             offsets + left, offsets + right, c,
             [strings, match_length](uint32 c, uint32 string_offset) -> bool {
               return LittleEndian::ToHost32(c) <
                      static_cast<unsigned char>(
                          strings[string_offset + match_length]);
             }) -
         offsets);
    span_size = right - left;
    if (span_size <= 0) {
      return nullptr;
    }
    ++match_length;

    // By the loop variant and due to the fact that the strings are sorted,
    // a matching string will be at `left` now.
    if (!input_iterator.HasNext()) {
      const int string_offset = LittleEndian::ToHost32(offsets[left]);
      if (strings[string_offset + match_length] == 0) {
        *terminal_index = left;
        return &strings[string_offset];
      }
      return nullptr;
    }
  }

  // No match found.
  return nullptr;
}

// Finds terminal matches in the terminal rules hash tables.
// In case a match is found, `terminal` will be set to point into the
// terminals string pool.
template <typename T>
const RulesSet_::LhsSet* FindTerminalMatches(
    T input_iterator, const RulesSet* rules_set,
    const RulesSet_::Rules_::TerminalRulesMap* terminal_rules,
    StringPiece* terminal) {
  const int terminal_size = terminal->size();
  if (terminal_size < terminal_rules->min_terminal_length() ||
      terminal_size > terminal_rules->max_terminal_length()) {
    return nullptr;
  }
  int terminal_index;
  if (const char* terminal_match = FindTerminal(
          input_iterator, rules_set->terminals()->data(),
          terminal_rules->terminal_offsets()->data(),
          terminal_rules->terminal_offsets()->size(), &terminal_index)) {
    *terminal = StringPiece(terminal_match, terminal->length());
    return rules_set->lhs_set()->Get(
        terminal_rules->lhs_set_index()->Get(terminal_index));
  }
  return nullptr;
}

// Finds unary rules matches.
const RulesSet_::LhsSet* FindUnaryRulesMatches(const RulesSet* rules_set,
                                               const RulesSet_::Rules* rules,
                                               const Nonterm nonterminal) {
  if (!rules->unary_rules()) {
    return nullptr;
  }
  if (const RulesSet_::Rules_::UnaryRulesEntry* entry =
          rules->unary_rules()->LookupByKey(nonterminal)) {
    return rules_set->lhs_set()->Get(entry->value());
  }
  return nullptr;
}

// Finds binary rules matches.
const RulesSet_::LhsSet* FindBinaryRulesMatches(
    const RulesSet* rules_set, const RulesSet_::Rules* rules,
    const TwoNonterms nonterminals) {
  if (!rules->binary_rules()) {
    return nullptr;
  }

  // Lookup in rules hash table.
  const uint32 bucket_index =
      BinaryRuleHasher()(nonterminals) % rules->binary_rules()->size();

  // Get hash table bucket.
  if (const RulesSet_::Rules_::BinaryRuleTableBucket* bucket =
          rules->binary_rules()->Get(bucket_index)) {
    if (bucket->rules() == nullptr) {
      return nullptr;
    }

    // Check all entries in the chain.
    for (const RulesSet_::Rules_::BinaryRule* rule : *bucket->rules()) {
      if (rule->rhs_first() == nonterminals.first &&
          rule->rhs_second() == nonterminals.second) {
        return rules_set->lhs_set()->Get(rule->lhs_set_index());
      }
    }
  }

  return nullptr;
}

inline void GetLhs(const RulesSet* rules_set, const int lhs_entry,
                   Nonterm* nonterminal, CallbackId* callback, uint64* param,
                   int8* max_whitespace_gap) {
  if (lhs_entry > 0) {
    // Direct encoding of the nonterminal.
    *nonterminal = lhs_entry;
    *callback = kNoCallback;
    *param = 0;
    *max_whitespace_gap = -1;
  } else {
    const RulesSet_::Lhs* lhs = rules_set->lhs()->Get(-lhs_entry);
    *nonterminal = lhs->nonterminal();
    *callback = lhs->callback_id();
    *param = lhs->callback_param();
    *max_whitespace_gap = lhs->max_whitespace_gap();
  }
}

}  // namespace

void Matcher::Reset() {
  state_ = STATE_DEFAULT;
  arena_.Reset();
  pending_items_ = nullptr;
  pending_exclusion_items_ = nullptr;
  std::fill(chart_.begin(), chart_.end(), nullptr);
  last_end_ = std::numeric_limits<int>().lowest();
}

void Matcher::Finish() {
  // Check any pending items.
  ProcessPendingExclusionMatches();
}

void Matcher::QueueForProcessing(Match* item) {
  // Push element to the front.
  item->next = pending_items_;
  pending_items_ = item;
}

void Matcher::QueueForPostCheck(ExclusionMatch* item) {
  // Push element to the front.
  item->next = pending_exclusion_items_;
  pending_exclusion_items_ = item;
}

void Matcher::AddTerminal(const CodepointSpan codepoint_span,
                          const int match_offset, StringPiece terminal) {
  TC3_CHECK_GE(codepoint_span.second, last_end_);

  // Finish any pending post-checks.
  if (codepoint_span.second > last_end_) {
    ProcessPendingExclusionMatches();
  }

  last_end_ = codepoint_span.second;
  for (const RulesSet_::Rules* shard : rules_shards_) {
    // Try case-sensitive matches.
    if (const RulesSet_::LhsSet* lhs_set =
            FindTerminalMatches(ByteIterator(terminal), rules_,
                                shard->terminal_rules(), &terminal)) {
      // `terminal` points now into the rules string pool, providing a
      // stable reference.
      ExecuteLhsSet(
          codepoint_span, match_offset,
          /*whitespace_gap=*/(codepoint_span.first - match_offset),
          [terminal](Match* match) {
            match->terminal = terminal.data();
            match->rhs2 = nullptr;
          },
          lhs_set, delegate_);
    }

    // Try case-insensitive matches.
    if (const RulesSet_::LhsSet* lhs_set = FindTerminalMatches(
            LowercasingByteIterator(&unilib_, terminal), rules_,
            shard->lowercase_terminal_rules(), &terminal)) {
      // `terminal` points now into the rules string pool, providing a
      // stable reference.
      ExecuteLhsSet(
          codepoint_span, match_offset,
          /*whitespace_gap=*/(codepoint_span.first - match_offset),
          [terminal](Match* match) {
            match->terminal = terminal.data();
            match->rhs2 = nullptr;
          },
          lhs_set, delegate_);
    }
  }
  ProcessPendingSet();
}

void Matcher::AddMatch(Match* match) {
  TC3_CHECK_GE(match->codepoint_span.second, last_end_);

  // Finish any pending post-checks.
  if (match->codepoint_span.second > last_end_) {
    ProcessPendingExclusionMatches();
  }

  last_end_ = match->codepoint_span.second;
  QueueForProcessing(match);
  ProcessPendingSet();
}

void Matcher::ExecuteLhsSet(const CodepointSpan codepoint_span,
                            const int match_offset_bytes,
                            const int whitespace_gap,
                            const std::function<void(Match*)>& initializer,
                            const RulesSet_::LhsSet* lhs_set,
                            CallbackDelegate* delegate) {
  TC3_CHECK(lhs_set);
  Match* match = nullptr;
  Nonterm prev_lhs = kUnassignedNonterm;
  for (const int32 lhs_entry : *lhs_set->lhs()) {
    Nonterm lhs;
    CallbackId callback_id;
    uint64 callback_param;
    int8 max_whitespace_gap;
    GetLhs(rules_, lhs_entry, &lhs, &callback_id, &callback_param,
           &max_whitespace_gap);

    // Check that the allowed whitespace gap limit is followed.
    if (max_whitespace_gap >= 0 && whitespace_gap > max_whitespace_gap) {
      continue;
    }

    // Handle default callbacks.
    switch (static_cast<DefaultCallback>(callback_id)) {
      case DefaultCallback::kSetType: {
        Match* typed_match = AllocateAndInitMatch<Match>(lhs, codepoint_span,
                                                         match_offset_bytes);
        initializer(typed_match);
        typed_match->type = callback_param;
        QueueForProcessing(typed_match);
        continue;
      }
      case DefaultCallback::kAssertion: {
        AssertionMatch* assertion_match = AllocateAndInitMatch<AssertionMatch>(
            lhs, codepoint_span, match_offset_bytes);
        initializer(assertion_match);
        assertion_match->type = Match::kAssertionMatch;
        assertion_match->negative = (callback_param != 0);
        QueueForProcessing(assertion_match);
        continue;
      }
      case DefaultCallback::kMapping: {
        MappingMatch* mapping_match = AllocateAndInitMatch<MappingMatch>(
            lhs, codepoint_span, match_offset_bytes);
        initializer(mapping_match);
        mapping_match->type = Match::kMappingMatch;
        mapping_match->id = callback_param;
        QueueForProcessing(mapping_match);
        continue;
      }
      case DefaultCallback::kExclusion: {
        // We can only check the exclusion once all matches up to this position
        // have been processed. Schedule and post check later.
        ExclusionMatch* exclusion_match = AllocateAndInitMatch<ExclusionMatch>(
            lhs, codepoint_span, match_offset_bytes);
        initializer(exclusion_match);
        exclusion_match->exclusion_nonterm = callback_param;
        QueueForPostCheck(exclusion_match);
        continue;
      }
      default:
        break;
    }

    if (callback_id != kNoCallback && rules_->callback() != nullptr) {
      const RulesSet_::CallbackEntry* callback_info =
          rules_->callback()->LookupByKey(callback_id);
      if (callback_info && callback_info->value().is_filter()) {
        // Filter callback.
        Match candidate;
        candidate.Init(lhs, codepoint_span, match_offset_bytes);
        initializer(&candidate);
        delegate->MatchFound(&candidate, callback_id, callback_param, this);
        continue;
      }
    }

    if (prev_lhs != lhs) {
      prev_lhs = lhs;
      match =
          AllocateAndInitMatch<Match>(lhs, codepoint_span, match_offset_bytes);
      initializer(match);
      QueueForProcessing(match);
    }

    if (callback_id != kNoCallback) {
      // This is an output callback.
      delegate->MatchFound(match, callback_id, callback_param, this);
    }
  }
}

void Matcher::ProcessPendingSet() {
  // Avoid recursion caused by:
  // ProcessPendingSet --> callback --> AddMatch --> ProcessPendingSet --> ...
  if (state_ == STATE_PROCESSING) {
    return;
  }
  state_ = STATE_PROCESSING;
  while (pending_items_) {
    // Process.
    Match* item = pending_items_;
    pending_items_ = pending_items_->next;

    // Add it to the chart.
    item->next = chart_[item->codepoint_span.second & kChartHashTableBitmask];
    chart_[item->codepoint_span.second & kChartHashTableBitmask] = item;

    // Check unary rules that trigger.
    for (const RulesSet_::Rules* shard : rules_shards_) {
      if (const RulesSet_::LhsSet* lhs_set =
              FindUnaryRulesMatches(rules_, shard, item->lhs)) {
        ExecuteLhsSet(
            item->codepoint_span, item->match_offset,
            /*whitespace_gap=*/
            (item->codepoint_span.first - item->match_offset),
            [item](Match* match) {
              match->rhs1 = nullptr;
              match->rhs2 = item;
            },
            lhs_set, delegate_);
      }
    }

    // Check binary rules that trigger.
    // Lookup by begin.
    Match* prev = chart_[item->match_offset & kChartHashTableBitmask];
    // The chain of items is in decreasing `end` order.
    // Find the ones that have prev->end == item->begin.
    while (prev != nullptr &&
           (prev->codepoint_span.second > item->match_offset)) {
      prev = prev->next;
    }
    for (;
         prev != nullptr && (prev->codepoint_span.second == item->match_offset);
         prev = prev->next) {
      for (const RulesSet_::Rules* shard : rules_shards_) {
        if (const RulesSet_::LhsSet* lhs_set =
                FindBinaryRulesMatches(rules_, shard, {prev->lhs, item->lhs})) {
          ExecuteLhsSet(
              /*codepoint_span=*/
              {prev->codepoint_span.first, item->codepoint_span.second},
              prev->match_offset,
              /*whitespace_gap=*/
              (item->codepoint_span.first -
               item->match_offset),  // Whitespace gap is the gap
                                     // between the two parts.
              [prev, item](Match* match) {
                match->rhs1 = prev;
                match->rhs2 = item;
              },
              lhs_set, delegate_);
        }
      }
    }
  }
  state_ = STATE_DEFAULT;
}

void Matcher::ProcessPendingExclusionMatches() {
  while (pending_exclusion_items_) {
    ExclusionMatch* item = pending_exclusion_items_;
    pending_exclusion_items_ = static_cast<ExclusionMatch*>(item->next);

    // Check that the exclusion condition is fulfilled.
    if (!ContainsMatch(item->exclusion_nonterm, item->codepoint_span)) {
      AddMatch(item);
    }
  }
}

bool Matcher::ContainsMatch(const Nonterm nonterm,
                            const CodepointSpan& span) const {
  // Lookup by end.
  Match* match = chart_[span.second & kChartHashTableBitmask];
  // The chain of items is in decreasing `end` order.
  while (match != nullptr && match->codepoint_span.second > span.second) {
    match = match->next;
  }
  while (match != nullptr && match->codepoint_span.second == span.second) {
    if (match->lhs == nonterm && match->codepoint_span.first == span.first) {
      return true;
    }
    match = match->next;
  }
  return false;
}

}  // namespace libtextclassifier3::grammar
