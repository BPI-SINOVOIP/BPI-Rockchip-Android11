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

#include "actions/grammar-actions.h"

#include <algorithm>
#include <unordered_map>

#include "actions/feature-processor.h"
#include "actions/utils.h"
#include "annotator/types.h"
#include "utils/grammar/callback-delegate.h"
#include "utils/grammar/match.h"
#include "utils/grammar/matcher.h"
#include "utils/grammar/rules-utils.h"
#include "utils/i18n/language-tag_generated.h"
#include "utils/utf8/unicodetext.h"

namespace libtextclassifier3 {
namespace {

class GrammarActionsCallbackDelegate : public grammar::CallbackDelegate {
 public:
  GrammarActionsCallbackDelegate(const UniLib* unilib,
                                 const RulesModel_::GrammarRules* grammar_rules)
      : unilib_(*unilib), grammar_rules_(grammar_rules) {}

  // Handle a grammar rule match in the actions grammar.
  void MatchFound(const grammar::Match* match, grammar::CallbackId type,
                  int64 value, grammar::Matcher* matcher) override {
    switch (static_cast<GrammarActions::Callback>(type)) {
      case GrammarActions::Callback::kActionRuleMatch: {
        HandleRuleMatch(match, /*rule_id=*/value);
        return;
      }
      default:
        grammar::CallbackDelegate::MatchFound(match, type, value, matcher);
    }
  }

  // Deduplicate, verify and populate actions from grammar matches.
  bool GetActions(const Conversation& conversation,
                  const std::string& smart_reply_action_type,
                  const ReflectiveFlatbufferBuilder* entity_data_builder,
                  std::vector<ActionSuggestion>* action_suggestions) const {
    std::vector<UnicodeText::const_iterator> codepoint_offsets;
    const UnicodeText message_unicode =
        UTF8ToUnicodeText(conversation.messages.back().text,
                          /*do_copy=*/false);
    for (auto it = message_unicode.begin(); it != message_unicode.end(); it++) {
      codepoint_offsets.push_back(it);
    }
    codepoint_offsets.push_back(message_unicode.end());
    for (const grammar::Derivation& candidate :
         grammar::DeduplicateDerivations(candidates_)) {
      // Check that assertions are fulfilled.
      if (!VerifyAssertions(candidate.match)) {
        continue;
      }
      if (!InstantiateActionsFromMatch(
              codepoint_offsets,
              /*message_index=*/conversation.messages.size() - 1,
              smart_reply_action_type, candidate, entity_data_builder,
              action_suggestions)) {
        return false;
      }
    }
    return true;
  }

 private:
  // Handles action rule matches.
  void HandleRuleMatch(const grammar::Match* match, const int64 rule_id) {
    candidates_.push_back(grammar::Derivation{match, rule_id});
  }

