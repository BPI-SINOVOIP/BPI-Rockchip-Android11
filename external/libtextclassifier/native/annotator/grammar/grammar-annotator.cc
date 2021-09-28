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

#include "annotator/grammar/grammar-annotator.h"

#include "annotator/feature-processor.h"
#include "annotator/grammar/utils.h"
#include "annotator/types.h"
#include "utils/base/logging.h"
#include "utils/grammar/callback-delegate.h"
#include "utils/grammar/match.h"
#include "utils/grammar/matcher.h"
#include "utils/grammar/rules-utils.h"
#include "utils/grammar/types.h"
#include "utils/normalization.h"
#include "utils/optional.h"
#include "utils/utf8/unicodetext.h"

namespace libtextclassifier3 {
namespace {

// Returns the unicode codepoint offsets in a utf8 encoded text.
std::vector<UnicodeText::const_iterator> UnicodeCodepointOffsets(
    const UnicodeText& text) {
  std::vector<UnicodeText::const_iterator> offsets;
  for (auto it = text.begin(); it != text.end(); it++) {
    offsets.push_back(it);
  }
  offsets.push_back(text.end());
  return offsets;
}

}  // namespace

class GrammarAnnotatorCallbackDelegate : public grammar::CallbackDelegate {
 public:
  explicit GrammarAnnotatorCallbackDelegate(
      const UniLib* unilib, const GrammarModel* model,
      const ReflectiveFlatbufferBuilder* entity_data_builder,
      const ModeFlag mode)
      : unilib_(*unilib),
        model_(model),
        entity_data_builder_(entity_data_builder),
        mode_(mode) {}

  // Handles a grammar rule match in the annotator grammar.
  void MatchFound(const grammar::Match* match, grammar::CallbackId type,
                  int64 value, grammar::Matcher* matcher) override {
    switch (static_cast<GrammarAnnotator::Callback>(type)) {
      case GrammarAnnotator::Callback::kRuleMatch: {
        HandleRuleMatch(match, /*rule_id=*/value);
        return;
      }
      default:
        grammar::CallbackDelegate::MatchFound(match, type, value, matcher);
    }
  }

  // Deduplicate and populate annotations from grammar matches.
  bool GetAnnotations(const std::vector<UnicodeText::const_iterator>& text,
                      std::vector<AnnotatedSpan>* annotations) const {
    for (const grammar::Derivation& candidate :
         grammar::DeduplicateDerivations(candidates_)) {
      // Check that assertions are fulfilled.
      if (!grammar::VerifyAssertions(candidate.match)) {
        continue;
      }
      if (!AddAnnotatedSpanFromMatch(text, candidate, annotations)) {
        return false;
      }
    }
    return true;
  }

  bool GetTextSelection(const std::vector<UnicodeText::const_iterator>& text,
                        const CodepointSpan& selection, AnnotatedSpan* result) {
    std::vector<grammar::Derivation> selection_candidates;
    // Deduplicate and verify matches.
    auto maybe_interpretation = GetBestValidInterpretation(
        grammar::DeduplicateDerivations(GetOverlappingRuleMatches(
            selection, candidates_, /*only_exact_overlap=*/false)));
    if (!maybe_interpretation.has_value()) {
      return false;
    }
    const GrammarModel_::RuleClassificationResult* interpretation;
    const grammar::Match* match;
    std::tie(interpretation, match) = maybe_interpretation.value();
    return InstantiateAnnotatedSpanFromInterpretation(text, interpretation,
                                                      match, result);
  }

  // Provides a classification results from the grammar matches.
  bool GetClassification(const std::vector<UnicodeText::const_iterator>& text,
                         const CodepointSpan& selection,
                         ClassificationResult* classification) const {
    // Deduplicate and verify matches.
    auto maybe_interpretation = GetBestValidInterpretation(
        grammar::DeduplicateDerivations(GetOverlappingRuleMatches(
            selection, candidates_, /*only_exact_overlap=*/true)));
    if (!maybe_interpretation.has_value()) {
      return false;
    }

    // Instantiate result.
    const GrammarModel_::RuleClassificationResult* interpretation;
    const grammar::Match* match;
    std::tie(interpretation, match) = maybe_interpretation.value();
    return InstantiateClassificationInterpretation(text, interpretation, match,
                                                   classification);
  }

 private:
  // Handles annotation/selection/classification rule matches.
  void HandleRuleMatch(const grammar::Match* match, const int64 rule_id) {
    if ((model_->rule_classification_result()->Get(rule_id)->enabled_modes() &
         mode_) != 0) {
      candidates_.push_back(grammar::Derivation{match, rule_id});
    }
  }

