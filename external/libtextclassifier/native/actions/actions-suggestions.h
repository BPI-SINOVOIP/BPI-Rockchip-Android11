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

#ifndef LIBTEXTCLASSIFIER_ACTIONS_ACTIONS_SUGGESTIONS_H_
#define LIBTEXTCLASSIFIER_ACTIONS_ACTIONS_SUGGESTIONS_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "actions/actions_model_generated.h"
#include "actions/feature-processor.h"
#include "actions/grammar-actions.h"
#include "actions/ngram-model.h"
#include "actions/ranker.h"
#include "actions/regex-actions.h"
#include "actions/types.h"
#include "annotator/annotator.h"
#include "annotator/model-executor.h"
#include "annotator/types.h"
#include "utils/flatbuffers.h"
#include "utils/i18n/locale.h"
#include "utils/memory/mmap.h"
#include "utils/tflite-model-executor.h"
#include "utils/utf8/unilib.h"
#include "utils/variant.h"
#include "utils/zlib/zlib.h"

namespace libtextclassifier3 {

// Options for suggesting actions.
struct ActionSuggestionOptions {
  static ActionSuggestionOptions Default() { return ActionSuggestionOptions(); }
  std::unordered_map<std::string, Variant> model_parameters;
};

// Class for predicting actions following a conversation.
class ActionsSuggestions {
 public:
  // Creates ActionsSuggestions from given data buffer with model.
  static std::unique_ptr<ActionsSuggestions> FromUnownedBuffer(
      const uint8_t* buffer, const int size, const UniLib* unilib = nullptr,
      const std::string& triggering_preconditions_overlay = "");

  // Creates ActionsSuggestions from model in the ScopedMmap object and takes
  // ownership of it.
  static std::unique_ptr<ActionsSuggestions> FromScopedMmap(
      std::unique_ptr<libtextclassifier3::ScopedMmap> mmap,
      const UniLib* unilib = nullptr,
      const std::string& triggering_preconditions_overlay = "");
  // Same as above, but also takes ownership of the unilib.
  static std::unique_ptr<ActionsSuggestions> FromScopedMmap(
      std::unique_ptr<libtextclassifier3::ScopedMmap> mmap,
      std::unique_ptr<UniLib> unilib,
      const std::string& triggering_preconditions_overlay);

  // Creates ActionsSuggestions from model given as a file descriptor, offset
  // and size in it. If offset and size are less than 0, will ignore them and
  // will just use the fd.
  static std::unique_ptr<ActionsSuggestions> FromFileDescriptor(
      const int fd, const int offset, const int size,
      const UniLib* unilib = nullptr,
      const std::string& triggering_preconditions_overlay = "");
  // Same as above, but also takes ownership of the unilib.
  static std::unique_ptr<ActionsSuggestions> FromFileDescriptor(
      const int fd, const int offset, const int size,
      std::unique_ptr<UniLib> unilib,
      const std::string& triggering_preconditions_overlay = "");

  // Creates ActionsSuggestions from model given as a file descriptor.
  static std::unique_ptr<ActionsSuggestions> FromFileDescriptor(
      const int fd, const UniLib* unilib = nullptr,
      const std::string& triggering_preconditions_overlay = "");
  // Same as above, but also takes ownership of the unilib.
  static std::unique_ptr<ActionsSuggestions> FromFileDescriptor(
      const int fd, std::unique_ptr<UniLib> unilib,
      const std::string& triggering_preconditions_overlay);

  // Creates ActionsSuggestions from model given as a POSIX path.
  static std::unique_ptr<ActionsSuggestions> FromPath(
      const std::string& path, const UniLib* unilib = nullptr,
      const std::string& triggering_preconditions_overlay = "");
  // Same as above, but also takes ownership of unilib.
  static std::unique_ptr<ActionsSuggestions> FromPath(
      const std::string& path, std::unique_ptr<UniLib> unilib,
      const std::string& triggering_preconditions_overlay);

  ActionsSuggestionsResponse SuggestActions(
      const Conversation& conversation,
      const ActionSuggestionOptions& options = ActionSuggestionOptions()) const;

  ActionsSuggestionsResponse SuggestActions(
      const Conversation& conversation, const Annotator* annotator,
      const ActionSuggestionOptions& options = ActionSuggestionOptions()) const;

  const ActionsModel* model() const;
  const reflection::Schema* entity_data_schema() const;

  static constexpr int kLocalUserId = 0;

  // Should be in sync with those defined in Android.
  // android/frameworks/base/core/java/android/view/textclassifier/ConversationActions.java
  static const std::string& kViewCalendarType;
  static const std::string& kViewMapType;
  static const std::string& kTrackFlightType;
  static const std::string& kOpenUrlType;
  static const std::string& kSendSmsType;
  static const std::string& kCallPhoneType;
  static const std::string& kSendEmailType;
  static const std::string& kShareLocation;

 protected:
  // Exposed for testing.
  bool EmbedTokenId(const int32 token_id, std::vector<float>* embedding) const;

  // Embeds the tokens per message separately. Each message is padded to the
  // maximum length with the padding token.
  bool EmbedTokensPerMessage(const std::vector<std::vector<Token>>& tokens,
                             std::vector<float>* embeddings,
                             int* max_num_tokens_per_message) const;

  // Concatenates the embedded message tokens - separated by start and end
  // token between messages.
  // If the total token count is greater than the maximum length, tokens at the
  // start are dropped to fit into the limit.
  // If the total token count is smaller than the minimum length, padding tokens
  // are added to the end.
  // Messages are assumed to be ordered by recency - most recent is last.
  bool EmbedAndFlattenTokens(const std::vector<std::vector<Token>>& tokens,
                             std::vector<float>* embeddings,
                             int* total_token_count) const;

