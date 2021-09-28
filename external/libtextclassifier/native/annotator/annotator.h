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

// Inference code for the text classification model.

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_ANNOTATOR_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_ANNOTATOR_H_

#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "annotator/contact/contact-engine.h"
#include "annotator/datetime/parser.h"
#include "annotator/duration/duration.h"
#include "annotator/experimental/experimental.h"
#include "annotator/feature-processor.h"
#include "annotator/grammar/dates/cfg-datetime-annotator.h"
#include "annotator/grammar/grammar-annotator.h"
#include "annotator/installed_app/installed-app-engine.h"
#include "annotator/knowledge/knowledge-engine.h"
#include "annotator/model-executor.h"
#include "annotator/model_generated.h"
#include "annotator/number/number.h"
#include "annotator/person_name/person-name-engine.h"
#include "annotator/strip-unpaired-brackets.h"
#include "annotator/translate/translate.h"
#include "annotator/types.h"
#include "annotator/zlib-utils.h"
#include "utils/base/status.h"
#include "utils/base/statusor.h"
#include "utils/flatbuffers.h"
#include "utils/i18n/locale.h"
#include "utils/memory/mmap.h"
#include "utils/utf8/unilib.h"
#include "utils/zlib/zlib.h"
#include "lang_id/lang-id.h"

namespace libtextclassifier3 {

// Holds TFLite interpreters for selection and classification models.
// NOTE: This class is not thread-safe, thus should NOT be re-used across
// threads.
class InterpreterManager {
 public:
  // The constructor can be called with nullptr for any of the executors, and is
  // a defined behavior, as long as the corresponding *Interpreter() method is
  // not called when the executor is null.
  InterpreterManager(const ModelExecutor* selection_executor,
                     const ModelExecutor* classification_executor)
      : selection_executor_(selection_executor),
        classification_executor_(classification_executor) {}

  // Gets or creates and caches an interpreter for the selection model.
  tflite::Interpreter* SelectionInterpreter();

  // Gets or creates and caches an interpreter for the classification model.
  tflite::Interpreter* ClassificationInterpreter();

 private:
  const ModelExecutor* selection_executor_;
  const ModelExecutor* classification_executor_;

  std::unique_ptr<tflite::Interpreter> selection_interpreter_;
  std::unique_ptr<tflite::Interpreter> classification_interpreter_;
};

// Stores entity types enabled for annotation, and provides operator() for
// checking whether a given entity type is enabled.
class EnabledEntityTypes {
 public:
  explicit EnabledEntityTypes(
      const std::unordered_set<std::string>& entity_types)
      : entity_types_(entity_types) {}

  bool operator()(const std::string& entity_type) const {
    return entity_types_.empty() ||
           entity_types_.find(entity_type) != entity_types_.cend();
  }

 private:
  const std::unordered_set<std::string>& entity_types_;
};

// A text processing model that provides text classification, annotation,
// selection suggestion for various types.
// NOTE: This class is not thread-safe.
class Annotator {
 public:
  static std::unique_ptr<Annotator> FromUnownedBuffer(
      const char* buffer, int size, const UniLib* unilib = nullptr,
      const CalendarLib* calendarlib = nullptr);
  // Takes ownership of the mmap.
  static std::unique_ptr<Annotator> FromScopedMmap(
      std::unique_ptr<ScopedMmap>* mmap, const UniLib* unilib = nullptr,
      const CalendarLib* calendarlib = nullptr);
  static std::unique_ptr<Annotator> FromScopedMmap(
      std::unique_ptr<ScopedMmap>* mmap, std::unique_ptr<UniLib> unilib,
      std::unique_ptr<CalendarLib> calendarlib);
  static std::unique_ptr<Annotator> FromFileDescriptor(
      int fd, int offset, int size, const UniLib* unilib = nullptr,
      const CalendarLib* calendarlib = nullptr);
  static std::unique_ptr<Annotator> FromFileDescriptor(
      int fd, int offset, int size, std::unique_ptr<UniLib> unilib,
      std::unique_ptr<CalendarLib> calendarlib);
  static std::unique_ptr<Annotator> FromFileDescriptor(
      int fd, const UniLib* unilib = nullptr,
      const CalendarLib* calendarlib = nullptr);
  static std::unique_ptr<Annotator> FromFileDescriptor(
      int fd, std::unique_ptr<UniLib> unilib,
      std::unique_ptr<CalendarLib> calendarlib);
  static std::unique_ptr<Annotator> FromPath(
      const std::string& path, const UniLib* unilib = nullptr,
      const CalendarLib* calendarlib = nullptr);
  static std::unique_ptr<Annotator> FromPath(
      const std::string& path, std::unique_ptr<UniLib> unilib,
      std::unique_ptr<CalendarLib> calendarlib);

