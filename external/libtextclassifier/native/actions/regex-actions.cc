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

#include "actions/regex-actions.h"

#include "actions/utils.h"
#include "utils/base/logging.h"
#include "utils/regex-match.h"
#include "utils/utf8/unicodetext.h"
#include "utils/zlib/zlib_regex.h"

namespace libtextclassifier3 {
namespace {

// Creates an annotation from a regex capturing group.
bool FillAnnotationFromMatchGroup(
    const UniLib::RegexMatcher* matcher,
    const RulesModel_::RuleActionSpec_::RuleCapturingGroup* group,
    const std::string& group_match_text, const int message_index,
    ActionSuggestionAnnotation* annotation) {
  if (group->annotation_name() != nullptr ||
      group->annotation_type() != nullptr) {
    int status = UniLib::RegexMatcher::kNoError;
    const CodepointSpan span = {matcher->Start(group->group_id(), &status),
                                matcher->End(group->group_id(), &status)};
    if (status != UniLib::RegexMatcher::kNoError) {
      TC3_LOG(ERROR) << "Could not extract span from rule capturing group.";
      return false;
    }
    return FillAnnotationFromCapturingMatch(span, group, message_index,
                                            group_match_text, annotation);
  }
  return true;
}

}  // namespace

bool RegexActions::InitializeRules(
    const RulesModel* rules, const RulesModel* low_confidence_rules,
    const TriggeringPreconditions* triggering_preconditions_overlay,
    ZlibDecompressor* decompressor) {
  if (rules != nullptr) {
    if (!InitializeRulesModel(rules, decompressor, &rules_)) {
      TC3_LOG(ERROR) << "Could not initialize action rules.";
      return false;
    }
  }

  if (low_confidence_rules != nullptr) {
    if (!InitializeRulesModel(low_confidence_rules, decompressor,
                              &low_confidence_rules_)) {
      TC3_LOG(ERROR) << "Could not initialize low confidence rules.";
      return false;
    }
  }

  // Extend by rules provided by the overwrite.
  // NOTE: The rules from the original models are *not* cleared.
  if (triggering_preconditions_overlay != nullptr &&
      triggering_preconditions_overlay->low_confidence_rules() != nullptr) {
    // These rules are optionally compressed, but separately.
    std::unique_ptr<ZlibDecompressor> overwrite_decompressor =
        ZlibDecompressor::Instance();
    if (overwrite_decompressor == nullptr) {
      TC3_LOG(ERROR) << "Could not initialze decompressor for overwrite rules.";
      return false;
    }
    if (!InitializeRulesModel(
            triggering_preconditions_overlay->low_confidence_rules(),
            overwrite_decompressor.get(), &low_confidence_rules_)) {
      TC3_LOG(ERROR)
          << "Could not initialize low confidence rules from overwrite.";
      return false;
    }
  }

  return true;
}

bool RegexActions::InitializeRulesModel(
    const RulesModel* rules, ZlibDecompressor* decompressor,
    std::vector<CompiledRule>* compiled_rules) const {
  for (const RulesModel_::RegexRule* rule : *rules->regex_rule()) {
    std::unique_ptr<UniLib::RegexPattern> compiled_pattern =
        UncompressMakeRegexPattern(
            unilib_, rule->pattern(), rule->compressed_pattern(),
            rules->lazy_regex_compilation(), decompressor);
    if (compiled_pattern == nullptr) {
      TC3_LOG(ERROR) << "Failed to load rule pattern.";
      return false;
    }

    // Check whether there is a check on the output.
    std::unique_ptr<UniLib::RegexPattern> compiled_output_pattern;
    if (rule->output_pattern() != nullptr ||
        rule->compressed_output_pattern() != nullptr) {
      compiled_output_pattern = UncompressMakeRegexPattern(
          unilib_, rule->output_pattern(), rule->compressed_output_pattern(),
          rules->lazy_regex_compilation(), decompressor);
      if (compiled_output_pattern == nullptr) {
        TC3_LOG(ERROR) << "Failed to load rule output pattern.";
        return false;
      }
    }

    compiled_rules->emplace_back(rule, std::move(compiled_pattern),
                                 std::move(compiled_output_pattern));
  }

  return true;
}

bool RegexActions::IsLowConfidenceInput(
    const Conversation& conversation, const int num_messages,
    std::vector<const UniLib::RegexPattern*>* post_check_rules) const {
  for (int i = 1; i <= num_messages; i++) {
    const std::string& message =
        conversation.messages[conversation.messages.size() - i].text;
    const UnicodeText message_unicode(
        UTF8ToUnicodeText(message, /*do_copy=*/false));
    for (int low_confidence_rule = 0;
         low_confidence_rule < low_confidence_rules_.size();
         low_confidence_rule++) {
      const CompiledRule& rule = low_confidence_rules_[low_confidence_rule];
      const std::unique_ptr<UniLib::RegexMatcher> matcher =
          rule.pattern->Matcher(message_unicode);
      int status = UniLib::RegexMatcher::kNoError;
      if (matcher->Find(&status) && status == UniLib::RegexMatcher::kNoError) {
        // Rule only applies to input-output pairs, so defer the check.
        if (rule.output_pattern != nullptr) {
          post_check_rules->push_back(rule.output_pattern.get());
          continue;
        }
        return true;
      }
    }
  }
  return false;
}

bool RegexActions::FilterConfidenceOutput(
    const std::vector<const UniLib::RegexPattern*>& post_check_rules,
    std::vector<ActionSuggestion>* actions) const {
  if (post_check_rules.empty() || actions->empty()) {
    return true;
  }
  std::vector<ActionSuggestion> filtered_text_replies;
  for (const ActionSuggestion& action : *actions) {
    if (action.response_text.empty()) {
      filtered_text_replies.push_back(action);
      continue;
    }
    bool passes_post_check = true;
    const UnicodeText text_reply_unicode(
        UTF8ToUnicodeText(action.response_text, /*do_copy=*/false));
    for (const UniLib::RegexPattern* post_check_rule : post_check_rules) {
      const std::unique_ptr<UniLib::RegexMatcher> matcher =
          post_check_rule->Matcher(text_reply_unicode);
      if (matcher == nullptr) {
        TC3_LOG(ERROR) << "Could not create matcher for post check rule.";
        return false;
      }
      int status = UniLib::RegexMatcher::kNoError;
      if (matcher->Find(&status) || status != UniLib::RegexMatcher::kNoError) {
        passes_post_check = false;
        break;
      }
    }
    if (passes_post_check) {
      filtered_text_replies.push_back(action);
    }
  }
  *actions = std::move(filtered_text_replies);
  return true;
}

bool RegexActions::SuggestActions(
    const Conversation& conversation,
    const ReflectiveFlatbufferBuilder* entity_data_builder,
    std::vector<ActionSuggestion>* actions) const {
  // Create actions based on rules checking the last message.
  const int message_index = conversation.messages.size() - 1;
  const std::string& message = conversation.messages.back().text;
  const UnicodeText message_unicode(
      UTF8ToUnicodeText(message, /*do_copy=*/false));
  for (const CompiledRule& rule : rules_) {
    const std::unique_ptr<UniLib::RegexMatcher> matcher =
        rule.pattern->Matcher(message_unicode);
    int status = UniLib::RegexMatcher::kNoError;
    while (matcher->Find(&status) && status == UniLib::RegexMatcher::kNoError) {
      for (const RulesModel_::RuleActionSpec* rule_action :
           *rule.rule->actions()) {
        const ActionSuggestionSpec* action = rule_action->action();
        std::vector<ActionSuggestionAnnotation> annotations;

        std::unique_ptr<ReflectiveFlatbuffer> entity_data =
            entity_data_builder != nullptr ? entity_data_builder->NewRoot()
                                           : nullptr;

        // Add entity data from rule capturing groups.
        if (rule_action->capturing_group() != nullptr) {
          for (const RulesModel_::RuleActionSpec_::RuleCapturingGroup* group :
               *rule_action->capturing_group()) {
            Optional<std::string> group_match_text =
                GetCapturingGroupText(matcher.get(), group->group_id());
            if (!group_match_text.has_value()) {
              // The group was not part of the match, ignore and continue.
              continue;
            }

            UnicodeText normalized_group_match_text =
                NormalizeMatchText(unilib_, group, group_match_text.value());

            if (!MergeEntityDataFromCapturingMatch(
                    group, normalized_group_match_text.ToUTF8String(),
                    entity_data.get())) {
              TC3_LOG(ERROR)
                  << "Could not merge entity data from a capturing match.";
              return false;
            }

            // Create a text annotation for the group span.
            ActionSuggestionAnnotation annotation;
            if (FillAnnotationFromMatchGroup(matcher.get(), group,
                                             group_match_text.value(),
                                             message_index, &annotation)) {
              annotations.push_back(annotation);
            }

            // Create text reply.
            SuggestTextRepliesFromCapturingMatch(
                entity_data_builder, group, normalized_group_match_text,
                smart_reply_action_type_, actions);
          }
        }

        if (action != nullptr) {
          ActionSuggestion suggestion;
          suggestion.annotations = annotations;
          FillSuggestionFromSpec(action, entity_data.get(), &suggestion);
          actions->push_back(suggestion);
        }
      }
    }
  }
  return true;
}

}  // namespace libtextclassifier3