  const ActionsModel* model_;

  // Feature extractor and options.
  std::unique_ptr<const ActionsFeatureProcessor> feature_processor_;
  std::unique_ptr<const EmbeddingExecutor> embedding_executor_;
  std::vector<float> embedded_padding_token_;
  std::vector<float> embedded_start_token_;
  std::vector<float> embedded_end_token_;
  int token_embedding_size_;

 private:
  // Checks that model contains all required fields, and initializes internal
  // datastructures.
  bool ValidateAndInitialize();

  void SetOrCreateUnilib(const UniLib* unilib);

  // Prepare preconditions.
  // Takes values from flag provided data, but falls back to model provided
  // values for parameters that are not explicitly provided.
  bool InitializeTriggeringPreconditions();

  // Tokenizes a conversation and produces the tokens per message.
  std::vector<std::vector<Token>> Tokenize(
      const std::vector<std::string>& context) const;

  bool AllocateInput(const int conversation_length, const int max_tokens,
                     const int total_token_count,
                     tflite::Interpreter* interpreter) const;

  bool SetupModelInput(const std::vector<std::string>& context,
                       const std::vector<int>& user_ids,
                       const std::vector<float>& time_diffs,
                       const int num_suggestions,
                       const ActionSuggestionOptions& options,
                       tflite::Interpreter* interpreter) const;

  void FillSuggestionFromSpecWithEntityData(const ActionSuggestionSpec* spec,
                                            ActionSuggestion* suggestion) const;

  void PopulateTextReplies(const tflite::Interpreter* interpreter,
                           int suggestion_index, int score_index,
                           const std::string& type,
                           ActionsSuggestionsResponse* response) const;

  void PopulateIntentTriggering(const tflite::Interpreter* interpreter,
                                int suggestion_index, int score_index,
                                const ActionSuggestionSpec* task_spec,
                                ActionsSuggestionsResponse* response) const;

  bool ReadModelOutput(tflite::Interpreter* interpreter,
                       const ActionSuggestionOptions& options,
                       ActionsSuggestionsResponse* response) const;

  bool SuggestActionsFromModel(
      const Conversation& conversation, const int num_messages,
      const ActionSuggestionOptions& options,
      ActionsSuggestionsResponse* response,
      std::unique_ptr<tflite::Interpreter>* interpreter) const;

  // Creates options for annotation of a message.
  AnnotationOptions AnnotationOptionsForMessage(
      const ConversationMessage& message) const;

  void SuggestActionsFromAnnotations(
      const Conversation& conversation,
      std::vector<ActionSuggestion>* actions) const;

  void SuggestActionsFromAnnotation(
      const int message_index, const ActionSuggestionAnnotation& annotation,
      std::vector<ActionSuggestion>* actions) const;

  // Run annotator on the messages of a conversation.
  Conversation AnnotateConversation(const Conversation& conversation,
                                    const Annotator* annotator) const;

  // Deduplicates equivalent annotations - annotations that have the same type
  // and same span text.
  // Returns the indices of the deduplicated annotations.
  std::vector<int> DeduplicateAnnotations(
      const std::vector<ActionSuggestionAnnotation>& annotations) const;

  bool SuggestActionsFromLua(
      const Conversation& conversation,
      const TfLiteModelExecutor* model_executor,
      const tflite::Interpreter* interpreter,
      const reflection::Schema* annotation_entity_data_schema,
      std::vector<ActionSuggestion>* actions) const;

  bool GatherActionsSuggestions(const Conversation& conversation,
                                const Annotator* annotator,
                                const ActionSuggestionOptions& options,
                                ActionsSuggestionsResponse* response) const;

  std::unique_ptr<libtextclassifier3::ScopedMmap> mmap_;

  // Tensorflow Lite models.
  std::unique_ptr<const TfLiteModelExecutor> model_executor_;

  // Regex rules model.
  std::unique_ptr<RegexActions> regex_actions_;

  // The grammar rules model.
  std::unique_ptr<GrammarActions> grammar_actions_;

  std::unique_ptr<UniLib> owned_unilib_;
  const UniLib* unilib_;

  // Locales supported by the model.
  std::vector<Locale> locales_;

  // Annotation entities used by the model.
  std::unordered_set<std::string> annotation_entity_types_;

  // Builder for creating extra data.
  const reflection::Schema* entity_data_schema_;
  std::unique_ptr<ReflectiveFlatbufferBuilder> entity_data_builder_;
  std::unique_ptr<ActionsSuggestionsRanker> ranker_;

  std::string lua_bytecode_;

  // Triggering preconditions. These parameters can be backed by the model and
  // (partially) be provided by flags.
  TriggeringPreconditionsT preconditions_;
  std::string triggering_preconditions_overlay_buffer_;
  const TriggeringPreconditions* triggering_preconditions_overlay_;

  // Low confidence input ngram classifier.
  std::unique_ptr<const NGramModel> ngram_model_;
};

// Interprets the buffer as a Model flatbuffer and returns it for reading.
const ActionsModel* ViewActionsModel(const void* buffer, int size);

// Opens model from given path and runs a function, passing the loaded Model
// flatbuffer as an argument.
//
// This is mainly useful if we don't want to pay the cost for the model
// initialization because we'll be only reading some flatbuffer values from the
// file.
template <typename ReturnType, typename Func>
ReturnType VisitActionsModel(const std::string& path, Func function) {
  ScopedMmap mmap(path);
  if (!mmap.handle().ok()) {
    function(/*model=*/nullptr);
  }
  const ActionsModel* model =
      ViewActionsModel(mmap.handle().start(), mmap.handle().num_bytes());
  return function(model);
}

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ACTIONS_ACTIONS_SUGGESTIONS_H_