  // Returns true if the model is ready for use.
  bool IsInitialized() { return initialized_; }

  // Initializes the knowledge engine with the given config.
  bool InitializeKnowledgeEngine(const std::string& serialized_config);

  // Initializes the contact engine with the given config.
  bool InitializeContactEngine(const std::string& serialized_config);

  // Initializes the installed app engine with the given config.
  bool InitializeInstalledAppEngine(const std::string& serialized_config);

  // Initializes the person name engine with the given person name model in the
  // provided buffer. The buffer needs to outlive the annotator.
  bool InitializePersonNameEngineFromUnownedBuffer(const void* buffer,
                                                   int size);

  // Initializes the person name engine with the given person name model from
  // the provided mmap.
  bool InitializePersonNameEngineFromScopedMmap(const ScopedMmap& mmap);

  // Initializes the person name engine with the given person name model in the
  // provided file path.
  bool InitializePersonNameEngineFromPath(const std::string& path);

  // Initializes the person name engine with the given person name model in the
  // provided file descriptor.
  bool InitializePersonNameEngineFromFileDescriptor(int fd, int offset,
                                                    int size);

  // Initializes the experimental annotators if available.
  // Returns true if there is an implementation of experimental annotators
  // linked in.
  bool InitializeExperimentalAnnotators();

  // Sets up the lang-id instance that should be used.
  void SetLangId(const libtextclassifier3::mobile::lang_id::LangId* lang_id);

  // Runs inference for given a context and current selection (i.e. index
  // of the first and one past last selected characters (utf8 codepoint
  // offsets)). Returns the indices (utf8 codepoint offsets) of the selection
  // beginning character and one past selection end character.
  // Returns the original click_indices if an error occurs.
  // NOTE: The selection indices are passed in and returned in terms of
  // UTF8 codepoints (not bytes).
  // Requires that the model is a smart selection model.
  CodepointSpan SuggestSelection(
      const std::string& context, CodepointSpan click_indices,
      const SelectionOptions& options = SelectionOptions()) const;

  // Classifies the selected text given the context string.
  // Returns an empty result if an error occurs.
  std::vector<ClassificationResult> ClassifyText(
      const std::string& context, CodepointSpan selection_indices,
      const ClassificationOptions& options = ClassificationOptions()) const;

  // Annotates the given structed input request. Models which handle the full
  // context request will receive all the metadata they require. While models
  // that don't use the extra context are called using only a string.
  // For each fragment the annotations are sorted by their position in
  // the fragment and exclude spans classified as 'other'.
  //
  // The number of vectors of annotated spans will match the number
  // of input fragments. The order of annotation span vectors will match the
  // order of input fragments. If annotation is not possible for any of the
  // annotators, no annotation is returned.
  StatusOr<std::vector<std::vector<AnnotatedSpan>>> AnnotateStructuredInput(
      const std::vector<InputFragment>& string_fragments,
      const AnnotationOptions& options = AnnotationOptions()) const;

  // Annotates given input text. The annotations are sorted by their position
  // in the context string and exclude spans classified as 'other'.
  std::vector<AnnotatedSpan> Annotate(
      const std::string& context,
      const AnnotationOptions& options = AnnotationOptions()) const;

  // Looks up a knowledge entity by its id. If successful, populates the
  // serialized knowledge result and returns true.
  bool LookUpKnowledgeEntity(const std::string& id,
                             std::string* serialized_knowledge_result) const;

  const Model* model() const;
  const reflection::Schema* entity_data_schema() const;

  // Exposes the feature processor for tests and evaluations.
  const FeatureProcessor* SelectionFeatureProcessorForTests() const;
  const FeatureProcessor* ClassificationFeatureProcessorForTests() const;

  // Exposes the date time parser for tests and evaluations.
  const DatetimeParser* DatetimeParserForTests() const;

  static const std::string& kPhoneCollection;
  static const std::string& kAddressCollection;
  static const std::string& kDateCollection;
  static const std::string& kUrlCollection;
  static const std::string& kEmailCollection;

 protected:
  struct ScoredChunk {
    TokenSpan token_span;
    float score;
  };

  // Constructs and initializes text classifier from given model.
  // Takes ownership of 'mmap', and thus owns the buffer that backs 'model'.
  Annotator(std::unique_ptr<ScopedMmap>* mmap, const Model* model,
            const UniLib* unilib, const CalendarLib* calendarlib);
  Annotator(std::unique_ptr<ScopedMmap>* mmap, const Model* model,
            std::unique_ptr<UniLib> unilib,
            std::unique_ptr<CalendarLib> calendarlib);

