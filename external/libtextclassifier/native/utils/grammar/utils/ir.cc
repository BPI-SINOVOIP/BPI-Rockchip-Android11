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

#include "utils/grammar/utils/ir.h"

#include "utils/strings/append.h"
#include "utils/strings/stringpiece.h"
#include "utils/zlib/zlib.h"

namespace libtextclassifier3::grammar {
namespace {

constexpr size_t kMaxHashTableSize = 100;

template <typename T>
void SortForBinarySearchLookup(T* entries) {
  std::sort(entries->begin(), entries->end(),
            [](const auto& a, const auto& b) { return a->key < b->key; });
}

template <typename T>
void SortStructsForBinarySearchLookup(T* entries) {
  std::sort(entries->begin(), entries->end(),
            [](const auto& a, const auto& b) { return a.key() < b.key(); });
}

bool IsSameLhs(const Ir::Lhs& lhs, const RulesSet_::Lhs& other) {
  return (lhs.nonterminal == other.nonterminal() &&
          lhs.callback.id == other.callback_id() &&
          lhs.callback.param == other.callback_param() &&
          lhs.preconditions.max_whitespace_gap == other.max_whitespace_gap());
}

bool IsSameLhsEntry(const Ir::Lhs& lhs, const int32 lhs_entry,
                    const std::vector<RulesSet_::Lhs>& candidates) {
  // Simple case: direct encoding of the nonterminal.
  if (lhs_entry > 0) {
    return (lhs.nonterminal == lhs_entry && lhs.callback.id == kNoCallback &&
            lhs.preconditions.max_whitespace_gap == -1);
  }

  // Entry is index into callback lookup.
  return IsSameLhs(lhs, candidates[-lhs_entry]);
}

bool IsSameLhsSet(const Ir::LhsSet& lhs_set,
                  const RulesSet_::LhsSetT& candidate,
                  const std::vector<RulesSet_::Lhs>& candidates) {
  if (lhs_set.size() != candidate.lhs.size()) {
    return false;
  }

  for (int i = 0; i < lhs_set.size(); i++) {
    // Check that entries are the same.
    if (!IsSameLhsEntry(lhs_set[i], candidate.lhs[i], candidates)) {
      return false;
    }
  }

  return false;
}

Ir::LhsSet SortedLhsSet(const Ir::LhsSet& lhs_set) {
  Ir::LhsSet sorted_lhs = lhs_set;
  std::sort(sorted_lhs.begin(), sorted_lhs.end(),
            [](const Ir::Lhs& a, const Ir::Lhs& b) {
              return std::tie(a.nonterminal, a.callback.id, a.callback.param,
                              a.preconditions.max_whitespace_gap) <
                     std::tie(b.nonterminal, b.callback.id, b.callback.param,
                              b.preconditions.max_whitespace_gap);
            });
  return lhs_set;
}

// Adds a new lhs match set to the output.
// Reuses the same set, if it was previously observed.
int AddLhsSet(const Ir::LhsSet& lhs_set, RulesSetT* rules_set) {
  Ir::LhsSet sorted_lhs = SortedLhsSet(lhs_set);
  // Check whether we can reuse an entry.
  const int output_size = rules_set->lhs_set.size();
  for (int i = 0; i < output_size; i++) {
    if (IsSameLhsSet(lhs_set, *rules_set->lhs_set[i], rules_set->lhs)) {
      return i;
    }
  }

  // Add new entry.
  rules_set->lhs_set.emplace_back(std::make_unique<RulesSet_::LhsSetT>());
  RulesSet_::LhsSetT* serialized_lhs_set = rules_set->lhs_set.back().get();
  for (const Ir::Lhs& lhs : lhs_set) {
    // Simple case: No callback and no special requirements, we directly encode
    // the nonterminal.
    if (lhs.callback.id == kNoCallback &&
        lhs.preconditions.max_whitespace_gap < 0) {
      serialized_lhs_set->lhs.push_back(lhs.nonterminal);
    } else {
      // Check whether we can reuse a callback entry.
      const int lhs_size = rules_set->lhs.size();
      bool found_entry = false;
      for (int i = 0; i < lhs_size; i++) {
        if (IsSameLhs(lhs, rules_set->lhs[i])) {
          found_entry = true;
          serialized_lhs_set->lhs.push_back(-i);
          break;
        }
      }

      // We could reuse an existing entry.
      if (found_entry) {
        continue;
      }

      // Add a new one.
      rules_set->lhs.push_back(
          RulesSet_::Lhs(lhs.nonterminal, lhs.callback.id, lhs.callback.param,
                         lhs.preconditions.max_whitespace_gap));
      serialized_lhs_set->lhs.push_back(-lhs_size);
    }
  }
  return output_size;
}

// Serializes a unary rules table.
void SerializeUnaryRulesShard(
    const std::unordered_map<Nonterm, Ir::LhsSet>& unary_rules,
    RulesSetT* rules_set, RulesSet_::RulesT* rules) {
  for (const auto& it : unary_rules) {
    rules->unary_rules.push_back(RulesSet_::Rules_::UnaryRulesEntry(
        it.first, AddLhsSet(it.second, rules_set)));
  }
  SortStructsForBinarySearchLookup(&rules->unary_rules);
}

// // Serializes a binary rules table.
void SerializeBinaryRulesShard(
    const std::unordered_map<TwoNonterms, Ir::LhsSet, BinaryRuleHasher>&
        binary_rules,
    RulesSetT* rules_set, RulesSet_::RulesT* rules) {
  const size_t num_buckets = std::min(binary_rules.size(), kMaxHashTableSize);
  for (int i = 0; i < num_buckets; i++) {
    rules->binary_rules.emplace_back(
        new RulesSet_::Rules_::BinaryRuleTableBucketT());
  }

  // Serialize the table.
  BinaryRuleHasher hash;
  for (const auto& it : binary_rules) {
    const TwoNonterms key = it.first;
    uint32 bucket_index = hash(key) % num_buckets;

    // Add entry to bucket chain list.
    rules->binary_rules[bucket_index]->rules.push_back(
        RulesSet_::Rules_::BinaryRule(key.first, key.second,
                                      AddLhsSet(it.second, rules_set)));
  }
}

}  // namespace

Nonterm Ir::AddToSet(const Lhs& lhs, LhsSet* lhs_set) {
  const int lhs_set_size = lhs_set->size();
  Nonterm shareable_nonterm = lhs.nonterminal;
  for (int i = 0; i < lhs_set_size; i++) {
    Lhs* candidate = &lhs_set->at(i);

    // Exact match, just reuse rule.
    if (lhs == *candidate) {
      return candidate->nonterminal;
    }

    // Cannot reuse unshareable ids.
    if (nonshareable_.find(candidate->nonterminal) != nonshareable_.end() ||
        nonshareable_.find(lhs.nonterminal) != nonshareable_.end()) {
      continue;
    }

    // Cannot reuse id if the preconditions are different.
    if (!(lhs.preconditions == candidate->preconditions)) {
      continue;
    }

    // If either callback is a filter, we can't share as we must always run
    // both filters.
    if ((lhs.callback.id != kNoCallback &&
         filters_.find(lhs.callback.id) != filters_.end()) ||
        (candidate->callback.id != kNoCallback &&
         filters_.find(candidate->callback.id) != filters_.end())) {
      continue;
    }

    // If the nonterminal is already defined, it must match for sharing.
    if (lhs.nonterminal != kUnassignedNonterm &&
        lhs.nonterminal != candidate->nonterminal) {
      continue;
    }

    // Check whether the callbacks match.
    if (lhs.callback == candidate->callback) {
      return candidate->nonterminal;
    }

    // We can reuse if one of the output callbacks is not used.
    if (lhs.callback.id == kNoCallback) {
      return candidate->nonterminal;
    } else if (candidate->callback.id == kNoCallback) {
      // Old entry has no output callback, which is redundant now.
      candidate->callback = lhs.callback;
      return candidate->nonterminal;
    }

    // We can share the nonterminal, but we need to
    // add a new output callback. Defer this as we might find a shareable
    // nonterminal first.
    shareable_nonterm = candidate->nonterminal;
  }

  // We didn't find a redundant entry, so create a new one.
  shareable_nonterm = DefineNonterminal(shareable_nonterm);
  lhs_set->push_back(Lhs{shareable_nonterm, lhs.callback, lhs.preconditions});
  return shareable_nonterm;
}

Nonterm Ir::Add(const Lhs& lhs, const std::string& terminal,
                const bool case_sensitive, const int shard) {
  TC3_CHECK_LT(shard, shards_.size());
  if (case_sensitive) {
    return AddRule(lhs, terminal, &shards_[shard].terminal_rules);
  } else {
    return AddRule(lhs, terminal, &shards_[shard].lowercase_terminal_rules);
  }
}

Nonterm Ir::Add(const Lhs& lhs, const std::vector<Nonterm>& rhs,
                const int shard) {
  // Add a new unary rule.
  if (rhs.size() == 1) {
    return Add(lhs, rhs.front(), shard);
  }

  // Add a chain of (rhs.size() - 1) binary rules.
  Nonterm prev = rhs.front();
  for (int i = 1; i < rhs.size() - 1; i++) {
    prev = Add(kUnassignedNonterm, prev, rhs[i], shard);
  }
  return Add(lhs, prev, rhs.back(), shard);
}

Nonterm Ir::AddRegex(Nonterm lhs, const std::string& regex_pattern) {
  lhs = DefineNonterminal(lhs);
  regex_rules_.emplace_back(regex_pattern, lhs);
  return lhs;
}

void Ir::AddAnnotation(const Nonterm lhs, const std::string& annotation) {
  annotations_.emplace_back(annotation, lhs);
}

// Serializes the terminal rules table.
void Ir::SerializeTerminalRules(
    RulesSetT* rules_set,
    std::vector<std::unique_ptr<RulesSet_::RulesT>>* rules_shards) const {
  // Use common pool for all terminals.
  struct TerminalEntry {
    std::string terminal;
    int set_index;
    int index;
    Ir::LhsSet lhs_set;
  };
  std::vector<TerminalEntry> terminal_rules;

  // Merge all terminals into a common pool.
  // We want to use one common pool, but still need to track which set they
  // belong to.
  std::vector<const std::unordered_map<std::string, Ir::LhsSet>*>
      terminal_rules_sets;
  std::vector<RulesSet_::Rules_::TerminalRulesMapT*> rules_maps;
  terminal_rules_sets.reserve(2 * shards_.size());
  rules_maps.reserve(terminal_rules_sets.size());
  for (int i = 0; i < shards_.size(); i++) {
    terminal_rules_sets.push_back(&shards_[i].terminal_rules);
    terminal_rules_sets.push_back(&shards_[i].lowercase_terminal_rules);
    rules_shards->at(i)->terminal_rules.reset(
        new RulesSet_::Rules_::TerminalRulesMapT());
    rules_shards->at(i)->lowercase_terminal_rules.reset(
        new RulesSet_::Rules_::TerminalRulesMapT());
    rules_maps.push_back(rules_shards->at(i)->terminal_rules.get());
    rules_maps.push_back(rules_shards->at(i)->lowercase_terminal_rules.get());
  }
  for (int i = 0; i < terminal_rules_sets.size(); i++) {
    for (const auto& it : *terminal_rules_sets[i]) {
      terminal_rules.push_back(
          TerminalEntry{it.first, /*set_index=*/i, /*index=*/0, it.second});
    }
  }
  std::sort(terminal_rules.begin(), terminal_rules.end(),
            [](const TerminalEntry& a, const TerminalEntry& b) {
              return a.terminal < b.terminal;
            });

  // Index the entries in sorted order.
  std::vector<int> index(terminal_rules_sets.size(), 0);
  for (int i = 0; i < terminal_rules.size(); i++) {
    terminal_rules[i].index = index[terminal_rules[i].set_index]++;
  }

  // We store the terminal strings sorted into a buffer and keep offsets into
  // that buffer. In this way, we don't need extra space for terminals that are
  // suffixes of others.

  // Find terminals that are a suffix of others, O(n^2) algorithm.
  constexpr int kInvalidIndex = -1;
  std::vector<int> suffix(terminal_rules.size(), kInvalidIndex);
  for (int i = 0; i < terminal_rules.size(); i++) {
    const StringPiece terminal(terminal_rules[i].terminal);

    // Check whether the ith terminal is a suffix of another.
    for (int j = 0; j < terminal_rules.size(); j++) {
      if (i == j) {
        continue;
      }
      if (StringPiece(terminal_rules[j].terminal).EndsWith(terminal)) {
        // If both terminals are the same keep the first.
        // This avoids cyclic dependencies.
        // This can happen if multiple shards use same terminals, such as
        // punctuation.
        if (terminal_rules[j].terminal.size() == terminal.size() && j < i) {
          continue;
        }
        suffix[i] = j;
        break;
      }
    }
  }

  rules_set->terminals = "";

  for (int i = 0; i < terminal_rules_sets.size(); i++) {
    rules_maps[i]->terminal_offsets.resize(terminal_rules_sets[i]->size());
    rules_maps[i]->max_terminal_length = 0;
    rules_maps[i]->min_terminal_length = std::numeric_limits<int>::max();
  }

  for (int i = 0; i < terminal_rules.size(); i++) {
    const TerminalEntry& entry = terminal_rules[i];

    // Update bounds.
    rules_maps[entry.set_index]->min_terminal_length =
        std::min(rules_maps[entry.set_index]->min_terminal_length,
                 static_cast<int>(entry.terminal.size()));
    rules_maps[entry.set_index]->max_terminal_length =
        std::max(rules_maps[entry.set_index]->max_terminal_length,
                 static_cast<int>(entry.terminal.size()));

    // Only include terminals that are not suffixes of others.
    if (suffix[i] != kInvalidIndex) {
      continue;
    }

    rules_maps[entry.set_index]->terminal_offsets[entry.index] =
        rules_set->terminals.length();
    rules_set->terminals += entry.terminal + '\0';
  }

  // Store just an offset into the existing terminal data for the terminals
  // that are suffixes of others.
  for (int i = 0; i < terminal_rules.size(); i++) {
    int canonical_index = i;
    if (suffix[canonical_index] == kInvalidIndex) {
      continue;
    }

    // Find the overlapping string that was included in the data.
    while (suffix[canonical_index] != kInvalidIndex) {
      canonical_index = suffix[canonical_index];
    }

    const TerminalEntry& entry = terminal_rules[i];
    const TerminalEntry& canonical_entry = terminal_rules[canonical_index];

    // The offset is the offset of the overlapping string and the offset within
    // that string.
    rules_maps[entry.set_index]->terminal_offsets[entry.index] =
        rules_maps[canonical_entry.set_index]
            ->terminal_offsets[canonical_entry.index] +
        (canonical_entry.terminal.length() - entry.terminal.length());
  }

  for (const TerminalEntry& entry : terminal_rules) {
    rules_maps[entry.set_index]->lhs_set_index.push_back(
        AddLhsSet(entry.lhs_set, rules_set));
  }
}

void Ir::Serialize(const bool include_debug_information,
                   RulesSetT* output) const {
  // Set callback information.
  for (const CallbackId filter_callback_id : filters_) {
    output->callback.push_back(RulesSet_::CallbackEntry(
        filter_callback_id, RulesSet_::Callback(/*is_filter=*/true)));
  }
  SortStructsForBinarySearchLookup(&output->callback);

  // Add information about predefined nonterminal classes.
  output->nonterminals.reset(new RulesSet_::NonterminalsT);
  output->nonterminals->start_nt = GetNonterminalForName(kStartNonterm);
  output->nonterminals->end_nt = GetNonterminalForName(kEndNonterm);
  output->nonterminals->wordbreak_nt = GetNonterminalForName(kWordBreakNonterm);
  output->nonterminals->token_nt = GetNonterminalForName(kTokenNonterm);
  output->nonterminals->uppercase_token_nt =
      GetNonterminalForName(kUppercaseTokenNonterm);
  output->nonterminals->digits_nt = GetNonterminalForName(kDigitsNonterm);
  for (int i = 1; i <= kMaxNDigitsNontermLength; i++) {
    if (const Nonterm n_digits_nt =
            GetNonterminalForName(strings::StringPrintf(kNDigitsNonterm, i))) {
      output->nonterminals->n_digits_nt.resize(i, kUnassignedNonterm);
      output->nonterminals->n_digits_nt[i - 1] = n_digits_nt;
    }
  }
  for (const auto& [annotation, annotation_nt] : annotations_) {
    output->nonterminals->annotation_nt.emplace_back(
        new RulesSet_::Nonterminals_::AnnotationNtEntryT);
    output->nonterminals->annotation_nt.back()->key = annotation;
    output->nonterminals->annotation_nt.back()->value = annotation_nt;
  }
  SortForBinarySearchLookup(&output->nonterminals->annotation_nt);

  if (include_debug_information) {
    output->debug_information.reset(new RulesSet_::DebugInformationT);
    // Keep original non-terminal names.
    for (const auto& it : nonterminal_names_) {
      output->debug_information->nonterminal_names.emplace_back(
          new RulesSet_::DebugInformation_::NonterminalNamesEntryT);
      output->debug_information->nonterminal_names.back()->key = it.first;
      output->debug_information->nonterminal_names.back()->value = it.second;
    }
    SortForBinarySearchLookup(&output->debug_information->nonterminal_names);
  }

  // Add regex rules.
  std::unique_ptr<ZlibCompressor> compressor = ZlibCompressor::Instance();
  for (auto [pattern, lhs] : regex_rules_) {
    output->regex_annotator.emplace_back(new RulesSet_::RegexAnnotatorT);
    output->regex_annotator.back()->compressed_pattern.reset(
        new CompressedBufferT);
    compressor->Compress(
        pattern, output->regex_annotator.back()->compressed_pattern.get());
    output->regex_annotator.back()->nonterminal = lhs;
  }

  // Serialize the unary and binary rules.
  for (const RulesShard& shard : shards_) {
    output->rules.emplace_back(std::make_unique<RulesSet_::RulesT>());
    RulesSet_::RulesT* rules = output->rules.back().get();
    // Serialize the unary rules.
    SerializeUnaryRulesShard(shard.unary_rules, output, rules);

    // Serialize the binary rules.
    SerializeBinaryRulesShard(shard.binary_rules, output, rules);
  }

  // Serialize the terminal rules.
  // We keep the rules separate by shard but merge the actual terminals into
  // one shared string pool to most effectively exploit reuse.
  SerializeTerminalRules(output, &output->rules);
}

std::string Ir::SerializeAsFlatbuffer(
    const bool include_debug_information) const {
  RulesSetT output;
  Serialize(include_debug_information, &output);
  flatbuffers::FlatBufferBuilder builder;
  builder.Finish(RulesSet::Pack(builder, &output));
  return std::string(reinterpret_cast<const char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

}  // namespace libtextclassifier3::grammar