  // Computes the selection boundaries from a grammar match.
  CodepointSpan MatchSelectionBoundaries(
      const grammar::Match* match,
      const GrammarModel_::RuleClassificationResult* classification) const {
    if (classification->capturing_group() == nullptr) {
      // Use full match as selection span.
      return match->codepoint_span;
    }

    // Set information from capturing matches.
    CodepointSpan span{kInvalidIndex, kInvalidIndex};
    // Gather active capturing matches.
    std::unordered_map<uint16, const grammar::Match*> capturing_matches;
    for (const grammar::MappingMatch* match :
         grammar::SelectAllOfType<grammar::MappingMatch>(
             match, grammar::Match::kMappingMatch)) {
      capturing_matches[match->id] = match;
    }

    // Compute span boundaries.
    for (int i = 0; i < classification->capturing_group()->size(); i++) {
      auto it = capturing_matches.find(i);
      if (it == capturing_matches.end()) {
        // Capturing group is not active, skip.
        continue;
      }
      const CapturingGroup* group = classification->capturing_group()->Get(i);
      if (group->extend_selection()) {
        if (span.first == kInvalidIndex) {
          span = it->second->codepoint_span;
        } else {
          span.first = std::min(span.first, it->second->codepoint_span.first);
          span.second =
              std::max(span.second, it->second->codepoint_span.second);
        }
      }
    }
    return span;
  }

  // Filters out results that do not overlap with a reference span.
  std::vector<grammar::Derivation> GetOverlappingRuleMatches(
      const CodepointSpan& selection,
      const std::vector<grammar::Derivation>& candidates,
      const bool only_exact_overlap) const {
    std::vector<grammar::Derivation> result;
    for (const grammar::Derivation& candidate : candidates) {
      // Discard matches that do not match the selection.
      // Simple check.
      if (!SpansOverlap(selection, candidate.match->codepoint_span)) {
        continue;
      }

      // Compute exact selection boundaries (without assertions and
      // non-capturing parts).
      const CodepointSpan span = MatchSelectionBoundaries(
          candidate.match,
          model_->rule_classification_result()->Get(candidate.rule_id));
      if (!SpansOverlap(selection, span) ||
          (only_exact_overlap && span != selection)) {
        continue;
      }
      result.push_back(candidate);
    }
    return result;
  }

  // Returns the best valid interpretation of a set of candidate matches.
  Optional<std::pair<const GrammarModel_::RuleClassificationResult*,
                     const grammar::Match*>>
  GetBestValidInterpretation(
      const std::vector<grammar::Derivation>& candidates) const {
    const GrammarModel_::RuleClassificationResult* best_interpretation =
        nullptr;
    const grammar::Match* best_match = nullptr;
    for (const grammar::Derivation& candidate : candidates) {
      if (!grammar::VerifyAssertions(candidate.match)) {
        continue;
      }
      const GrammarModel_::RuleClassificationResult*
          rule_classification_result =
              model_->rule_classification_result()->Get(candidate.rule_id);
      if (best_interpretation == nullptr ||
          best_interpretation->priority_score() <
              rule_classification_result->priority_score()) {
        best_interpretation = rule_classification_result;
        best_match = candidate.match;
      }
    }

    // No valid interpretation found.
    Optional<std::pair<const GrammarModel_::RuleClassificationResult*,
                       const grammar::Match*>>
        result;
    if (best_interpretation != nullptr) {
      result = {best_interpretation, best_match};
    }
    return result;
  }

  // Instantiates an annotated span from a rule match and appends it to the
  // result.
  bool AddAnnotatedSpanFromMatch(
      const std::vector<UnicodeText::const_iterator>& text,
      const grammar::Derivation& candidate,
      std::vector<AnnotatedSpan>* result) const {
    if (candidate.rule_id < 0 ||
        candidate.rule_id >= model_->rule_classification_result()->size()) {
      TC3_LOG(INFO) << "Invalid rule id.";
      return false;
    }
    const GrammarModel_::RuleClassificationResult* interpretation =
        model_->rule_classification_result()->Get(candidate.rule_id);
    result->emplace_back();
    return InstantiateAnnotatedSpanFromInterpretation(
        text, interpretation, candidate.match, &result->back());
  }