  // Constructs, validates and initializes text classifier from given model.
  // Does not own the buffer that backs 'model'.
  Annotator(const Model* model, const UniLib* unilib,
            const CalendarLib* calendarlib);

  // Checks that model contains all required fields, and initializes internal
  // datastructures.
  void ValidateAndInitialize();

  // Initializes regular expressions for the regex model.
  bool InitializeRegexModel(ZlibDecompressor* decompressor);

  // Resolves conflicts in the list of candidates by removing some overlapping
  // ones. Returns indices of the surviving ones.
  // NOTE: Assumes that the candidates are sorted according to their position in
  // the span.
  bool ResolveConflicts(const std::vector<AnnotatedSpan>& candidates,
                        const std::string& context,
                        const std::vector<Token>& cached_tokens,
                        const std::vector<Locale>& detected_text_language_tags,
                        AnnotationUsecase annotation_usecase,
                        InterpreterManager* interpreter_manager,
                        std::vector<int>* result) const;

  // Resolves one conflict between candidates on indices 'start_index'
  // (inclusive) and 'end_index' (exclusive). Assigns the winning candidate
  // indices to 'chosen_indices'. Returns false if a problem arises.
  bool ResolveConflict(const std::string& context,
                       const std::vector<Token>& cached_tokens,
                       const std::vector<AnnotatedSpan>& candidates,
                       const std::vector<Locale>& detected_text_language_tags,
                       int start_index, int end_index,
                       AnnotationUsecase annotation_usecase,
                       InterpreterManager* interpreter_manager,
                       std::vector<int>* chosen_indices) const;

  // Gets selection candidates from the ML model.
  // Provides the tokens produced during tokenization of the context string for
  // reuse.
  bool ModelSuggestSelection(
      const UnicodeText& context_unicode, CodepointSpan click_indices,
      const std::vector<Locale>& detected_text_language_tags,
      InterpreterManager* interpreter_manager, std::vector<Token>* tokens,
      std::vector<AnnotatedSpan>* result) const;

  // Classifies the selected text given the context string with the
  // classification model.
  // Returns true if no error occurred.
  bool ModelClassifyText(
      const std::string& context, const std::vector<Token>& cached_tokens,
      const std::vector<Locale>& locales, CodepointSpan selection_indices,
      InterpreterManager* interpreter_manager,
      FeatureProcessor::EmbeddingCache* embedding_cache,
      std::vector<ClassificationResult>* classification_results,
      std::vector<Token>* tokens) const;

  // Same as above but doesn't output tokens.
  bool ModelClassifyText(
      const std::string& context, const std::vector<Token>& cached_tokens,
      const std::vector<Locale>& detected_text_language_tags,
      CodepointSpan selection_indices, InterpreterManager* interpreter_manager,
      FeatureProcessor::EmbeddingCache* embedding_cache,
      std::vector<ClassificationResult>* classification_results) const;

  // Same as above but doesn't take cached tokens and doesn't output tokens.
  bool ModelClassifyText(
      const std::string& context,
      const std::vector<Locale>& detected_text_language_tags,
      CodepointSpan selection_indices, InterpreterManager* interpreter_manager,
      FeatureProcessor::EmbeddingCache* embedding_cache,
      std::vector<ClassificationResult>* classification_results) const;

  // Returns a relative token span that represents how many tokens on the left
  // from the selection and right from the selection are needed for the
  // classifier input.
  TokenSpan ClassifyTextUpperBoundNeededTokens() const;

  // Classifies the selected text with the regular expressions models.
  // Returns true if no error happened, false otherwise.
  bool RegexClassifyText(
      const std::string& context, CodepointSpan selection_indices,
      std::vector<ClassificationResult>* classification_result) const;

  // Classifies the selected text with the date time model.
  // Returns true if no error happened, false otherwise.
  bool DatetimeClassifyText(
      const std::string& context, CodepointSpan selection_indices,
      const ClassificationOptions& options,
      std::vector<ClassificationResult>* classification_results) const;

  // Chunks given input text with the selection model and classifies the spans
  // with the classification model.
  // The annotations are sorted by their position in the context string and
  // exclude spans classified as 'other'.
  // Provides the tokens produced during tokenization of the context string for
  // reuse.
  bool ModelAnnotate(const std::string& context,
                     const std::vector<Locale>& detected_text_language_tags,
                     InterpreterManager* interpreter_manager,
                     std::vector<Token>* tokens,
                     std::vector<AnnotatedSpan>* result) const;

