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

#ifndef LIBTEXTCLASSIFIER_ACTIONS_REGEX_ACTIONS_H_
#define LIBTEXTCLASSIFIER_ACTIONS_REGEX_ACTIONS_H_

#include <memory>
#include <string>
#include <vector>

#include "actions/actions_model_generated.h"
#include "actions/types.h"
#include "utils/flatbuffers.h"
#include "utils/utf8/unilib.h"
#include "utils/zlib/zlib.h"

namespace libtextclassifier3 {

// Regular expression backed actions suggestions.
class RegexActions {
 public:
  explicit RegexActions(const UniLib* unilib,
                        const std::string& smart_reply_action_type)
      : unilib_(*unilib), smart_reply_action_type_(smart_reply_action_type) {}

  // Decompresses and initializes all rules in a model.
  bool InitializeRules(
      const RulesModel* rules, const RulesModel* low_confidence_rules,
      const TriggeringPreconditions* triggering_preconditions_overlay,
      ZlibDecompressor* decompressor);

  // Checks whether the input triggers the low confidence rules.
  bool IsLowConfidenceInput(
      const Conversation& conversation, const int num_messages,
      std::vector<const UniLib::RegexPattern*>* post_check_rules) const;

  // Checks and filters suggestions triggering the low confidence post checks.
  bool FilterConfidenceOutput(
      const std::vector<const UniLib::RegexPattern*>& post_check_rules,
      std::vector<ActionSuggestion>* actions) const;

  // Suggests actions for a conversation from a message stream using the regex
  // rules.
  bool SuggestActions(const Conversation& conversation,
                      const ReflectiveFlatbufferBuilder* entity_data_builder,
                      std::vector<ActionSuggestion>* actions) const;

 private:
  struct CompiledRule {
    const RulesModel_::RegexRule* rule;
    std::unique_ptr<UniLib::RegexPattern> pattern;
    std::unique_ptr<UniLib::RegexPattern> output_pattern;
    CompiledRule(const RulesModel_::RegexRule* rule,
                 std::unique_ptr<UniLib::RegexPattern> pattern,
                 std::unique_ptr<UniLib::RegexPattern> output_pattern)
        : rule(rule),
          pattern(std::move(pattern)),
          output_pattern(std::move(output_pattern)) {}
  };

  // Decompresses and initializes a set of regular expression rules.
  bool InitializeRulesModel(const RulesModel* rules,
                            ZlibDecompressor* decompressor,
                            std::vector<CompiledRule>* compiled_rules) const;

  const UniLib& unilib_;
  const std::string smart_reply_action_type_;
  std::vector<CompiledRule> rules_, low_confidence_rules_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ACTIONS_REGEX_ACTIONS_H_