  bool InstantiateAnnotatedSpanFromInterpretation(
      const std::vector<UnicodeText::const_iterator>& text,
      const GrammarModel_::RuleClassificationResult* interpretation,
      const grammar::Match* match, AnnotatedSpan* result) const {
    result->span = MatchSelectionBoundaries(match, interpretation);
    ClassificationResult classification;
    if (!InstantiateClassificationInterpretation(text, interpretation, match,
                                                 &classification)) {
      return false;
    }
    result->classification.push_back(classification);
    return true;
  }

  // Instantiates a classification result from a rule match.
  bool InstantiateClassificationInterpretation(
      const std::vector<UnicodeText::const_iterator>& text,
      const GrammarModel_::RuleClassificationResult* interpretation,
      const grammar::Match* match, ClassificationResult* classification) const {
    classification->collection = interpretation->collection_name()->str();
    classification->score = interpretation->target_classification_score();
    classification->priority_score = interpretation->priority_score();

    // Assemble entity data.
    if (entity_data_builder_ == nullptr) {
      return true;
    }
    std::unique_ptr<ReflectiveFlatbuffer> entity_data =
        entity_data_builder_->NewRoot();
    if (interpretation->serialized_entity_data() != nullptr) {
      entity_data->MergeFromSerializedFlatbuffer(
          StringPiece(interpretation->serialized_entity_data()->data(),
                      interpretation->serialized_entity_data()->size()));
    }
    if (interpretation->entity_data() != nullptr) {
      entity_data->MergeFrom(reinterpret_cast<const flatbuffers::Table*>(
          interpretation->entity_data()));
    }

    // Populate entity data from the capturing matches.
    if (interpretation->capturing_group() != nullptr) {
      // Gather active capturing matches.
      std::unordered_map<uint16, const grammar::Match*> capturing_matches;
      for (const grammar::MappingMatch* match :
           grammar::SelectAllOfType<grammar::MappingMatch>(
               match, grammar::Match::kMappingMatch)) {
        capturing_matches[match->id] = match;
      }
      for (int i = 0; i < interpretation->capturing_group()->size(); i++) {
        auto it = capturing_matches.find(i);
        if (it == capturing_matches.end()) {
          // Capturing group is not active, skip.
          continue;
        }
        const CapturingGroup* group = interpretation->capturing_group()->Get(i);

        // Add static entity data.
        if (group->serialized_entity_data() != nullptr) {
          entity_data->MergeFromSerializedFlatbuffer(
              StringPiece(interpretation->serialized_entity_data()->data(),
                          interpretation->serialized_entity_data()->size()));
        }

        // Set entity field from captured text.
        if (group->entity_field_path() != nullptr) {
          const grammar::Match* capturing_match = it->second;
          StringPiece group_text = StringPiece(
              text[capturing_match->codepoint_span.first].utf8_data(),
              text[capturing_match->codepoint_span.second].utf8_data() -
                  text[capturing_match->codepoint_span.first].utf8_data());
          UnicodeText normalized_group_text =
              UTF8ToUnicodeText(group_text, /*do_copy=*/false);
          if (group->normalization_options() != nullptr) {
            normalized_group_text = NormalizeText(
                unilib_, group->normalization_options(), normalized_group_text);
          }
          if (!entity_data->ParseAndSet(group->entity_field_path(),
                                        normalized_group_text.ToUTF8String())) {
            TC3_LOG(ERROR) << "Could not set entity data from capturing match.";
            return false;
          }
        }
      }
    }

    if (entity_data && entity_data->HasExplicitlySetFields()) {
      classification->serialized_entity_data = entity_data->Serialize();
    }
    return true;
  }

  const UniLib& unilib_;
  const GrammarModel* model_;
  const ReflectiveFlatbufferBuilder* entity_data_builder_;
  const ModeFlag mode_;