  // Groups the tokens into chunks. A chunk is a token span that should be the
  // suggested selection when any of its contained tokens is clicked. The chunks
  // are non-overlapping and are sorted by their position in the context string.
  // "num_tokens" is the total number of tokens available (as this method does
  // not need the actual vector of tokens).
  // "span_of_interest" is a span of all the tokens that could be clicked.
  // The resulting chunks all have to overlap with it and they cover this span
  // completely. The first and last chunk might extend beyond it.
  // The chunks vector is cleared before filling.
  bool ModelChunk(int num_tokens, const TokenSpan& span_of_interest,
                  tflite::Interpreter* selection_interpreter,
                  const CachedFeatures& cached_features,
                  std::vector<TokenSpan>* chunks) const;

  // A helper method for ModelChunk(). It generates scored chunk candidates for
  // a click context model.
  // NOTE: The returned chunks can (and most likely do) overlap.
  bool ModelClickContextScoreChunks(
      int num_tokens, const TokenSpan& span_of_interest,
      const CachedFeatures& cached_features,
      tflite::Interpreter* selection_interpreter,
      std::vector<ScoredChunk>* scored_chunks) const;

  // A helper method for ModelChunk(). It generates scored chunk candidates for
  // a bounds-sensitive model.
  // NOTE: The returned chunks can (and most likely do) overlap.
  bool ModelBoundsSensitiveScoreChunks(
      int num_tokens, const TokenSpan& span_of_interest,
      const TokenSpan& inference_span, const CachedFeatures& cached_features,
      tflite::Interpreter* selection_interpreter,
      std::vector<ScoredChunk>* scored_chunks) const;

  // Produces chunks isolated by a set of regular expressions.
  bool RegexChunk(const UnicodeText& context_unicode,
                  const std::vector<int>& rules,
                  std::vector<AnnotatedSpan>* result,
                  bool is_serialized_entity_data_enabled) const;

  // Produces chunks from the datetime parser.
  bool DatetimeChunk(const UnicodeText& context_unicode,
                     int64 reference_time_ms_utc,
                     const std::string& reference_timezone,
                     const std::string& locales, ModeFlag mode,
                     AnnotationUsecase annotation_usecase,
                     bool is_serialized_entity_data_enabled,
                     std::vector<AnnotatedSpan>* result) const;

  // Returns whether a classification should be filtered.
  bool FilteredForAnnotation(const AnnotatedSpan& span) const;
  bool FilteredForClassification(
      const ClassificationResult& classification) const;
  bool FilteredForSelection(const AnnotatedSpan& span) const;

  // Computes the selection boundaries from a regular expression match.
  CodepointSpan ComputeSelectionBoundaries(
      const UniLib::RegexMatcher* match,
      const RegexModel_::Pattern* config) const;

  // Returns whether a regex pattern provides entity data from a match.
  bool HasEntityData(const RegexModel_::Pattern* pattern) const;

  // Constructs and serializes entity data from regex matches.
  bool SerializedEntityDataFromRegexMatch(
      const RegexModel_::Pattern* pattern, UniLib::RegexMatcher* matcher,
      std::string* serialized_entity_data) const;

  // For knowledge candidates which have a ContactPointer, fill in the
  // appropriate contact metadata, if possible.
  void AddContactMetadataToKnowledgeClassificationResults(
      std::vector<AnnotatedSpan>* candidates) const;

  // Gets priority score from the list of classification results.
  float GetPriorityScore(
      const std::vector<ClassificationResult>& classification) const;

  // Verifies a regex match and returns true if verification was successful.
  bool VerifyRegexMatchCandidate(
      const std::string& context,
      const VerificationOptions* verification_options, const std::string& match,
      const UniLib::RegexMatcher* matcher) const;

  const Model* model_;

  std::unique_ptr<const ModelExecutor> selection_executor_;
  std::unique_ptr<const ModelExecutor> classification_executor_;
  std::unique_ptr<const EmbeddingExecutor> embedding_executor_;

  std::unique_ptr<const FeatureProcessor> selection_feature_processor_;
  std::unique_ptr<const FeatureProcessor> classification_feature_processor_;

  std::unique_ptr<const DatetimeParser> datetime_parser_;
  std::unique_ptr<const dates::CfgDatetimeAnnotator> cfg_datetime_parser_;

  std::unique_ptr<const GrammarAnnotator> grammar_annotator_;

 private:
  struct CompiledRegexPattern {
    const RegexModel_::Pattern* config;
    std::unique_ptr<UniLib::RegexPattern> pattern;
  };

