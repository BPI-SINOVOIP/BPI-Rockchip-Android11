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

#include "actions/actions-suggestions.h"

#include <memory>

#include "actions/lua-actions.h"
#include "actions/types.h"
#include "actions/utils.h"
#include "actions/zlib-utils.h"
#include "annotator/collections.h"
#include "utils/base/logging.h"
#include "utils/flatbuffers.h"
#include "utils/lua-utils.h"
#include "utils/normalization.h"
#include "utils/optional.h"
#include "utils/strings/split.h"
#include "utils/strings/stringpiece.h"
#include "utils/utf8/unicodetext.h"
#include "tensorflow/lite/string_util.h"

namespace libtextclassifier3 {

const std::string& ActionsSuggestions::kViewCalendarType =
    *[]() { return new std::string("view_calendar"); }();
const std::string& ActionsSuggestions::kViewMapType =
    *[]() { return new std::string("view_map"); }();
const std::string& ActionsSuggestions::kTrackFlightType =
    *[]() { return new std::string("track_flight"); }();
const std::string& ActionsSuggestions::kOpenUrlType =
    *[]() { return new std::string("open_url"); }();
const std::string& ActionsSuggestions::kSendSmsType =
    *[]() { return new std::string("send_sms"); }();
const std::string& ActionsSuggestions::kCallPhoneType =
    *[]() { return new std::string("call_phone"); }();
const std::string& ActionsSuggestions::kSendEmailType =
    *[]() { return new std::string("send_email"); }();
const std::string& ActionsSuggestions::kShareLocation =
    *[]() { return new std::string("share_location"); }();

// Name for a datetime annotation that only includes time but no date.
const std::string& kTimeAnnotation =
    *[]() { return new std::string("time"); }();

constexpr float kDefaultFloat = 0.0;
constexpr bool kDefaultBool = false;
constexpr int kDefaultInt = 1;

namespace {

const ActionsModel* LoadAndVerifyModel(const uint8_t* addr, int size) {
  flatbuffers::Verifier verifier(addr, size);
  if (VerifyActionsModelBuffer(verifier)) {
    return GetActionsModel(addr);
  } else {
    return nullptr;
  }
}

template <typename T>
T ValueOrDefault(const flatbuffers::Table* values, const int32 field_offset,
                 const T default_value) {
  if (values == nullptr) {
    return default_value;
  }
  return values->GetField<T>(field_offset, default_value);
}

// Returns number of (tail) messages of a conversation to consider.
int NumMessagesToConsider(const Conversation& conversation,
                          const int max_conversation_history_length) {
  return ((max_conversation_history_length < 0 ||
           conversation.messages.size() < max_conversation_history_length)
              ? conversation.messages.size()
              : max_conversation_history_length);
}

}  // namespace

std::unique_ptr<ActionsSuggestions> ActionsSuggestions::FromUnownedBuffer(
    const uint8_t* buffer, const int size, const UniLib* unilib,
    const std::string& triggering_preconditions_overlay) {
  auto actions = std::unique_ptr<ActionsSuggestions>(new ActionsSuggestions());
  const ActionsModel* model = LoadAndVerifyModel(buffer, size);
  if (model == nullptr) {
    return nullptr;
  }
  actions->model_ = model;
  actions->SetOrCreateUnilib(unilib);
  actions->triggering_preconditions_overlay_buffer_ =
      triggering_preconditions_overlay;
  if (!actions->ValidateAndInitialize()) {
    return nullptr;
  }
  return actions;
}

std::unique_ptr<ActionsSuggestions> ActionsSuggestions::FromScopedMmap(
    std::unique_ptr<libtextclassifier3::ScopedMmap> mmap, const UniLib* unilib,
    const std::string& triggering_preconditions_overlay) {
  if (!mmap->handle().ok()) {
    TC3_VLOG(1) << "Mmap failed.";
    return nullptr;
  }
  const ActionsModel* model = LoadAndVerifyModel(
      reinterpret_cast<const uint8_t*>(mmap->handle().start()),
      mmap->handle().num_bytes());
  if (!model) {
    TC3_LOG(ERROR) << "Model verification failed.";
    return nullptr;
  }
  auto actions = std::unique_ptr<ActionsSuggestions>(new ActionsSuggestions());
  actions->model_ = model;
  actions->mmap_ = std::move(mmap);
  actions->SetOrCreateUnilib(unilib);
  actions->triggering_preconditions_overlay_buffer_ =
      triggering_preconditions_overlay;
  if (!actions->ValidateAndInitialize()) {
    return nullptr;
  }
  return actions;
}

std::unique_ptr<ActionsSuggestions> ActionsSuggestions::FromScopedMmap(
    std::unique_ptr<libtextclassifier3::ScopedMmap> mmap,
    std::unique_ptr<UniLib> unilib,
    const std::string& triggering_preconditions_overlay) {
  if (!mmap->handle().ok()) {
    TC3_VLOG(1) << "Mmap failed.";
    return nullptr;
  }
  const ActionsModel* model = LoadAndVerifyModel(
      reinterpret_cast<const uint8_t*>(mmap->handle().start()),
      mmap->handle().num_bytes());
  if (!model) {
    TC3_LOG(ERROR) << "Model verification failed.";
    return nullptr;
  }
  auto actions = std::unique_ptr<ActionsSuggestions>(new ActionsSuggestions());
  actions->model_ = model;
  actions->mmap_ = std::move(mmap);
  actions->owned_unilib_ = std::move(unilib);
  actions->unilib_ = actions->owned_unilib_.get();
  actions->triggering_preconditions_overlay_buffer_ =
      triggering_preconditions_overlay;
  if (!actions->ValidateAndInitialize()) {
    return nullptr;
  }
  return actions;
}

std::unique_ptr<ActionsSuggestions> ActionsSuggestions::FromFileDescriptor(
    const int fd, const int offset, const int size, const UniLib* unilib,
    const std::string& triggering_preconditions_overlay) {
  std::unique_ptr<libtextclassifier3::ScopedMmap> mmap;
  if (offset >= 0 && size >= 0) {
    mmap.reset(new libtextclassifier3::ScopedMmap(fd, offset, size));
  } else {
    mmap.reset(new libtextclassifier3::ScopedMmap(fd));
  }
  return FromScopedMmap(std::move(mmap), unilib,
                        triggering_preconditions_overlay);
}

std::unique_ptr<ActionsSuggestions> ActionsSuggestions::FromFileDescriptor(
    const int fd, const int offset, const int size,
    std::unique_ptr<UniLib> unilib,
    const std::string& triggering_preconditions_overlay) {
  std::unique_ptr<libtextclassifier3::ScopedMmap> mmap;
  if (offset >= 0 && size >= 0) {
    mmap.reset(new libtextclassifier3::ScopedMmap(fd, offset, size));
  } else {
    mmap.reset(new libtextclassifier3::ScopedMmap(fd));
  }
  return FromScopedMmap(std::move(mmap), std::move(unilib),
                        triggering_preconditions_overlay);
}

std::unique_ptr<ActionsSuggestions> ActionsSuggestions::FromFileDescriptor(
    const int fd, const UniLib* unilib,
    const std::string& triggering_preconditions_overlay) {
  std::unique_ptr<libtextclassifier3::ScopedMmap> mmap(
      new libtextclassifier3::ScopedMmap(fd));
  return FromScopedMmap(std::move(mmap), unilib,
                        triggering_preconditions_overlay);
}

std::unique_ptr<ActionsSuggestions> ActionsSuggestions::FromFileDescriptor(
    const int fd, std::unique_ptr<UniLib> unilib,
    const std::string& triggering_preconditions_overlay) {
  std::unique_ptr<libtextclassifier3::ScopedMmap> mmap(
      new libtextclassifier3::ScopedMmap(fd));
  return FromScopedMmap(std::move(mmap), std::move(unilib),
                        triggering_preconditions_overlay);
}

std::unique_ptr<ActionsSuggestions> ActionsSuggestions::FromPath(
    const std::string& path, const UniLib* unilib,
    const std::string& triggering_preconditions_overlay) {
  std::unique_ptr<libtextclassifier3::ScopedMmap> mmap(
      new libtextclassifier3::ScopedMmap(path));
  return FromScopedMmap(std::move(mmap), unilib,
                        triggering_preconditions_overlay);
}

std::unique_ptr<ActionsSuggestions> ActionsSuggestions::FromPath(
    const std::string& path, std::unique_ptr<UniLib> unilib,
    const std::string& triggering_preconditions_overlay) {
  std::unique_ptr<libtextclassifier3::ScopedMmap> mmap(
      new libtextclassifier3::ScopedMmap(path));
  return FromScopedMmap(std::move(mmap), std::move(unilib),
                        triggering_preconditions_overlay);
}

void ActionsSuggestions::SetOrCreateUnilib(const UniLib* unilib) {
  if (unilib != nullptr) {
    unilib_ = unilib;
  } else {
    owned_unilib_.reset(new UniLib);
    unilib_ = owned_unilib_.get();
  }
}

bool ActionsSuggestions::ValidateAndInitialize() {
  if (model_ == nullptr) {
    TC3_LOG(ERROR) << "No model specified.";
    return false;
  }

  if (model_->smart_reply_action_type() == nullptr) {
    TC3_LOG(ERROR) << "No smart reply action type specified.";
    return false;
  }

  if (!InitializeTriggeringPreconditions()) {
    TC3_LOG(ERROR) << "Could not initialize preconditions.";
    return false;
  }

  if (model_->locales() &&
      !ParseLocales(model_->locales()->c_str(), &locales_)) {
    TC3_LOG(ERROR) << "Could not parse model supported locales.";
    return false;
  }

  if (model_->tflite_model_spec() != nullptr) {
    model_executor_ = TfLiteModelExecutor::FromBuffer(
        model_->tflite_model_spec()->tflite_model());
    if (!model_executor_) {
      TC3_LOG(ERROR) << "Could not initialize model executor.";
      return false;
    }
  }

  // Gather annotation entities for the rules.
  if (model_->annotation_actions_spec() != nullptr &&
      model_->annotation_actions_spec()->annotation_mapping() != nullptr) {
    for (const AnnotationActionsSpec_::AnnotationMapping* mapping :
         *model_->annotation_actions_spec()->annotation_mapping()) {
      annotation_entity_types_.insert(mapping->annotation_collection()->str());
    }
  }

  if (model_->actions_entity_data_schema() != nullptr) {
    entity_data_schema_ = LoadAndVerifyFlatbuffer<reflection::Schema>(
        model_->actions_entity_data_schema()->Data(),
        model_->actions_entity_data_schema()->size());
    if (entity_data_schema_ == nullptr) {
      TC3_LOG(ERROR) << "Could not load entity data schema data.";
      return false;
    }

    entity_data_builder_.reset(
        new ReflectiveFlatbufferBuilder(entity_data_schema_));
  } else {
    entity_data_schema_ = nullptr;
  }

  // Initialize regular expressions model.
  std::unique_ptr<ZlibDecompressor> decompressor = ZlibDecompressor::Instance();
  regex_actions_.reset(
      new RegexActions(unilib_, model_->smart_reply_action_type()->str()));
  if (!regex_actions_->InitializeRules(
          model_->rules(), model_->low_confidence_rules(),
          triggering_preconditions_overlay_, decompressor.get())) {
    TC3_LOG(ERROR) << "Could not initialize regex rules.";
    return false;
  }

  // Setup grammar model.
  if (model_->rules() != nullptr &&
      model_->rules()->grammar_rules() != nullptr) {
    grammar_actions_.reset(new GrammarActions(
        unilib_, model_->rules()->grammar_rules(), entity_data_builder_.get(),
        model_->smart_reply_action_type()->str()));

    // Gather annotation entities for the grammars.
    if (auto annotation_nt = model_->rules()
                                 ->grammar_rules()
                                 ->rules()
                                 ->nonterminals()
                                 ->annotation_nt()) {
      for (const grammar::RulesSet_::Nonterminals_::AnnotationNtEntry* entry :
           *annotation_nt) {
        annotation_entity_types_.insert(entry->key()->str());
      }
    }
  }

  std::string actions_script;
  if (GetUncompressedString(model_->lua_actions_script(),
                            model_->compressed_lua_actions_script(),
                            decompressor.get(), &actions_script) &&
      !actions_script.empty()) {
    if (!Compile(actions_script, &lua_bytecode_)) {
      TC3_LOG(ERROR) << "Could not precompile lua actions snippet.";
      return false;
    }
  }

  if (!(ranker_ = ActionsSuggestionsRanker::CreateActionsSuggestionsRanker(
            model_->ranking_options(), decompressor.get(),
            model_->smart_reply_action_type()->str()))) {
    TC3_LOG(ERROR) << "Could not create an action suggestions ranker.";
    return false;
  }

  // Create feature processor if specified.
  const ActionsTokenFeatureProcessorOptions* options =
      model_->feature_processor_options();
  if (options != nullptr) {
    if (options->tokenizer_options() == nullptr) {
      TC3_LOG(ERROR) << "No tokenizer options specified.";
      return false;
    }

    feature_processor_.reset(new ActionsFeatureProcessor(options, unilib_));
    embedding_executor_ = TFLiteEmbeddingExecutor::FromBuffer(
        options->embedding_model(), options->embedding_size(),
        options->embedding_quantization_bits());

    if (embedding_executor_ == nullptr) {
      TC3_LOG(ERROR) << "Could not initialize embedding executor.";
      return false;
    }

    // Cache embedding of padding, start and end token.
    if (!EmbedTokenId(options->padding_token_id(), &embedded_padding_token_) ||
        !EmbedTokenId(options->start_token_id(), &embedded_start_token_) ||
        !EmbedTokenId(options->end_token_id(), &embedded_end_token_)) {
      TC3_LOG(ERROR) << "Could not precompute token embeddings.";
      return false;
    }
    token_embedding_size_ = feature_processor_->GetTokenEmbeddingSize();
  }

  // Create low confidence model if specified.
  if (model_->low_confidence_ngram_model() != nullptr) {
    ngram_model_ = NGramModel::Create(
        unilib_, model_->low_confidence_ngram_model(),
        feature_processor_ == nullptr ? nullptr
                                      : feature_processor_->tokenizer());
    if (ngram_model_ == nullptr) {
      TC3_LOG(ERROR) << "Could not create ngram linear regression model.";
      return false;
    }
  }

  return true;
}

bool ActionsSuggestions::InitializeTriggeringPreconditions() {
  triggering_preconditions_overlay_ =
      LoadAndVerifyFlatbuffer<TriggeringPreconditions>(
          triggering_preconditions_overlay_buffer_);

  if (triggering_preconditions_overlay_ == nullptr &&
      !triggering_preconditions_overlay_buffer_.empty()) {
    TC3_LOG(ERROR) << "Could not load triggering preconditions overwrites.";
    return false;
  }
  const flatbuffers::Table* overlay =
      reinterpret_cast<const flatbuffers::Table*>(
          triggering_preconditions_overlay_);
  const TriggeringPreconditions* defaults = model_->preconditions();
  if (defaults == nullptr) {
    TC3_LOG(ERROR) << "No triggering conditions specified.";
    return false;
  }

  preconditions_.min_smart_reply_triggering_score = ValueOrDefault(
      overlay, TriggeringPreconditions::VT_MIN_SMART_REPLY_TRIGGERING_SCORE,
      defaults->min_smart_reply_triggering_score());
  preconditions_.max_sensitive_topic_score = ValueOrDefault(
      overlay, TriggeringPreconditions::VT_MAX_SENSITIVE_TOPIC_SCORE,
      defaults->max_sensitive_topic_score());
  preconditions_.suppress_on_sensitive_topic = ValueOrDefault(
      overlay, TriggeringPreconditions::VT_SUPPRESS_ON_SENSITIVE_TOPIC,
      defaults->suppress_on_sensitive_topic());
  preconditions_.min_input_length =
      ValueOrDefault(overlay, TriggeringPreconditions::VT_MIN_INPUT_LENGTH,
                     defaults->min_input_length());
  preconditions_.max_input_length =
      ValueOrDefault(overlay, TriggeringPreconditions::VT_MAX_INPUT_LENGTH,
                     defaults->max_input_length());
  preconditions_.min_locale_match_fraction = ValueOrDefault(
      overlay, TriggeringPreconditions::VT_MIN_LOCALE_MATCH_FRACTION,
      defaults->min_locale_match_fraction());
  preconditions_.handle_missing_locale_as_supported = ValueOrDefault(
      overlay, TriggeringPreconditions::VT_HANDLE_MISSING_LOCALE_AS_SUPPORTED,
      defaults->handle_missing_locale_as_supported());
  preconditions_.handle_unknown_locale_as_supported = ValueOrDefault(
      overlay, TriggeringPreconditions::VT_HANDLE_UNKNOWN_LOCALE_AS_SUPPORTED,
      defaults->handle_unknown_locale_as_supported());
  preconditions_.suppress_on_low_confidence_input = ValueOrDefault(
      overlay, TriggeringPreconditions::VT_SUPPRESS_ON_LOW_CONFIDENCE_INPUT,
      defaults->suppress_on_low_confidence_input());
  preconditions_.min_reply_score_threshold = ValueOrDefault(
      overlay, TriggeringPreconditions::VT_MIN_REPLY_SCORE_THRESHOLD,
      defaults->min_reply_score_threshold());

  return true;
}

bool ActionsSuggestions::EmbedTokenId(const int32 token_id,
                                      std::vector<float>* embedding) const {
  return feature_processor_->AppendFeatures(
      {token_id},
      /*dense_features=*/{}, embedding_executor_.get(), embedding);
}

std::vector<std::vector<Token>> ActionsSuggestions::Tokenize(
    const std::vector<std::string>& context) const {
  std::vector<std::vector<Token>> tokens;
  tokens.reserve(context.size());
  for (const std::string& message : context) {
    tokens.push_back(feature_processor_->tokenizer()->Tokenize(message));
  }
  return tokens;
}

bool ActionsSuggestions::EmbedTokensPerMessage(
    const std::vector<std::vector<Token>>& tokens,
    std::vector<float>* embeddings, int* max_num_tokens_per_message) const {
  const int num_messages = tokens.size();
  *max_num_tokens_per_message = 0;
  for (int i = 0; i < num_messages; i++) {
    const int num_message_tokens = tokens[i].size();
    if (num_message_tokens > *max_num_tokens_per_message) {
      *max_num_tokens_per_message = num_message_tokens;
    }
  }

  if (model_->feature_processor_options()->min_num_tokens_per_message() >
      *max_num_tokens_per_message) {
    *max_num_tokens_per_message =
        model_->feature_processor_options()->min_num_tokens_per_message();
  }
  if (model_->feature_processor_options()->max_num_tokens_per_message() > 0 &&
      *max_num_tokens_per_message >
          model_->feature_processor_options()->max_num_tokens_per_message()) {
    *max_num_tokens_per_message =
        model_->feature_processor_options()->max_num_tokens_per_message();
  }

  // Embed all tokens and add paddings to pad tokens of each message to the
  // maximum number of tokens in a message of the conversation.
  // If a number of tokens is specified in the model config, tokens at the
  // beginning of a message are dropped if they don't fit in the limit.
  for (int i = 0; i < num_messages; i++) {
    const int start =
        std::max<int>(tokens[i].size() - *max_num_tokens_per_message, 0);
    for (int pos = start; pos < tokens[i].size(); pos++) {
      if (!feature_processor_->AppendTokenFeatures(
              tokens[i][pos], embedding_executor_.get(), embeddings)) {
        TC3_LOG(ERROR) << "Could not run token feature extractor.";
        return false;
      }
    }
    // Add padding.
    for (int k = tokens[i].size(); k < *max_num_tokens_per_message; k++) {
      embeddings->insert(embeddings->end(), embedded_padding_token_.begin(),
                         embedded_padding_token_.end());
    }
  }

  return true;
}

bool ActionsSuggestions::EmbedAndFlattenTokens(
    const std::vector<std::vector<Token>>& tokens,
    std::vector<float>* embeddings, int* total_token_count) const {
  const int num_messages = tokens.size();
  int start_message = 0;
  int message_token_offset = 0;

  // If a maximum model input length is specified, we need to check how
  // much we need to trim at the start.
  const int max_num_total_tokens =
      model_->feature_processor_options()->max_num_total_tokens();
  if (max_num_total_tokens > 0) {
    int total_tokens = 0;
    start_message = num_messages - 1;
    for (; start_message >= 0; start_message--) {
      // Tokens of the message + start and end token.
      const int num_message_tokens = tokens[start_message].size() + 2;
      total_tokens += num_message_tokens;

      // Check whether we exhausted the budget.
      if (total_tokens >= max_num_total_tokens) {
        message_token_offset = total_tokens - max_num_total_tokens;
        break;
      }
    }
  }

  // Add embeddings.
  *total_token_count = 0;
  for (int i = start_message; i < num_messages; i++) {
    if (message_token_offset == 0) {
      ++(*total_token_count);
      // Add `start message` token.
      embeddings->insert(embeddings->end(), embedded_start_token_.begin(),
                         embedded_start_token_.end());
    }

    for (int pos = std::max(0, message_token_offset - 1);
         pos < tokens[i].size(); pos++) {
      ++(*total_token_count);
      if (!feature_processor_->AppendTokenFeatures(
              tokens[i][pos], embedding_executor_.get(), embeddings)) {
        TC3_LOG(ERROR) << "Could not run token feature extractor.";
        return false;
      }
    }

    // Add `end message` token.
    ++(*total_token_count);
    embeddings->insert(embeddings->end(), embedded_end_token_.begin(),
                       embedded_end_token_.end());

    // Reset for the subsequent messages.
    message_token_offset = 0;
  }

  // Add optional padding.
  const int min_num_total_tokens =
      model_->feature_processor_options()->min_num_total_tokens();
  for (; *total_token_count < min_num_total_tokens; ++(*total_token_count)) {
    embeddings->insert(embeddings->end(), embedded_padding_token_.begin(),
                       embedded_padding_token_.end());
  }

  return true;
}

bool ActionsSuggestions::AllocateInput(const int conversation_length,
                                       const int max_tokens,
                                       const int total_token_count,
                                       tflite::Interpreter* interpreter) const {
  if (model_->tflite_model_spec()->resize_inputs()) {
    if (model_->tflite_model_spec()->input_context() >= 0) {
      interpreter->ResizeInputTensor(
          interpreter->inputs()[model_->tflite_model_spec()->input_context()],
          {1, conversation_length});
    }
    if (model_->tflite_model_spec()->input_user_id() >= 0) {
      interpreter->ResizeInputTensor(
          interpreter->inputs()[model_->tflite_model_spec()->input_user_id()],
          {1, conversation_length});
    }
    if (model_->tflite_model_spec()->input_time_diffs() >= 0) {
      interpreter->ResizeInputTensor(
          interpreter
              ->inputs()[model_->tflite_model_spec()->input_time_diffs()],
          {1, conversation_length});
    }
    if (model_->tflite_model_spec()->input_num_tokens() >= 0) {
      interpreter->ResizeInputTensor(
          interpreter
              ->inputs()[model_->tflite_model_spec()->input_num_tokens()],
          {conversation_length, 1});
    }
    if (model_->tflite_model_spec()->input_token_embeddings() >= 0) {
      interpreter->ResizeInputTensor(
          interpreter
              ->inputs()[model_->tflite_model_spec()->input_token_embeddings()],
          {conversation_length, max_tokens, token_embedding_size_});
    }
    if (model_->tflite_model_spec()->input_flattened_token_embeddings() >= 0) {
      interpreter->ResizeInputTensor(
          interpreter->inputs()[model_->tflite_model_spec()
                                    ->input_flattened_token_embeddings()],
          {1, total_token_count});
    }
  }

  return interpreter->AllocateTensors() == kTfLiteOk;
}

bool ActionsSuggestions::SetupModelInput(
    const std::vector<std::string>& context, const std::vector<int>& user_ids,
    const std::vector<float>& time_diffs, const int num_suggestions,
    const ActionSuggestionOptions& options,
    tflite::Interpreter* interpreter) const {
  // Compute token embeddings.
  std::vector<std::vector<Token>> tokens;
  std::vector<float> token_embeddings;
  std::vector<float> flattened_token_embeddings;
  int max_tokens = 0;
  int total_token_count = 0;
  if (model_->tflite_model_spec()->input_num_tokens() >= 0 ||
      model_->tflite_model_spec()->input_token_embeddings() >= 0 ||
      model_->tflite_model_spec()->input_flattened_token_embeddings() >= 0) {
    if (feature_processor_ == nullptr) {
      TC3_LOG(ERROR) << "No feature processor specified.";
      return false;
    }

    // Tokenize the messages in the conversation.
    tokens = Tokenize(context);
    if (model_->tflite_model_spec()->input_token_embeddings() >= 0) {
      if (!EmbedTokensPerMessage(tokens, &token_embeddings, &max_tokens)) {
        TC3_LOG(ERROR) << "Could not extract token features.";
        return false;
      }
    }
    if (model_->tflite_model_spec()->input_flattened_token_embeddings() >= 0) {
      if (!EmbedAndFlattenTokens(tokens, &flattened_token_embeddings,
                                 &total_token_count)) {
        TC3_LOG(ERROR) << "Could not extract token features.";
        return false;
      }
    }
  }

  if (!AllocateInput(context.size(), max_tokens, total_token_count,
                     interpreter)) {
    TC3_LOG(ERROR) << "TensorFlow Lite model allocation failed.";
    return false;
  }
  if (model_->tflite_model_spec()->input_context() >= 0) {
    model_executor_->SetInput<std::string>(
        model_->tflite_model_spec()->input_context(), context, interpreter);
  }
  if (model_->tflite_model_spec()->input_context_length() >= 0) {
    model_executor_->SetInput<int>(
        model_->tflite_model_spec()->input_context_length(), context.size(),
        interpreter);
  }
  if (model_->tflite_model_spec()->input_user_id() >= 0) {
    model_executor_->SetInput<int>(model_->tflite_model_spec()->input_user_id(),
                                   user_ids, interpreter);
  }
  if (model_->tflite_model_spec()->input_num_suggestions() >= 0) {
    model_executor_->SetInput<int>(
        model_->tflite_model_spec()->input_num_suggestions(), num_suggestions,
        interpreter);
  }
  if (model_->tflite_model_spec()->input_time_diffs() >= 0) {
    model_executor_->SetInput<float>(
        model_->tflite_model_spec()->input_time_diffs(), time_diffs,
        interpreter);
  }
  if (model_->tflite_model_spec()->input_num_tokens() >= 0) {
    std::vector<int> num_tokens_per_message(tokens.size());
    for (int i = 0; i < tokens.size(); i++) {
      num_tokens_per_message[i] = tokens[i].size();
    }
    model_executor_->SetInput<int>(
        model_->tflite_model_spec()->input_num_tokens(), num_tokens_per_message,
        interpreter);
  }
  if (model_->tflite_model_spec()->input_token_embeddings() >= 0) {
    model_executor_->SetInput<float>(
        model_->tflite_model_spec()->input_token_embeddings(), token_embeddings,
        interpreter);
  }
  if (model_->tflite_model_spec()->input_flattened_token_embeddings() >= 0) {
    model_executor_->SetInput<float>(
        model_->tflite_model_spec()->input_flattened_token_embeddings(),
        flattened_token_embeddings, interpreter);
  }
  // Set up additional input parameters.
  if (const auto* input_name_index =
          model_->tflite_model_spec()->input_name_index()) {
    const std::unordered_map<std::string, Variant>& model_parameters =
        options.model_parameters;
    for (const TensorflowLiteModelSpec_::InputNameIndexEntry* entry :
         *input_name_index) {
      const std::string param_name = entry->key()->str();
      const int param_index = entry->value();
      const TfLiteType param_type =
          interpreter->tensor(interpreter->inputs()[param_index])->type;
      const auto param_value_it = model_parameters.find(param_name);
      const bool has_value = param_value_it != model_parameters.end();
      switch (param_type) {
        case kTfLiteFloat32:
          model_executor_->SetInput<float>(
              param_index,
              has_value ? param_value_it->second.Value<float>() : kDefaultFloat,
              interpreter);
          break;
        case kTfLiteInt32:
          model_executor_->SetInput<int32_t>(
              param_index,
              has_value ? param_value_it->second.Value<int>() : kDefaultInt,
              interpreter);
          break;
        case kTfLiteInt64:
          model_executor_->SetInput<int64_t>(
              param_index,
              has_value ? param_value_it->second.Value<int64>() : kDefaultInt,
              interpreter);
          break;
        case kTfLiteUInt8:
          model_executor_->SetInput<uint8_t>(
              param_index,
              has_value ? param_value_it->second.Value<uint8>() : kDefaultInt,
              interpreter);
          break;
        case kTfLiteInt8:
          model_executor_->SetInput<int8_t>(
              param_index,
              has_value ? param_value_it->second.Value<int8>() : kDefaultInt,
              interpreter);
          break;
        case kTfLiteBool:
          model_executor_->SetInput<bool>(
              param_index,
              has_value ? param_value_it->second.Value<bool>() : kDefaultBool,
              interpreter);
          break;
        default:
          TC3_LOG(ERROR) << "Unsupported type of additional input parameter: "
                         << param_name;
      }
    }
  }
  return true;
}

void ActionsSuggestions::PopulateTextReplies(
    const tflite::Interpreter* interpreter, int suggestion_index,
    int score_index, const std::string& type,
    ActionsSuggestionsResponse* response) const {
  const std::vector<tflite::StringRef> replies =
      model_executor_->Output<tflite::StringRef>(suggestion_index, interpreter);
  const TensorView<float> scores =
      model_executor_->OutputView<float>(score_index, interpreter);
  for (int i = 0; i < replies.size(); i++) {
    if (replies[i].len == 0) {
      continue;
    }
    const float score = scores.data()[i];
    if (score < preconditions_.min_reply_score_threshold) {
      continue;
    }
    response->actions.push_back(
        {std::string(replies[i].str, replies[i].len), type, score});
  }
}

void ActionsSuggestions::FillSuggestionFromSpecWithEntityData(
    const ActionSuggestionSpec* spec, ActionSuggestion* suggestion) const {
  std::unique_ptr<ReflectiveFlatbuffer> entity_data =
      entity_data_builder_ != nullptr ? entity_data_builder_->NewRoot()
                                      : nullptr;
  FillSuggestionFromSpec(spec, entity_data.get(), suggestion);
}

void ActionsSuggestions::PopulateIntentTriggering(
    const tflite::Interpreter* interpreter, int suggestion_index,
    int score_index, const ActionSuggestionSpec* task_spec,
    ActionsSuggestionsResponse* response) const {
  if (!task_spec || task_spec->type()->size() == 0) {
    TC3_LOG(ERROR)
        << "Task type for intent (action) triggering cannot be empty!";
    return;
  }
  const TensorView<bool> intent_prediction =
      model_executor_->OutputView<bool>(suggestion_index, interpreter);
  const TensorView<float> intent_scores =
      model_executor_->OutputView<float>(score_index, interpreter);
  // Two result corresponding to binary triggering case.
  TC3_CHECK_EQ(intent_prediction.size(), 2);
  TC3_CHECK_EQ(intent_scores.size(), 2);
  // We rely on in-graph thresholding logic so at this point the results
  // have been ranked properly according to threshold.
  const bool triggering = intent_prediction.data()[0];
  const float trigger_score = intent_scores.data()[0];

  if (triggering) {
    ActionSuggestion suggestion;
    std::unique_ptr<ReflectiveFlatbuffer> entity_data =
        entity_data_builder_ != nullptr ? entity_data_builder_->NewRoot()
                                        : nullptr;
    FillSuggestionFromSpecWithEntityData(task_spec, &suggestion);
    suggestion.score = trigger_score;
    response->actions.push_back(std::move(suggestion));
  }
}

bool ActionsSuggestions::ReadModelOutput(
    tflite::Interpreter* interpreter, const ActionSuggestionOptions& options,
    ActionsSuggestionsResponse* response) const {
  // Read sensitivity and triggering score predictions.
  if (model_->tflite_model_spec()->output_triggering_score() >= 0) {
    const TensorView<float> triggering_score =
        model_executor_->OutputView<float>(
            model_->tflite_model_spec()->output_triggering_score(),
            interpreter);
    if (!triggering_score.is_valid() || triggering_score.size() == 0) {
      TC3_LOG(ERROR) << "Could not compute triggering score.";
      return false;
    }
    response->triggering_score = triggering_score.data()[0];
    response->output_filtered_min_triggering_score =
        (response->triggering_score <
         preconditions_.min_smart_reply_triggering_score);
  }
  if (model_->tflite_model_spec()->output_sensitive_topic_score() >= 0) {
    const TensorView<float> sensitive_topic_score =
        model_executor_->OutputView<float>(
            model_->tflite_model_spec()->output_sensitive_topic_score(),
            interpreter);
    if (!sensitive_topic_score.is_valid() ||
        sensitive_topic_score.dim(0) != 1) {
      TC3_LOG(ERROR) << "Could not compute sensitive topic score.";
      return false;
    }
    response->sensitivity_score = sensitive_topic_score.data()[0];
    response->output_filtered_sensitivity =
        (response->sensitivity_score >
         preconditions_.max_sensitive_topic_score);
  }

  // Suppress model outputs.
  if (response->output_filtered_sensitivity) {
    return true;
  }

  // Read smart reply predictions.
  if (!response->output_filtered_min_triggering_score &&
      model_->tflite_model_spec()->output_replies() >= 0) {
    PopulateTextReplies(interpreter,
                        model_->tflite_model_spec()->output_replies(),
                        model_->tflite_model_spec()->output_replies_scores(),
                        model_->smart_reply_action_type()->str(), response);
  }

  // Read actions suggestions.
  if (model_->tflite_model_spec()->output_actions_scores() >= 0) {
    const TensorView<float> actions_scores = model_executor_->OutputView<float>(
        model_->tflite_model_spec()->output_actions_scores(), interpreter);
    for (int i = 0; i < model_->action_type()->size(); i++) {
      const ActionTypeOptions* action_type = model_->action_type()->Get(i);
      // Skip disabled action classes, such as the default other category.
      if (!action_type->enabled()) {
        continue;
      }
      const float score = actions_scores.data()[i];
      if (score < action_type->min_triggering_score()) {
        continue;
      }

      // Create action from model output.
      ActionSuggestion suggestion;
      suggestion.type = action_type->name()->str();
      std::unique_ptr<ReflectiveFlatbuffer> entity_data =
          entity_data_builder_ != nullptr ? entity_data_builder_->NewRoot()
                                          : nullptr;
      FillSuggestionFromSpecWithEntityData(action_type->action(), &suggestion);
      suggestion.score = score;
      response->actions.push_back(std::move(suggestion));
    }
  }

  // Read multi-task predictions and construct the result properly.
  if (const auto* prediction_metadata =
          model_->tflite_model_spec()->prediction_metadata()) {
    for (const PredictionMetadata* metadata : *prediction_metadata) {
      const ActionSuggestionSpec* task_spec = metadata->task_spec();
      const int suggestions_index = metadata->output_suggestions();
      const int suggestions_scores_index =
          metadata->output_suggestions_scores();
      switch (metadata->prediction_type()) {
        case PredictionType_NEXT_MESSAGE_PREDICTION:
          if (!task_spec || task_spec->type()->size() == 0) {
            TC3_LOG(WARNING) << "Task type not provided, use default "
                                "smart_reply_action_type!";
          }
          PopulateTextReplies(
              interpreter, suggestions_index, suggestions_scores_index,
              task_spec ? task_spec->type()->str()
                        : model_->smart_reply_action_type()->str(),
              response);
          break;
        case PredictionType_INTENT_TRIGGERING:
          PopulateIntentTriggering(interpreter, suggestions_index,
                                   suggestions_scores_index, task_spec,
                                   response);
          break;
        default:
          TC3_LOG(ERROR) << "Unsupported prediction type!";
          return false;
      }
    }
  }

  return true;
}

bool ActionsSuggestions::SuggestActionsFromModel(
    const Conversation& conversation, const int num_messages,
    const ActionSuggestionOptions& options,
    ActionsSuggestionsResponse* response,
    std::unique_ptr<tflite::Interpreter>* interpreter) const {
  TC3_CHECK_LE(num_messages, conversation.messages.size());

  if (!model_executor_) {
    return true;
  }
  *interpreter = model_executor_->CreateInterpreter();

  if (!*interpreter) {
    TC3_LOG(ERROR) << "Could not build TensorFlow Lite interpreter for the "
                      "actions suggestions model.";
    return false;
  }

  std::vector<std::string> context;
  std::vector<int> user_ids;
  std::vector<float> time_diffs;
  context.reserve(num_messages);
  user_ids.reserve(num_messages);
  time_diffs.reserve(num_messages);

  // Gather last `num_messages` messages from the conversation.
  int64 last_message_reference_time_ms_utc = 0;
  const float second_in_ms = 1000;
  for (int i = conversation.messages.size() - num_messages;
       i < conversation.messages.size(); i++) {
    const ConversationMessage& message = conversation.messages[i];
    context.push_back(message.text);
    user_ids.push_back(message.user_id);

    float time_diff_secs = 0;
    if (message.reference_time_ms_utc != 0 &&
        last_message_reference_time_ms_utc != 0) {
      time_diff_secs = std::max(0.0f, (message.reference_time_ms_utc -
                                       last_message_reference_time_ms_utc) /
                                          second_in_ms);
    }
    if (message.reference_time_ms_utc != 0) {
      last_message_reference_time_ms_utc = message.reference_time_ms_utc;
    }
    time_diffs.push_back(time_diff_secs);
  }

  if (!SetupModelInput(context, user_ids, time_diffs,
                       /*num_suggestions=*/model_->num_smart_replies(), options,
                       interpreter->get())) {
    TC3_LOG(ERROR) << "Failed to setup input for TensorFlow Lite model.";
    return false;
  }

  if ((*interpreter)->Invoke() != kTfLiteOk) {
    TC3_LOG(ERROR) << "Failed to invoke TensorFlow Lite interpreter.";
    return false;
  }

  return ReadModelOutput(interpreter->get(), options, response);
}

AnnotationOptions ActionsSuggestions::AnnotationOptionsForMessage(
    const ConversationMessage& message) const {
  AnnotationOptions options;
  options.detected_text_language_tags = message.detected_text_language_tags;
  options.reference_time_ms_utc = message.reference_time_ms_utc;
  options.reference_timezone = message.reference_timezone;
  options.annotation_usecase =
      model_->annotation_actions_spec()->annotation_usecase();
  options.is_serialized_entity_data_enabled =
      model_->annotation_actions_spec()->is_serialized_entity_data_enabled();
  options.entity_types = annotation_entity_types_;
  return options;
}

// Run annotator on the messages of a conversation.
Conversation ActionsSuggestions::AnnotateConversation(
    const Conversation& conversation, const Annotator* annotator) const {
  if (annotator == nullptr) {
    return conversation;
  }
  const int num_messages_grammar =
      ((model_->rules() && model_->rules()->grammar_rules() &&
        model_->rules()
            ->grammar_rules()
            ->rules()
            ->nonterminals()
            ->annotation_nt())
           ? 1
           : 0);
  const int num_messages_mapping =
      (model_->annotation_actions_spec()
           ? std::max(model_->annotation_actions_spec()
                          ->max_history_from_any_person(),
                      model_->annotation_actions_spec()
                          ->max_history_from_last_person())
           : 0);
  const int num_messages = std::max(num_messages_grammar, num_messages_mapping);
  if (num_messages == 0) {
    // No annotations are used.
    return conversation;
  }
  Conversation annotated_conversation = conversation;
  for (int i = 0, message_index = annotated_conversation.messages.size() - 1;
       i < num_messages && message_index >= 0; i++, message_index--) {
    ConversationMessage* message =
        &annotated_conversation.messages[message_index];
    if (message->annotations.empty()) {
      message->annotations = annotator->Annotate(
          message->text, AnnotationOptionsForMessage(*message));
      for (int i = 0; i < message->annotations.size(); i++) {
        ClassificationResult* classification =
            &message->annotations[i].classification.front();

        // Specialize datetime annotation to time annotation if no date
        // component is present.
        if (classification->collection == Collections::DateTime() &&
            classification->datetime_parse_result.IsSet()) {
          bool has_only_time = true;
          for (const DatetimeComponent& component :
               classification->datetime_parse_result.datetime_components) {
            if (component.component_type !=
                    DatetimeComponent::ComponentType::UNSPECIFIED &&
                component.component_type <
                    DatetimeComponent::ComponentType::HOUR) {
              has_only_time = false;
              break;
            }
          }
          if (has_only_time) {
            classification->collection = kTimeAnnotation;
          }
        }
      }
    }
  }
  return annotated_conversation;
}

void ActionsSuggestions::SuggestActionsFromAnnotations(
    const Conversation& conversation,
    std::vector<ActionSuggestion>* actions) const {
  if (model_->annotation_actions_spec() == nullptr ||
      model_->annotation_actions_spec()->annotation_mapping() == nullptr ||
      model_->annotation_actions_spec()->annotation_mapping()->size() == 0) {
    return;
  }

  // Create actions based on the annotations.
  const int max_from_any_person =
      model_->annotation_actions_spec()->max_history_from_any_person();
  const int max_from_last_person =
      model_->annotation_actions_spec()->max_history_from_last_person();
  const int last_person = conversation.messages.back().user_id;

  int num_messages_last_person = 0;
  int num_messages_any_person = 0;
  bool all_from_last_person = true;
  for (int message_index = conversation.messages.size() - 1; message_index >= 0;
       message_index--) {
    const ConversationMessage& message = conversation.messages[message_index];
    std::vector<AnnotatedSpan> annotations = message.annotations;

    // Update how many messages we have processed from the last person in the
    // conversation and from any person in the conversation.
    num_messages_any_person++;
    if (all_from_last_person && message.user_id == last_person) {
      num_messages_last_person++;
    } else {
      all_from_last_person = false;
    }

    if (num_messages_any_person > max_from_any_person &&
        (!all_from_last_person ||
         num_messages_last_person > max_from_last_person)) {
      break;
    }

    if (message.user_id == kLocalUserId) {
      if (model_->annotation_actions_spec()->only_until_last_sent()) {
        break;
      }
      if (!model_->annotation_actions_spec()->include_local_user_messages()) {
        continue;
      }
    }

    std::vector<ActionSuggestionAnnotation> action_annotations;
    action_annotations.reserve(annotations.size());
    for (const AnnotatedSpan& annotation : annotations) {
      if (annotation.classification.empty()) {
        continue;
      }

      const ClassificationResult& classification_result =
          annotation.classification[0];

      ActionSuggestionAnnotation action_annotation;
      action_annotation.span = {
          message_index, annotation.span,
          UTF8ToUnicodeText(message.text, /*do_copy=*/false)
              .UTF8Substring(annotation.span.first, annotation.span.second)};
      action_annotation.entity = classification_result;
      action_annotation.name = classification_result.collection;
      action_annotations.push_back(std::move(action_annotation));
    }

    if (model_->annotation_actions_spec()->deduplicate_annotations()) {
      // Create actions only for deduplicated annotations.
      for (const int annotation_id :
           DeduplicateAnnotations(action_annotations)) {
        SuggestActionsFromAnnotation(
            message_index, action_annotations[annotation_id], actions);
      }
    } else {
      // Create actions for all annotations.
      for (const ActionSuggestionAnnotation& annotation : action_annotations) {
        SuggestActionsFromAnnotation(message_index, annotation, actions);
      }
    }
  }
}

void ActionsSuggestions::SuggestActionsFromAnnotation(
    const int message_index, const ActionSuggestionAnnotation& annotation,
    std::vector<ActionSuggestion>* actions) const {
  for (const AnnotationActionsSpec_::AnnotationMapping* mapping :
       *model_->annotation_actions_spec()->annotation_mapping()) {
    if (annotation.entity.collection ==
        mapping->annotation_collection()->str()) {
      if (annotation.entity.score < mapping->min_annotation_score()) {
        continue;
      }

      std::unique_ptr<ReflectiveFlatbuffer> entity_data =
          entity_data_builder_ != nullptr ? entity_data_builder_->NewRoot()
                                          : nullptr;

      // Set annotation text as (additional) entity data field.
      if (mapping->entity_field() != nullptr) {
        TC3_CHECK_NE(entity_data, nullptr);

        UnicodeText normalized_annotation_text =
            UTF8ToUnicodeText(annotation.span.text, /*do_copy=*/false);

        // Apply normalization if specified.
        if (mapping->normalization_options() != nullptr) {
          normalized_annotation_text =
              NormalizeText(*unilib_, mapping->normalization_options(),
                            normalized_annotation_text);
        }

        entity_data->ParseAndSet(mapping->entity_field(),
                                 normalized_annotation_text.ToUTF8String());
      }

      ActionSuggestion suggestion;
      FillSuggestionFromSpec(mapping->action(), entity_data.get(), &suggestion);
      if (mapping->use_annotation_score()) {
        suggestion.score = annotation.entity.score;
      }
      suggestion.annotations = {annotation};
      actions->push_back(std::move(suggestion));
    }
  }
}

std::vector<int> ActionsSuggestions::DeduplicateAnnotations(
    const std::vector<ActionSuggestionAnnotation>& annotations) const {
  std::map<std::pair<std::string, std::string>, int> deduplicated_annotations;

  for (int i = 0; i < annotations.size(); i++) {
    const std::pair<std::string, std::string> key = {annotations[i].name,
                                                     annotations[i].span.text};
    auto entry = deduplicated_annotations.find(key);
    if (entry != deduplicated_annotations.end()) {
      // Kepp the annotation with the higher score.
      if (annotations[entry->second].entity.score <
          annotations[i].entity.score) {
        entry->second = i;
      }
      continue;
    }
    deduplicated_annotations.insert(entry, {key, i});
  }

  std::vector<int> result;
  result.reserve(deduplicated_annotations.size());
  for (const auto& key_and_annotation : deduplicated_annotations) {
    result.push_back(key_and_annotation.second);
  }
  return result;
}

bool ActionsSuggestions::SuggestActionsFromLua(
    const Conversation& conversation, const TfLiteModelExecutor* model_executor,
    const tflite::Interpreter* interpreter,
    const reflection::Schema* annotation_entity_data_schema,
    std::vector<ActionSuggestion>* actions) const {
  if (lua_bytecode_.empty()) {
    return true;
  }

  auto lua_actions = LuaActionsSuggestions::CreateLuaActionsSuggestions(
      lua_bytecode_, conversation, model_executor, model_->tflite_model_spec(),
      interpreter, entity_data_schema_, annotation_entity_data_schema);
  if (lua_actions == nullptr) {
    TC3_LOG(ERROR) << "Could not create lua actions.";
    return false;
  }
  return lua_actions->SuggestActions(actions);
}

bool ActionsSuggestions::GatherActionsSuggestions(
    const Conversation& conversation, const Annotator* annotator,
    const ActionSuggestionOptions& options,
    ActionsSuggestionsResponse* response) const {
  if (conversation.messages.empty()) {
    return true;
  }

  // Run annotator against messages.
  const Conversation annotated_conversation =
      AnnotateConversation(conversation, annotator);

  const int num_messages = NumMessagesToConsider(
      annotated_conversation, model_->max_conversation_history_length());

  if (num_messages <= 0) {
    TC3_LOG(INFO) << "No messages provided for actions suggestions.";
    return false;
  }

  SuggestActionsFromAnnotations(annotated_conversation, &response->actions);

  if (grammar_actions_ != nullptr &&
      !grammar_actions_->SuggestActions(annotated_conversation,
                                        &response->actions)) {
    TC3_LOG(ERROR) << "Could not suggest actions from grammar rules.";
    return false;
  }

  int input_text_length = 0;
  int num_matching_locales = 0;
  for (int i = annotated_conversation.messages.size() - num_messages;
       i < annotated_conversation.messages.size(); i++) {
    input_text_length += annotated_conversation.messages[i].text.length();
    std::vector<Locale> message_languages;
    if (!ParseLocales(
            annotated_conversation.messages[i].detected_text_language_tags,
            &message_languages)) {
      continue;
    }
    if (Locale::IsAnyLocaleSupported(
            message_languages, locales_,
            preconditions_.handle_unknown_locale_as_supported)) {
      ++num_matching_locales;
    }
  }

  // Bail out if we are provided with too few or too much input.
  if (input_text_length < preconditions_.min_input_length ||
      (preconditions_.max_input_length >= 0 &&
       input_text_length > preconditions_.max_input_length)) {
    TC3_LOG(INFO) << "Too much or not enough input for inference.";
    return response;
  }

  // Bail out if the text does not look like it can be handled by the model.
  const float matching_fraction =
      static_cast<float>(num_matching_locales) / num_messages;
  if (matching_fraction < preconditions_.min_locale_match_fraction) {
    TC3_LOG(INFO) << "Not enough locale matches.";
    response->output_filtered_locale_mismatch = true;
    return true;
  }

  std::vector<const UniLib::RegexPattern*> post_check_rules;
  if (preconditions_.suppress_on_low_confidence_input) {
    if ((ngram_model_ != nullptr &&
         ngram_model_->EvalConversation(annotated_conversation,
                                        num_messages)) ||
        regex_actions_->IsLowConfidenceInput(annotated_conversation,
                                             num_messages, &post_check_rules)) {
      response->output_filtered_low_confidence = true;
      return true;
    }
  }

  std::unique_ptr<tflite::Interpreter> interpreter;
  if (!SuggestActionsFromModel(annotated_conversation, num_messages, options,
                               response, &interpreter)) {
    TC3_LOG(ERROR) << "Could not run model.";
    return false;
  }

  // Suppress all predictions if the conversation was deemed sensitive.
  if (preconditions_.suppress_on_sensitive_topic &&
      response->output_filtered_sensitivity) {
    return true;
  }

  if (!SuggestActionsFromLua(
          annotated_conversation, model_executor_.get(), interpreter.get(),
          annotator != nullptr ? annotator->entity_data_schema() : nullptr,
          &response->actions)) {
    TC3_LOG(ERROR) << "Could not suggest actions from script.";
    return false;
  }

  if (!regex_actions_->SuggestActions(annotated_conversation,
                                      entity_data_builder_.get(),
                                      &response->actions)) {
    TC3_LOG(ERROR) << "Could not suggest actions from regex rules.";
    return false;
  }

  if (preconditions_.suppress_on_low_confidence_input &&
      !regex_actions_->FilterConfidenceOutput(post_check_rules,
                                              &response->actions)) {
    TC3_LOG(ERROR) << "Could not post-check actions.";
    return false;
  }

  return true;
}

ActionsSuggestionsResponse ActionsSuggestions::SuggestActions(
    const Conversation& conversation, const Annotator* annotator,
    const ActionSuggestionOptions& options) const {
  ActionsSuggestionsResponse response;

  // Assert that messages are sorted correctly.
  for (int i = 1; i < conversation.messages.size(); i++) {
    if (conversation.messages[i].reference_time_ms_utc <
        conversation.messages[i - 1].reference_time_ms_utc) {
      TC3_LOG(ERROR) << "Messages are not sorted most recent last.";
    }
  }

  if (!GatherActionsSuggestions(conversation, annotator, options, &response)) {
    TC3_LOG(ERROR) << "Could not gather actions suggestions.";
    response.actions.clear();
  } else if (!ranker_->RankActions(conversation, &response, entity_data_schema_,
                                   annotator != nullptr
                                       ? annotator->entity_data_schema()
                                       : nullptr)) {
    TC3_LOG(ERROR) << "Could not rank actions.";
    response.actions.clear();
  }
  return response;
}

ActionsSuggestionsResponse ActionsSuggestions::SuggestActions(
    const Conversation& conversation,
    const ActionSuggestionOptions& options) const {
  return SuggestActions(conversation, /*annotator=*/nullptr, options);
}

const ActionsModel* ActionsSuggestions::model() const { return model_; }
const reflection::Schema* ActionsSuggestions::entity_data_schema() const {
  return entity_data_schema_;
}

const ActionsModel* ViewActionsModel(const void* buffer, int size) {
  if (buffer == nullptr) {
    return nullptr;
  }
  return LoadAndVerifyModel(reinterpret_cast<const uint8_t*>(buffer), size);
}

}  // namespace libtextclassifier3