  // Instantiates action suggestions from verified and deduplicated rule matches
  // and appends them to the result.
  // Expects the message as codepoints for text extraction from capturing
  // matches as well as the index of the message, for correct span production.
  bool InstantiateActionsFromMatch(
      const std::vector<UnicodeText::const_iterator>& message_codepoint_offsets,
      int message_index, const std::string& smart_reply_action_type,
      const grammar::Derivation& candidate,
      const ReflectiveFlatbufferBuilder* entity_data_builder,
      std::vector<ActionSuggestion>* result) const {
    const RulesModel_::GrammarRules_::RuleMatch* rule_match =
        grammar_rules_->rule_match()->Get(candidate.rule_id);
    if (rule_match == nullptr || rule_match->action_id() == nullptr) {
      TC3_LOG(ERROR) << "No rule action defined.";
      return false;
    }

    // Gather active capturing matches.
    std::unordered_map<uint16, const grammar::Match*> capturing_matches;
    for (const grammar::MappingMatch* match :
         grammar::SelectAllOfType<grammar::MappingMatch>(
             candidate.match, grammar::Match::kMappingMatch)) {
      capturing_matches[match->id] = match;
    }

    // Instantiate actions from the rule match.
    for (const uint16 action_id : *rule_match->action_id()) {
      const RulesModel_::RuleActionSpec* action_spec =
          grammar_rules_->actions()->Get(action_id);
      std::vector<ActionSuggestionAnnotation> annotations;

      std::unique_ptr<ReflectiveFlatbuffer> entity_data =
          entity_data_builder != nullptr ? entity_data_builder->NewRoot()
                                         : nullptr;

      // Set information from capturing matches.
      if (action_spec->capturing_group() != nullptr) {
        for (const RulesModel_::RuleActionSpec_::RuleCapturingGroup* group :
             *action_spec->capturing_group()) {
          auto it = capturing_matches.find(group->group_id());
          if (it == capturing_matches.end()) {
            // Capturing match is not active, skip.
            continue;
          }

          const grammar::Match* capturing_match = it->second;
          StringPiece match_text = StringPiece(
              message_codepoint_offsets[capturing_match->codepoint_span.first]
                  .utf8_data(),
              message_codepoint_offsets[capturing_match->codepoint_span.second]
                      .utf8_data() -
                  message_codepoint_offsets[capturing_match->codepoint_span
                                                .first]
                      .utf8_data());
          UnicodeText normalized_match_text =
              NormalizeMatchText(unilib_, group, match_text);

          if (!MergeEntityDataFromCapturingMatch(
                  group, normalized_match_text.ToUTF8String(),
                  entity_data.get())) {
            TC3_LOG(ERROR)
                << "Could not merge entity data from a capturing match.";
            return false;
          }

          // Add smart reply suggestions.
          SuggestTextRepliesFromCapturingMatch(entity_data_builder, group,
                                               normalized_match_text,
                                               smart_reply_action_type, result);

          // Add annotation.
          ActionSuggestionAnnotation annotation;
          if (FillAnnotationFromCapturingMatch(
                  /*span=*/capturing_match->codepoint_span, group,
                  /*message_index=*/message_index, match_text, &annotation)) {
            if (group->use_annotation_match()) {
              const grammar::AnnotationMatch* annotation_match =
                  grammar::SelectFirstOfType<grammar::AnnotationMatch>(
                      capturing_match, grammar::Match::kAnnotationMatch);
              if (!annotation_match) {
                TC3_LOG(ERROR) << "Could not get annotation for match.";
                return false;
              }
              annotation.entity = *annotation_match->annotation;
            }
            annotations.push_back(std::move(annotation));
          }
        }
      }

      if (action_spec->action() != nullptr) {
        ActionSuggestion suggestion;
        suggestion.annotations = annotations;
        FillSuggestionFromSpec(action_spec->action(), entity_data.get(),
                               &suggestion);
        result->push_back(std::move(suggestion));
      }
    }
    return true;
  }

  const UniLib& unilib_;
  const RulesModel_::GrammarRules* grammar_rules_;

  // All action rule match candidates.
  // Grammar rule matches are recorded, deduplicated, verified and then
  // instantiated.
  std::vector<grammar::Derivation> candidates_;
};
}  // namespace

GrammarActions::GrammarActions(
    const UniLib* unilib, const RulesModel_::GrammarRules* grammar_rules,
    const ReflectiveFlatbufferBuilder* entity_data_builder,
    const std::string& smart_reply_action_type)
    : unilib_(*unilib),
      grammar_rules_(grammar_rules),
      tokenizer_(CreateTokenizer(grammar_rules->tokenizer_options(), unilib)),
      lexer_(unilib, grammar_rules->rules()),
      entity_data_builder_(entity_data_builder),
      smart_reply_action_type_(smart_reply_action_type),
      rules_locales_(ParseRulesLocales(grammar_rules->rules())) {}

bool GrammarActions::SuggestActions(
    const Conversation& conversation,
    std::vector<ActionSuggestion>* result) const {
  if (grammar_rules_->rules()->rules() == nullptr) {
    // Nothing to do.
    return true;
  }

  std::vector<Locale> locales;
  if (!ParseLocales(conversation.messages.back().detected_text_language_tags,
                    &locales)) {
    TC3_LOG(ERROR) << "Could not parse locales of input text.";
    return false;
  }

  // Select locale matching rules.
  std::vector<const grammar::RulesSet_::Rules*> locale_rules =
      SelectLocaleMatchingShards(grammar_rules_->rules(), rules_locales_,
                                 locales);
  if (locale_rules.empty()) {
    // Nothing to do.
    return true;
  }

  GrammarActionsCallbackDelegate callback_handler(&unilib_, grammar_rules_);
  grammar::Matcher matcher(&unilib_, grammar_rules_->rules(), locale_rules,
                           &callback_handler);

  const UnicodeText text =
      UTF8ToUnicodeText(conversation.messages.back().text, /*do_copy=*/false);

  // Run grammar on last message.
  lexer_.Process(text, tokenizer_->Tokenize(text),
                 /*annotations=*/&conversation.messages.back().annotations,
                 &matcher);

  // Populate results.
  return callback_handler.GetActions(conversation, smart_reply_action_type_,
                                     entity_data_builder_, result);
}

}  // namespace libtextclassifier3