  // Removes annotations the entity type of which is not in the set of enabled
  // entity types.
  void RemoveNotEnabledEntityTypes(
      const EnabledEntityTypes& is_entity_type_enabled,
      std::vector<AnnotatedSpan>* annotated_spans) const;

  // Runs only annotators that do not support structured input. Does conflict
  // resolution, removal of disallowed entities and sorting on both new
  // generated candidates and passed in entities.
  // Returns Status::Error if the annotation failed, in which case the vector of
  // candidates should be ignored.
  Status AnnotateSingleInput(const std::string& context,
                             const AnnotationOptions& options,
                             std::vector<AnnotatedSpan>* candidates) const;

  // Parses the money amount into whole and decimal part and fills in the
  // entity data information.
  bool ParseAndFillInMoneyAmount(std::string* serialized_entity_data) const;

  std::unique_ptr<ScopedMmap> mmap_;
  bool initialized_ = false;
  bool enabled_for_annotation_ = false;
  bool enabled_for_classification_ = false;
  bool enabled_for_selection_ = false;
  std::unordered_set<std::string> filtered_collections_annotation_;
  std::unordered_set<std::string> filtered_collections_classification_;
  std::unordered_set<std::string> filtered_collections_selection_;

  std::vector<CompiledRegexPattern> regex_patterns_;

  // Indices into regex_patterns_ for the different modes.
  std::vector<int> annotation_regex_patterns_, classification_regex_patterns_,
      selection_regex_patterns_;

  std::unique_ptr<UniLib> owned_unilib_;
  const UniLib* unilib_;
  std::unique_ptr<CalendarLib> owned_calendarlib_;
  const CalendarLib* calendarlib_;

  std::unique_ptr<const KnowledgeEngine> knowledge_engine_;
  std::unique_ptr<const ContactEngine> contact_engine_;
  std::unique_ptr<const InstalledAppEngine> installed_app_engine_;
  std::unique_ptr<const NumberAnnotator> number_annotator_;
  std::unique_ptr<const DurationAnnotator> duration_annotator_;
  std::unique_ptr<const PersonNameEngine> person_name_engine_;
  std::unique_ptr<const TranslateAnnotator> translate_annotator_;
  std::unique_ptr<const ExperimentalAnnotator> experimental_annotator_;

  // Builder for creating extra data.
  const reflection::Schema* entity_data_schema_;
  std::unique_ptr<ReflectiveFlatbufferBuilder> entity_data_builder_;

  // Locales for which the entire model triggers.
  std::vector<Locale> model_triggering_locales_;

  // Locales for which the ML model triggers.
  std::vector<Locale> ml_model_triggering_locales_;

  // Locales that the dictionary classification support.
  std::vector<Locale> dictionary_locales_;

  // Decimal and thousands number separators.
  std::unordered_set<char32> money_separators_;

  // Model for language identification.
  const libtextclassifier3::mobile::lang_id::LangId* lang_id_ = nullptr;

  // If true, will prioritize the longest annotation during conflict resolution.
  bool prioritize_longest_annotation_ = false;

  // If true, the annotator will perform conflict resolution between the
  // different sub-annotators also in the RAW mode. If false, no conflict
  // resolution will be performed in RAW mode.
  bool do_conflict_resolution_in_raw_mode_ = true;
};

namespace internal {

// Helper function, which if the initial 'span' contains only white-spaces,
// moves the selection to a single-codepoint selection on the left side
// of this block of white-space.
CodepointSpan SnapLeftIfWhitespaceSelection(CodepointSpan span,
                                            const UnicodeText& context_unicode,
                                            const UniLib& unilib);

// Copies tokens from 'cached_tokens' that are
// 'tokens_around_selection_to_copy' (on the left, and right) tokens distant
// from the tokens that correspond to 'selection_indices'.
std::vector<Token> CopyCachedTokens(const std::vector<Token>& cached_tokens,
                                    CodepointSpan selection_indices,
                                    TokenSpan tokens_around_selection_to_copy);
}  // namespace internal

// Interprets the buffer as a Model flatbuffer and returns it for reading.
const Model* ViewModel(const void* buffer, int size);

// Opens model from given path and runs a function, passing the loaded Model
// flatbuffer as an argument.
//
// This is mainly useful if we don't want to pay the cost for the model
// initialization because we'll be only reading some flatbuffer values from the
// file.
template <typename ReturnType, typename Func>
ReturnType VisitAnnotatorModel(const std::string& path, Func function) {
  ScopedMmap mmap(path);
  if (!mmap.handle().ok()) {
    function(/*model=*/nullptr);
  }
  const Model* model =
      ViewModel(mmap.handle().start(), mmap.handle().num_bytes());
  return function(model);
}

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_ANNOTATOR_H_