  // All annotation/selection/classification rule match candidates.
  // Grammar rule matches are recorded, deduplicated and then instantiated.
  std::vector<grammar::Derivation> candidates_;
};

GrammarAnnotator::GrammarAnnotator(
    const UniLib* unilib, const GrammarModel* model,
    const ReflectiveFlatbufferBuilder* entity_data_builder)
    : unilib_(*unilib),
      model_(model),
      lexer_(unilib, model->rules()),
      tokenizer_(BuildTokenizer(unilib, model->tokenizer_options())),
      entity_data_builder_(entity_data_builder),
      rules_locales_(grammar::ParseRulesLocales(model->rules())) {}

bool GrammarAnnotator::Annotate(const std::vector<Locale>& locales,
                                const UnicodeText& text,
                                std::vector<AnnotatedSpan>* result) const {
  if (model_ == nullptr || model_->rules() == nullptr) {
    // Nothing to do.
    return true;
  }

  // Select locale matching rules.
  std::vector<const grammar::RulesSet_::Rules*> locale_rules =
      SelectLocaleMatchingShards(model_->rules(), rules_locales_, locales);
  if (locale_rules.empty()) {
    // Nothing to do.
    return true;
  }

  // Run the grammar.
  GrammarAnnotatorCallbackDelegate callback_handler(
      &unilib_, model_, entity_data_builder_,
      /*mode=*/ModeFlag_ANNOTATION);
  grammar::Matcher matcher(&unilib_, model_->rules(), locale_rules,
                           &callback_handler);
  lexer_.Process(text, tokenizer_.Tokenize(text), /*annotations=*/nullptr,
                 &matcher);

  // Populate results.
  return callback_handler.GetAnnotations(UnicodeCodepointOffsets(text), result);
}

bool GrammarAnnotator::SuggestSelection(const std::vector<Locale>& locales,
                                        const UnicodeText& text,
                                        const CodepointSpan& selection,
                                        AnnotatedSpan* result) const {
  if (model_ == nullptr || model_->rules() == nullptr ||
      selection == CodepointSpan{kInvalidIndex, kInvalidIndex}) {
    // Nothing to do.
    return false;
  }

  // Select locale matching rules.
  std::vector<const grammar::RulesSet_::Rules*> locale_rules =
      SelectLocaleMatchingShards(model_->rules(), rules_locales_, locales);
  if (locale_rules.empty()) {
    // Nothing to do.
    return true;
  }

  // Run the grammar.
  GrammarAnnotatorCallbackDelegate callback_handler(
      &unilib_, model_, entity_data_builder_,
      /*mode=*/ModeFlag_SELECTION);
  grammar::Matcher matcher(&unilib_, model_->rules(), locale_rules,
                           &callback_handler);
  lexer_.Process(text, tokenizer_.Tokenize(text), /*annotations=*/nullptr,
                 &matcher);

  // Populate the result.
  return callback_handler.GetTextSelection(UnicodeCodepointOffsets(text),
                                           selection, result);
}

bool GrammarAnnotator::ClassifyText(
    const std::vector<Locale>& locales, const UnicodeText& text,
    const CodepointSpan& selection,
    ClassificationResult* classification_result) const {
  if (model_ == nullptr || model_->rules() == nullptr ||
      selection == CodepointSpan{kInvalidIndex, kInvalidIndex}) {
    // Nothing to do.
    return false;
  }

  // Select locale matching rules.
  std::vector<const grammar::RulesSet_::Rules*> locale_rules =
      SelectLocaleMatchingShards(model_->rules(), rules_locales_, locales);
  if (locale_rules.empty()) {
    // Nothing to do.
    return false;
  }

  // Run the grammar.
  GrammarAnnotatorCallbackDelegate callback_handler(
      &unilib_, model_, entity_data_builder_,
      /*mode=*/ModeFlag_CLASSIFICATION);
  grammar::Matcher matcher(&unilib_, model_->rules(), locale_rules,
                           &callback_handler);

  const std::vector<Token> tokens = tokenizer_.Tokenize(text);
  if (model_->context_left_num_tokens() == -1 &&
      model_->context_right_num_tokens() == -1) {
    // Use all tokens.
    lexer_.Process(text, tokens, /*annotations=*/{}, &matcher);
  } else {
    TokenSpan context_span = CodepointSpanToTokenSpan(
        tokens, selection, /*snap_boundaries_to_containing_tokens=*/true);
    std::vector<Token>::const_iterator begin = tokens.begin();
    std::vector<Token>::const_iterator end = tokens.begin();
    if (model_->context_left_num_tokens() != -1) {
      std::advance(begin, std::max(0, context_span.first -
                                          model_->context_left_num_tokens()));
    }
    if (model_->context_right_num_tokens() == -1) {
      end = tokens.end();
    } else {
      std::advance(end, std::min(static_cast<int>(tokens.size()),
                                 context_span.second +
                                     model_->context_right_num_tokens()));
    }
    lexer_.Process(text, begin, end,
                   /*annotations=*/nullptr, &matcher);
  }

  // Populate result.
  return callback_handler.GetClassification(UnicodeCodepointOffsets(text),
                                            selection, classification_result);
}

}  // namespace libtextclassifier3
