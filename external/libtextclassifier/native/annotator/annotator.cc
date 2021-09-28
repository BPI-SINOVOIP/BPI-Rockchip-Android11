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

#include "annotator/annotator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

#include "annotator/collections.h"
#include "annotator/model_generated.h"
#include "annotator/types.h"
#include "utils/base/logging.h"
#include "utils/base/status.h"
#include "utils/base/statusor.h"
#include "utils/checksum.h"
#include "utils/i18n/locale.h"
#include "utils/math/softmax.h"
#include "utils/normalization.h"
#include "utils/optional.h"
#include "utils/regex-match.h"
#include "utils/strings/numbers.h"
#include "utils/strings/split.h"
#include "utils/utf8/unicodetext.h"
#include "utils/utf8/unilib-common.h"
#include "utils/zlib/zlib_regex.h"

namespace libtextclassifier3 {

using SortedIntSet = std::set<int, std::function<bool(int, int)>>;

const std::string& Annotator::kPhoneCollection =
    *[]() { return new std::string("phone"); }();
const std::string& Annotator::kAddressCollection =
    *[]() { return new std::string("address"); }();
const std::string& Annotator::kDateCollection =
    *[]() { return new std::string("date"); }();
const std::string& Annotator::kUrlCollection =
    *[]() { return new std::string("url"); }();
const std::string& Annotator::kEmailCollection =
    *[]() { return new std::string("email"); }();

namespace {
const Model* LoadAndVerifyModel(const void* addr, int size) {
  flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t*>(addr), size);
  if (VerifyModelBuffer(verifier)) {
    return GetModel(addr);
  } else {
    return nullptr;
  }
}

const PersonNameModel* LoadAndVerifyPersonNameModel(const void* addr,
                                                    int size) {
  flatbuffers::Verifier verifier(reinterpret_cast<const uint8_t*>(addr), size);
  if (VerifyPersonNameModelBuffer(verifier)) {
    return GetPersonNameModel(addr);
  } else {
    return nullptr;
  }
}

// If lib is not nullptr, just returns lib. Otherwise, if lib is nullptr, will
// create a new instance, assign ownership to owned_lib, and return it.
const UniLib* MaybeCreateUnilib(const UniLib* lib,
                                std::unique_ptr<UniLib>* owned_lib) {
  if (lib) {
    return lib;
  } else {
    owned_lib->reset(new UniLib);
    return owned_lib->get();
  }
}

// As above, but for CalendarLib.
const CalendarLib* MaybeCreateCalendarlib(
    const CalendarLib* lib, std::unique_ptr<CalendarLib>* owned_lib) {
  if (lib) {
    return lib;
  } else {
    owned_lib->reset(new CalendarLib);
    return owned_lib->get();
  }
}

// Returns whether the provided input is valid:
//   * Valid utf8 text.
//   * Sane span indices.
bool IsValidSpanInput(const UnicodeText& context, const CodepointSpan span) {
  if (!context.is_valid()) {
    return false;
  }
  return (span.first >= 0 && span.first < span.second &&
          span.second <= context.size_codepoints());
}

std::unordered_set<char32> FlatbuffersIntVectorToChar32UnorderedSet(
    const flatbuffers::Vector<int32_t>* ints) {
  if (ints == nullptr) {
    return {};
  }
  std::unordered_set<char32> ints_set;
  for (auto value : *ints) {
    ints_set.insert(static_cast<char32>(value));
  }
  return ints_set;
}

DateAnnotationOptions ToDateAnnotationOptions(
    const GrammarDatetimeModel_::AnnotationOptions* fb_annotation_options,
    const std::string& reference_timezone, const int64 reference_time_ms_utc) {
  DateAnnotationOptions result_annotation_options;
  result_annotation_options.base_timestamp_millis = reference_time_ms_utc;
  result_annotation_options.reference_timezone = reference_timezone;
  if (fb_annotation_options != nullptr) {
    result_annotation_options.enable_special_day_offset =
        fb_annotation_options->enable_special_day_offset();
    result_annotation_options.merge_adjacent_components =
        fb_annotation_options->merge_adjacent_components();
    result_annotation_options.enable_date_range =
        fb_annotation_options->enable_date_range();
    result_annotation_options.include_preposition =
        fb_annotation_options->include_preposition();
    if (fb_annotation_options->extra_requested_dates() != nullptr) {
      for (const auto& extra_requested_date :
           *fb_annotation_options->extra_requested_dates()) {
        result_annotation_options.extra_requested_dates.push_back(
            extra_requested_date->str());
      }
    }
    if (fb_annotation_options->ignored_spans() != nullptr) {
      for (const auto& ignored_span : *fb_annotation_options->ignored_spans()) {
        result_annotation_options.ignored_spans.push_back(ignored_span->str());
      }
    }
  }
  return result_annotation_options;
}

}  // namespace

tflite::Interpreter* InterpreterManager::SelectionInterpreter() {
  if (!selection_interpreter_) {
    TC3_CHECK(selection_executor_);
    selection_interpreter_ = selection_executor_->CreateInterpreter();
    if (!selection_interpreter_) {
      TC3_LOG(ERROR) << "Could not build TFLite interpreter.";
    }
  }
  return selection_interpreter_.get();
}

tflite::Interpreter* InterpreterManager::ClassificationInterpreter() {
  if (!classification_interpreter_) {
    TC3_CHECK(classification_executor_);
    classification_interpreter_ = classification_executor_->CreateInterpreter();
    if (!classification_interpreter_) {
      TC3_LOG(ERROR) << "Could not build TFLite interpreter.";
    }
  }
  return classification_interpreter_.get();
}

std::unique_ptr<Annotator> Annotator::FromUnownedBuffer(
    const char* buffer, int size, const UniLib* unilib,
    const CalendarLib* calendarlib) {
  const Model* model = LoadAndVerifyModel(buffer, size);
  if (model == nullptr) {
    return nullptr;
  }

  auto classifier =
      std::unique_ptr<Annotator>(new Annotator(model, unilib, calendarlib));
  if (!classifier->IsInitialized()) {
    return nullptr;
  }

  return classifier;
}

std::unique_ptr<Annotator> Annotator::FromScopedMmap(
    std::unique_ptr<ScopedMmap>* mmap, const UniLib* unilib,
    const CalendarLib* calendarlib) {
  if (!(*mmap)->handle().ok()) {
    TC3_VLOG(1) << "Mmap failed.";
    return nullptr;
  }

  const Model* model = LoadAndVerifyModel((*mmap)->handle().start(),
                                          (*mmap)->handle().num_bytes());
  if (!model) {
    TC3_LOG(ERROR) << "Model verification failed.";
    return nullptr;
  }

  auto classifier = std::unique_ptr<Annotator>(
      new Annotator(mmap, model, unilib, calendarlib));
  if (!classifier->IsInitialized()) {
    return nullptr;
  }

  return classifier;
}

std::unique_ptr<Annotator> Annotator::FromScopedMmap(
    std::unique_ptr<ScopedMmap>* mmap, std::unique_ptr<UniLib> unilib,
    std::unique_ptr<CalendarLib> calendarlib) {
  if (!(*mmap)->handle().ok()) {
    TC3_VLOG(1) << "Mmap failed.";
    return nullptr;
  }

  const Model* model = LoadAndVerifyModel((*mmap)->handle().start(),
                                          (*mmap)->handle().num_bytes());
  if (model == nullptr) {
    TC3_LOG(ERROR) << "Model verification failed.";
    return nullptr;
  }

  auto classifier = std::unique_ptr<Annotator>(
      new Annotator(mmap, model, std::move(unilib), std::move(calendarlib)));
  if (!classifier->IsInitialized()) {
    return nullptr;
  }

  return classifier;
}

std::unique_ptr<Annotator> Annotator::FromFileDescriptor(
    int fd, int offset, int size, const UniLib* unilib,
    const CalendarLib* calendarlib) {
  std::unique_ptr<ScopedMmap> mmap(new ScopedMmap(fd, offset, size));
  return FromScopedMmap(&mmap, unilib, calendarlib);
}

std::unique_ptr<Annotator> Annotator::FromFileDescriptor(
    int fd, int offset, int size, std::unique_ptr<UniLib> unilib,
    std::unique_ptr<CalendarLib> calendarlib) {
  std::unique_ptr<ScopedMmap> mmap(new ScopedMmap(fd, offset, size));
  return FromScopedMmap(&mmap, std::move(unilib), std::move(calendarlib));
}

std::unique_ptr<Annotator> Annotator::FromFileDescriptor(
    int fd, const UniLib* unilib, const CalendarLib* calendarlib) {
  std::unique_ptr<ScopedMmap> mmap(new ScopedMmap(fd));
  return FromScopedMmap(&mmap, unilib, calendarlib);
}

std::unique_ptr<Annotator> Annotator::FromFileDescriptor(
    int fd, std::unique_ptr<UniLib> unilib,
    std::unique_ptr<CalendarLib> calendarlib) {
  std::unique_ptr<ScopedMmap> mmap(new ScopedMmap(fd));
  return FromScopedMmap(&mmap, std::move(unilib), std::move(calendarlib));
}

std::unique_ptr<Annotator> Annotator::FromPath(const std::string& path,
                                               const UniLib* unilib,
                                               const CalendarLib* calendarlib) {
  std::unique_ptr<ScopedMmap> mmap(new ScopedMmap(path));
  return FromScopedMmap(&mmap, unilib, calendarlib);
}

std::unique_ptr<Annotator> Annotator::FromPath(
    const std::string& path, std::unique_ptr<UniLib> unilib,
    std::unique_ptr<CalendarLib> calendarlib) {
  std::unique_ptr<ScopedMmap> mmap(new ScopedMmap(path));
  return FromScopedMmap(&mmap, std::move(unilib), std::move(calendarlib));
}

Annotator::Annotator(std::unique_ptr<ScopedMmap>* mmap, const Model* model,
                     const UniLib* unilib, const CalendarLib* calendarlib)
    : model_(model),
      mmap_(std::move(*mmap)),
      owned_unilib_(nullptr),
      unilib_(MaybeCreateUnilib(unilib, &owned_unilib_)),
      owned_calendarlib_(nullptr),
      calendarlib_(MaybeCreateCalendarlib(calendarlib, &owned_calendarlib_)) {
  ValidateAndInitialize();
}

Annotator::Annotator(std::unique_ptr<ScopedMmap>* mmap, const Model* model,
                     std::unique_ptr<UniLib> unilib,
                     std::unique_ptr<CalendarLib> calendarlib)
    : model_(model),
      mmap_(std::move(*mmap)),
      owned_unilib_(std::move(unilib)),
      unilib_(owned_unilib_.get()),
      owned_calendarlib_(std::move(calendarlib)),
      calendarlib_(owned_calendarlib_.get()) {
  ValidateAndInitialize();
}

Annotator::Annotator(const Model* model, const UniLib* unilib,
                     const CalendarLib* calendarlib)
    : model_(model),
      owned_unilib_(nullptr),
      unilib_(MaybeCreateUnilib(unilib, &owned_unilib_)),
      owned_calendarlib_(nullptr),
      calendarlib_(MaybeCreateCalendarlib(calendarlib, &owned_calendarlib_)) {
  ValidateAndInitialize();
}

void Annotator::ValidateAndInitialize() {
  initialized_ = false;

  if (model_ == nullptr) {
    TC3_LOG(ERROR) << "No model specified.";
    return;
  }

  const bool model_enabled_for_annotation =
      (model_->triggering_options() != nullptr &&
       (model_->triggering_options()->enabled_modes() & ModeFlag_ANNOTATION));
  const bool model_enabled_for_classification =
      (model_->triggering_options() != nullptr &&
       (model_->triggering_options()->enabled_modes() &
        ModeFlag_CLASSIFICATION));
  const bool model_enabled_for_selection =
      (model_->triggering_options() != nullptr &&
       (model_->triggering_options()->enabled_modes() & ModeFlag_SELECTION));

  // Annotation requires the selection model.
  if (model_enabled_for_annotation || model_enabled_for_selection) {
    if (!model_->selection_options()) {
      TC3_LOG(ERROR) << "No selection options.";
      return;
    }
    if (!model_->selection_feature_options()) {
      TC3_LOG(ERROR) << "No selection feature options.";
      return;
    }
    if (!model_->selection_feature_options()->bounds_sensitive_features()) {
      TC3_LOG(ERROR) << "No selection bounds sensitive feature options.";
      return;
    }
    if (!model_->selection_model()) {
      TC3_LOG(ERROR) << "No selection model.";
      return;
    }
    selection_executor_ = ModelExecutor::FromBuffer(model_->selection_model());
    if (!selection_executor_) {
      TC3_LOG(ERROR) << "Could not initialize selection executor.";
      return;
    }
    selection_feature_processor_.reset(
        new FeatureProcessor(model_->selection_feature_options(), unilib_));
  }

  // Annotation requires the classification model for conflict resolution and
  // scoring.
  // Selection requires the classification model for conflict resolution.
  if (model_enabled_for_annotation || model_enabled_for_classification ||
      model_enabled_for_selection) {
    if (!model_->classification_options()) {
      TC3_LOG(ERROR) << "No classification options.";
      return;
    }

    if (!model_->classification_feature_options()) {
      TC3_LOG(ERROR) << "No classification feature options.";
      return;
    }

    if (!model_->classification_feature_options()
             ->bounds_sensitive_features()) {
      TC3_LOG(ERROR) << "No classification bounds sensitive feature options.";
      return;
    }
    if (!model_->classification_model()) {
      TC3_LOG(ERROR) << "No clf model.";
      return;
    }

    classification_executor_ =
        ModelExecutor::FromBuffer(model_->classification_model());
    if (!classification_executor_) {
      TC3_LOG(ERROR) << "Could not initialize classification executor.";
      return;
    }

    classification_feature_processor_.reset(new FeatureProcessor(
        model_->classification_feature_options(), unilib_));
  }

  // The embeddings need to be specified if the model is to be used for
  // classification or selection.
  if (model_enabled_for_annotation || model_enabled_for_classification ||
      model_enabled_for_selection) {
    if (!model_->embedding_model()) {
      TC3_LOG(ERROR) << "No embedding model.";
      return;
    }

    // Check that the embedding size of the selection and classification model
    // matches, as they are using the same embeddings.
    if (model_enabled_for_selection &&
        (model_->selection_feature_options()->embedding_size() !=
             model_->classification_feature_options()->embedding_size() ||
         model_->selection_feature_options()->embedding_quantization_bits() !=
             model_->classification_feature_options()
                 ->embedding_quantization_bits())) {
      TC3_LOG(ERROR) << "Mismatching embedding size/quantization.";
      return;
    }

    embedding_executor_ = TFLiteEmbeddingExecutor::FromBuffer(
        model_->embedding_model(),
        model_->classification_feature_options()->embedding_size(),
        model_->classification_feature_options()->embedding_quantization_bits(),
        model_->embedding_pruning_mask());
    if (!embedding_executor_) {
      TC3_LOG(ERROR) << "Could not initialize embedding executor.";
      return;
    }
  }

  std::unique_ptr<ZlibDecompressor> decompressor = ZlibDecompressor::Instance();
  if (model_->regex_model()) {
    if (!InitializeRegexModel(decompressor.get())) {
      TC3_LOG(ERROR) << "Could not initialize regex model.";
      return;
    }
  }
  if (model_->grammar_datetime_model() &&
      model_->grammar_datetime_model()->datetime_rules()) {
    cfg_datetime_parser_.reset(new dates::CfgDatetimeAnnotator(
        unilib_,
        /*tokenizer_options=*/
        model_->grammar_datetime_model()->grammar_tokenizer_options(),
        calendarlib_,
        /*datetime_rules=*/model_->grammar_datetime_model()->datetime_rules(),
        model_->grammar_datetime_model()->target_classification_score(),
        model_->grammar_datetime_model()->priority_score()));
    if (!cfg_datetime_parser_) {
      TC3_LOG(ERROR) << "Could not initialize context free grammar based "
                        "datetime parser.";
      return;
    }
  }

  if (model_->datetime_model()) {
    datetime_parser_ = DatetimeParser::Instance(
        model_->datetime_model(), unilib_, calendarlib_, decompressor.get());
    if (!datetime_parser_) {
      TC3_LOG(ERROR) << "Could not initialize datetime parser.";
      return;
    }
  }

  if (model_->output_options()) {
    if (model_->output_options()->filtered_collections_annotation()) {
      for (const auto collection :
           *model_->output_options()->filtered_collections_annotation()) {
        filtered_collections_annotation_.insert(collection->str());
      }
    }
    if (model_->output_options()->filtered_collections_classification()) {
      for (const auto collection :
           *model_->output_options()->filtered_collections_classification()) {
        filtered_collections_classification_.insert(collection->str());
      }
    }
    if (model_->output_options()->filtered_collections_selection()) {
      for (const auto collection :
           *model_->output_options()->filtered_collections_selection()) {
        filtered_collections_selection_.insert(collection->str());
      }
    }
  }

  if (model_->number_annotator_options() &&
      model_->number_annotator_options()->enabled()) {
    number_annotator_.reset(
        new NumberAnnotator(model_->number_annotator_options(), unilib_));
  }

  if (model_->money_parsing_options()) {
    money_separators_ = FlatbuffersIntVectorToChar32UnorderedSet(
        model_->money_parsing_options()->separators());
  }

  if (model_->duration_annotator_options() &&
      model_->duration_annotator_options()->enabled()) {
    duration_annotator_.reset(
        new DurationAnnotator(model_->duration_annotator_options(),
                              selection_feature_processor_.get(), unilib_));
  }

  if (model_->entity_data_schema()) {
    entity_data_schema_ = LoadAndVerifyFlatbuffer<reflection::Schema>(
        model_->entity_data_schema()->Data(),
        model_->entity_data_schema()->size());
    if (entity_data_schema_ == nullptr) {
      TC3_LOG(ERROR) << "Could not load entity data schema data.";
      return;
    }

    entity_data_builder_.reset(
        new ReflectiveFlatbufferBuilder(entity_data_schema_));
  } else {
    entity_data_schema_ = nullptr;
    entity_data_builder_ = nullptr;
  }

  if (model_->grammar_model()) {
    grammar_annotator_.reset(new GrammarAnnotator(
        unilib_, model_->grammar_model(), entity_data_builder_.get()));
  }

  if (model_->triggering_locales() &&
      !ParseLocales(model_->triggering_locales()->c_str(),
                    &model_triggering_locales_)) {
    TC3_LOG(ERROR) << "Could not parse model supported locales.";
    return;
  }

  if (model_->triggering_options() != nullptr &&
      model_->triggering_options()->locales() != nullptr &&
      !ParseLocales(model_->triggering_options()->locales()->c_str(),
                    &ml_model_triggering_locales_)) {
    TC3_LOG(ERROR) << "Could not parse supported ML model locales.";
    return;
  }

  if (model_->triggering_options() != nullptr &&
      model_->triggering_options()->dictionary_locales() != nullptr &&
      !ParseLocales(model_->triggering_options()->dictionary_locales()->c_str(),
                    &dictionary_locales_)) {
    TC3_LOG(ERROR) << "Could not parse dictionary supported locales.";
    return;
  }

  if (model_->conflict_resolution_options() != nullptr) {
    prioritize_longest_annotation_ =
        model_->conflict_resolution_options()->prioritize_longest_annotation();
    do_conflict_resolution_in_raw_mode_ =
        model_->conflict_resolution_options()
            ->do_conflict_resolution_in_raw_mode();
  }

#ifdef TC3_EXPERIMENTAL
  TC3_LOG(WARNING) << "Enabling experimental annotators.";
  InitializeExperimentalAnnotators();
#endif

  initialized_ = true;
}

bool Annotator::InitializeRegexModel(ZlibDecompressor* decompressor) {
  if (!model_->regex_model()->patterns()) {
    return true;
  }

  // Initialize pattern recognizers.
  int regex_pattern_id = 0;
  for (const auto regex_pattern : *model_->regex_model()->patterns()) {
    std::unique_ptr<UniLib::RegexPattern> compiled_pattern =
        UncompressMakeRegexPattern(
            *unilib_, regex_pattern->pattern(),
            regex_pattern->compressed_pattern(),
            model_->regex_model()->lazy_regex_compilation(), decompressor);
    if (!compiled_pattern) {
      TC3_LOG(INFO) << "Failed to load regex pattern";
      return false;
    }

    if (regex_pattern->enabled_modes() & ModeFlag_ANNOTATION) {
      annotation_regex_patterns_.push_back(regex_pattern_id);
    }
    if (regex_pattern->enabled_modes() & ModeFlag_CLASSIFICATION) {
      classification_regex_patterns_.push_back(regex_pattern_id);
    }
    if (regex_pattern->enabled_modes() & ModeFlag_SELECTION) {
      selection_regex_patterns_.push_back(regex_pattern_id);
    }
    regex_patterns_.push_back({
        regex_pattern,
        std::move(compiled_pattern),
    });
    ++regex_pattern_id;
  }

  return true;
}

bool Annotator::InitializeKnowledgeEngine(
    const std::string& serialized_config) {
  std::unique_ptr<KnowledgeEngine> knowledge_engine(new KnowledgeEngine());
  if (!knowledge_engine->Initialize(serialized_config, unilib_)) {
    TC3_LOG(ERROR) << "Failed to initialize the knowledge engine.";
    return false;
  }
  if (model_->triggering_options() != nullptr) {
    knowledge_engine->SetPriorityScore(
        model_->triggering_options()->knowledge_priority_score());
  }
  knowledge_engine_ = std::move(knowledge_engine);
  return true;
}

bool Annotator::InitializeContactEngine(const std::string& serialized_config) {
  std::unique_ptr<ContactEngine> contact_engine(
      new ContactEngine(selection_feature_processor_.get(), unilib_,
                        model_->contact_annotator_options()));
  if (!contact_engine->Initialize(serialized_config)) {
    TC3_LOG(ERROR) << "Failed to initialize the contact engine.";
    return false;
  }
  contact_engine_ = std::move(contact_engine);
  return true;
}

bool Annotator::InitializeInstalledAppEngine(
    const std::string& serialized_config) {
  std::unique_ptr<InstalledAppEngine> installed_app_engine(
      new InstalledAppEngine(selection_feature_processor_.get(), unilib_));
  if (!installed_app_engine->Initialize(serialized_config)) {
    TC3_LOG(ERROR) << "Failed to initialize the installed app engine.";
    return false;
  }
  installed_app_engine_ = std::move(installed_app_engine);
  return true;
}

void Annotator::SetLangId(const libtextclassifier3::mobile::lang_id::LangId* lang_id) {
  lang_id_ = lang_id;
  if (lang_id_ != nullptr && model_->translate_annotator_options() &&
      model_->translate_annotator_options()->enabled()) {
    translate_annotator_.reset(new TranslateAnnotator(
        model_->translate_annotator_options(), lang_id_, unilib_));
  } else {
    translate_annotator_.reset(nullptr);
  }
}

bool Annotator::InitializePersonNameEngineFromUnownedBuffer(const void* buffer,
                                                            int size) {
  const PersonNameModel* person_name_model =
      LoadAndVerifyPersonNameModel(buffer, size);

  if (person_name_model == nullptr) {
    TC3_LOG(ERROR) << "Person name model verification failed.";
    return false;
  }

  if (!person_name_model->enabled()) {
    return true;
  }

  std::unique_ptr<PersonNameEngine> person_name_engine(
      new PersonNameEngine(selection_feature_processor_.get(), unilib_));
  if (!person_name_engine->Initialize(person_name_model)) {
    TC3_LOG(ERROR) << "Failed to initialize the person name engine.";
    return false;
  }
  person_name_engine_ = std::move(person_name_engine);
  return true;
}

bool Annotator::InitializePersonNameEngineFromScopedMmap(
    const ScopedMmap& mmap) {
  if (!mmap.handle().ok()) {
    TC3_LOG(ERROR) << "Mmap for person name model failed.";
    return false;
  }

  return InitializePersonNameEngineFromUnownedBuffer(mmap.handle().start(),
                                                     mmap.handle().num_bytes());
}

bool Annotator::InitializePersonNameEngineFromPath(const std::string& path) {
  std::unique_ptr<ScopedMmap> mmap(new ScopedMmap(path));
  return InitializePersonNameEngineFromScopedMmap(*mmap);
}

bool Annotator::InitializePersonNameEngineFromFileDescriptor(int fd, int offset,
                                                             int size) {
  std::unique_ptr<ScopedMmap> mmap(new ScopedMmap(fd, offset, size));
  return InitializePersonNameEngineFromScopedMmap(*mmap);
}

bool Annotator::InitializeExperimentalAnnotators() {
  if (ExperimentalAnnotator::IsEnabled()) {
    experimental_annotator_.reset(new ExperimentalAnnotator(
        model_->experimental_model(), *selection_feature_processor_, *unilib_));
    return true;
  }
  return false;
}

namespace {

int CountDigits(const std::string& str, CodepointSpan selection_indices) {
  int count = 0;
  int i = 0;
  const UnicodeText unicode_str = UTF8ToUnicodeText(str, /*do_copy=*/false);
  for (auto it = unicode_str.begin(); it != unicode_str.end(); ++it, ++i) {
    if (i >= selection_indices.first && i < selection_indices.second &&
        IsDigit(*it)) {
      ++count;
    }
  }
  return count;
}

}  // namespace

namespace internal {
// Helper function, which if the initial 'span' contains only white-spaces,
// moves the selection to a single-codepoint selection on a left or right side
// of this space.
CodepointSpan SnapLeftIfWhitespaceSelection(CodepointSpan span,
                                            const UnicodeText& context_unicode,
                                            const UniLib& unilib) {
  TC3_CHECK(ValidNonEmptySpan(span));

  UnicodeText::const_iterator it;

  // Check that the current selection is all whitespaces.
  it = context_unicode.begin();
  std::advance(it, span.first);
  for (int i = 0; i < (span.second - span.first); ++i, ++it) {
    if (!unilib.IsWhitespace(*it)) {
      return span;
    }
  }

  CodepointSpan result;

  // Try moving left.
  result = span;
  it = context_unicode.begin();
  std::advance(it, span.first);
  while (it != context_unicode.begin() && unilib.IsWhitespace(*it)) {
    --result.first;
    --it;
  }
  result.second = result.first + 1;
  if (!unilib.IsWhitespace(*it)) {
    return result;
  }

  // If moving left didn't find a non-whitespace character, just return the
  // original span.
  return span;
}
}  // namespace internal

bool Annotator::FilteredForAnnotation(const AnnotatedSpan& span) const {
  return !span.classification.empty() &&
         filtered_collections_annotation_.find(
             span.classification[0].collection) !=
             filtered_collections_annotation_.end();
}

bool Annotator::FilteredForClassification(
    const ClassificationResult& classification) const {
  return filtered_collections_classification_.find(classification.collection) !=
         filtered_collections_classification_.end();
}

bool Annotator::FilteredForSelection(const AnnotatedSpan& span) const {
  return !span.classification.empty() &&
         filtered_collections_selection_.find(
             span.classification[0].collection) !=
             filtered_collections_selection_.end();
}

namespace {
inline bool ClassifiedAsOther(
    const std::vector<ClassificationResult>& classification) {
  return !classification.empty() &&
         classification[0].collection == Collections::Other();
}

}  // namespace

float Annotator::GetPriorityScore(
    const std::vector<ClassificationResult>& classification) const {
  if (!classification.empty() && !ClassifiedAsOther(classification)) {
    return classification[0].priority_score;
  } else {
    if (model_->triggering_options() != nullptr) {
      return model_->triggering_options()->other_collection_priority_score();
    } else {
      return -1000.0;
    }
  }
}

bool Annotator::VerifyRegexMatchCandidate(
    const std::string& context, const VerificationOptions* verification_options,
    const std::string& match, const UniLib::RegexMatcher* matcher) const {
  if (verification_options == nullptr) {
    return true;
  }
  if (verification_options->verify_luhn_checksum() &&
      !VerifyLuhnChecksum(match)) {
    return false;
  }
  const int lua_verifier = verification_options->lua_verifier();
  if (lua_verifier >= 0) {
    if (model_->regex_model()->lua_verifier() == nullptr ||
        lua_verifier >= model_->regex_model()->lua_verifier()->size()) {
      TC3_LOG(ERROR) << "Invalid lua verifier specified: " << lua_verifier;
      return false;
    }
    return VerifyMatch(
        context, matcher,
        model_->regex_model()->lua_verifier()->Get(lua_verifier)->str());
  }
  return true;
}

CodepointSpan Annotator::SuggestSelection(
    const std::string& context, CodepointSpan click_indices,
    const SelectionOptions& options) const {
  CodepointSpan original_click_indices = click_indices;
  if (!initialized_) {
    TC3_LOG(ERROR) << "Not initialized";
    return original_click_indices;
  }
  if (options.annotation_usecase !=
      AnnotationUsecase_ANNOTATION_USECASE_SMART) {
    TC3_LOG(WARNING)
        << "Invoking SuggestSelection, which is not supported in RAW mode.";
    return original_click_indices;
  }
  if (!(model_->enabled_modes() & ModeFlag_SELECTION)) {
    return original_click_indices;
  }

  std::vector<Locale> detected_text_language_tags;
  if (!ParseLocales(options.detected_text_language_tags,
                    &detected_text_language_tags)) {
    TC3_LOG(WARNING)
        << "Failed to parse the detected_text_language_tags in options: "
        << options.detected_text_language_tags;
  }
  if (!Locale::IsAnyLocaleSupported(detected_text_language_tags,
                                    model_triggering_locales_,
                                    /*default_value=*/true)) {
    return original_click_indices;
  }

  const UnicodeText context_unicode = UTF8ToUnicodeText(context,
                                                        /*do_copy=*/false);

  if (!IsValidSpanInput(context_unicode, click_indices)) {
    TC3_VLOG(1)
        << "Trying to run SuggestSelection with invalid input, indices: "
        << click_indices.first << " " << click_indices.second;
    return original_click_indices;
  }

  if (model_->snap_whitespace_selections()) {
    // We want to expand a purely white-space selection to a multi-selection it
    // would've been part of. But with this feature disabled we would do a no-
    // op, because no token is found. Therefore, we need to modify the
    // 'click_indices' a bit to include a part of the token, so that the click-
    // finding logic finds the clicked token correctly. This modification is
    // done by the following function. Note, that it's enough to check the left
    // side of the current selection, because if the white-space is a part of a
    // multi-selection, necessarily both tokens - on the left and the right
    // sides need to be selected. Thus snapping only to the left is sufficient
    // (there's a check at the bottom that makes sure that if we snap to the
    // left token but the result does not contain the initial white-space,
    // returns the original indices).
    click_indices = internal::SnapLeftIfWhitespaceSelection(
        click_indices, context_unicode, *unilib_);
  }

  std::vector<AnnotatedSpan> candidates;
  InterpreterManager interpreter_manager(selection_executor_.get(),
                                         classification_executor_.get());
  std::vector<Token> tokens;
  if (!ModelSuggestSelection(context_unicode, click_indices,
                             detected_text_language_tags, &interpreter_manager,
                             &tokens, &candidates)) {
    TC3_LOG(ERROR) << "Model suggest selection failed.";
    return original_click_indices;
  }
  if (!RegexChunk(context_unicode, selection_regex_patterns_, &candidates,
                  /*is_serialized_entity_data_enabled=*/false)) {
    TC3_LOG(ERROR) << "Regex suggest selection failed.";
    return original_click_indices;
  }
  if (!DatetimeChunk(
          UTF8ToUnicodeText(context, /*do_copy=*/false),
          /*reference_time_ms_utc=*/0, /*reference_timezone=*/"",
          options.locales, ModeFlag_SELECTION, options.annotation_usecase,
          /*is_serialized_entity_data_enabled=*/false, &candidates)) {
    TC3_LOG(ERROR) << "Datetime suggest selection failed.";
    return original_click_indices;
  }
  if (knowledge_engine_ != nullptr &&
      !knowledge_engine_->Chunk(context, options.annotation_usecase,
                                options.location_context, Permissions(),
                                &candidates)) {
    TC3_LOG(ERROR) << "Knowledge suggest selection failed.";
    return original_click_indices;
  }
  if (contact_engine_ != nullptr &&
      !contact_engine_->Chunk(context_unicode, tokens, &candidates)) {
    TC3_LOG(ERROR) << "Contact suggest selection failed.";
    return original_click_indices;
  }
  if (installed_app_engine_ != nullptr &&
      !installed_app_engine_->Chunk(context_unicode, tokens, &candidates)) {
    TC3_LOG(ERROR) << "Installed app suggest selection failed.";
    return original_click_indices;
  }
  if (number_annotator_ != nullptr &&
      !number_annotator_->FindAll(context_unicode, options.annotation_usecase,
                                  &candidates)) {
    TC3_LOG(ERROR) << "Number annotator failed in suggest selection.";
    return original_click_indices;
  }
  if (duration_annotator_ != nullptr &&
      !duration_annotator_->FindAll(context_unicode, tokens,
                                    options.annotation_usecase, &candidates)) {
    TC3_LOG(ERROR) << "Duration annotator failed in suggest selection.";
    return original_click_indices;
  }
  if (person_name_engine_ != nullptr &&
      !person_name_engine_->Chunk(context_unicode, tokens, &candidates)) {
    TC3_LOG(ERROR) << "Person name suggest selection failed.";
    return original_click_indices;
  }

  AnnotatedSpan grammar_suggested_span;
  if (grammar_annotator_ != nullptr &&
      grammar_annotator_->SuggestSelection(detected_text_language_tags,
                                           context_unicode, click_indices,
                                           &grammar_suggested_span)) {
    candidates.push_back(grammar_suggested_span);
  }

  if (experimental_annotator_ != nullptr) {
    candidates.push_back(experimental_annotator_->SuggestSelection(
        context_unicode, click_indices));
  }

  // Sort candidates according to their position in the input, so that the next
  // code can assume that any connected component of overlapping spans forms a
  // contiguous block.
  std::sort(candidates.begin(), candidates.end(),
            [](const AnnotatedSpan& a, const AnnotatedSpan& b) {
              return a.span.first < b.span.first;
            });

  std::vector<int> candidate_indices;
  if (!ResolveConflicts(candidates, context, tokens,
                        detected_text_language_tags, options.annotation_usecase,
                        &interpreter_manager, &candidate_indices)) {
    TC3_LOG(ERROR) << "Couldn't resolve conflicts.";
    return original_click_indices;
  }

  std::sort(candidate_indices.begin(), candidate_indices.end(),
            [this, &candidates](int a, int b) {
              return GetPriorityScore(candidates[a].classification) >
                     GetPriorityScore(candidates[b].classification);
            });

  for (const int i : candidate_indices) {
    if (SpansOverlap(candidates[i].span, click_indices) &&
        SpansOverlap(candidates[i].span, original_click_indices)) {
      // Run model classification if not present but requested and there's a
      // classification collection filter specified.
      if (candidates[i].classification.empty() &&
          model_->selection_options()->always_classify_suggested_selection() &&
          !filtered_collections_selection_.empty()) {
        if (!ModelClassifyText(context, detected_text_language_tags,
                               candidates[i].span, &interpreter_manager,
                               /*embedding_cache=*/nullptr,
                               &candidates[i].classification)) {
          return original_click_indices;
        }
      }

      // Ignore if span classification is filtered.
      if (FilteredForSelection(candidates[i])) {
        return original_click_indices;
      }

      return candidates[i].span;
    }
  }

  return original_click_indices;
}

namespace {
// Helper function that returns the index of the first candidate that
// transitively does not overlap with the candidate on 'start_index'. If the end
// of 'candidates' is reached, it returns the index that points right behind the
// array.
int FirstNonOverlappingSpanIndex(const std::vector<AnnotatedSpan>& candidates,
                                 int start_index) {
  int first_non_overlapping = start_index + 1;
  CodepointSpan conflicting_span = candidates[start_index].span;
  while (
      first_non_overlapping < candidates.size() &&
      SpansOverlap(conflicting_span, candidates[first_non_overlapping].span)) {
    // Grow the span to include the current one.
    conflicting_span.second = std::max(
        conflicting_span.second, candidates[first_non_overlapping].span.second);

    ++first_non_overlapping;
  }
  return first_non_overlapping;
}
}  // namespace

bool Annotator::ResolveConflicts(
    const std::vector<AnnotatedSpan>& candidates, const std::string& context,
    const std::vector<Token>& cached_tokens,
    const std::vector<Locale>& detected_text_language_tags,
    AnnotationUsecase annotation_usecase,
    InterpreterManager* interpreter_manager, std::vector<int>* result) const {
  result->clear();
  result->reserve(candidates.size());
  for (int i = 0; i < candidates.size();) {
    int first_non_overlapping =
        FirstNonOverlappingSpanIndex(candidates, /*start_index=*/i);

    const bool conflict_found = first_non_overlapping != (i + 1);
    if (conflict_found) {
      std::vector<int> candidate_indices;
      if (!ResolveConflict(context, cached_tokens, candidates,
                           detected_text_language_tags, i,
                           first_non_overlapping, annotation_usecase,
                           interpreter_manager, &candidate_indices)) {
        return false;
      }
      result->insert(result->end(), candidate_indices.begin(),
                     candidate_indices.end());
    } else {
      result->push_back(i);
    }

    // Skip over the whole conflicting group/go to next candidate.
    i = first_non_overlapping;
  }
  return true;
}

namespace {
// Returns true, if the given two sources do conflict in given annotation
// usecase.
//  - In SMART usecase, all sources do conflict, because there's only 1 possible
//  annotation for a given span.
//  - In RAW usecase, certain annotations are allowed to overlap (e.g. datetime
//  and duration), while others not (e.g. duration and number).
bool DoSourcesConflict(AnnotationUsecase annotation_usecase,
                       const AnnotatedSpan::Source source1,
                       const AnnotatedSpan::Source source2) {
  uint32 source_mask =
      (1 << static_cast<int>(source1)) | (1 << static_cast<int>(source2));

  switch (annotation_usecase) {
    case AnnotationUsecase_ANNOTATION_USECASE_SMART:
      // In the SMART mode, all annotations conflict.
      return true;

    case AnnotationUsecase_ANNOTATION_USECASE_RAW:
      // DURATION and DATETIME do not conflict. E.g. "let's meet in 3 hours",
      // can have two non-conflicting annotations: "in 3 hours" (datetime), "3
      // hours" (duration).
      if ((source_mask &
           (1 << static_cast<int>(AnnotatedSpan::Source::DURATION))) &&
          (source_mask &
           (1 << static_cast<int>(AnnotatedSpan::Source::DATETIME)))) {
        return false;
      }

      // A KNOWLEDGE entity does not conflict with anything.
      if ((source_mask &
           (1 << static_cast<int>(AnnotatedSpan::Source::KNOWLEDGE)))) {
        return false;
      }

      // A PERSONNAME entity does not conflict with anything.
      if ((source_mask &
           (1 << static_cast<int>(AnnotatedSpan::Source::PERSON_NAME)))) {
        return false;
      }

      // Entities from other sources can conflict.
      return true;
  }
}
}  // namespace

bool Annotator::ResolveConflict(
    const std::string& context, const std::vector<Token>& cached_tokens,
    const std::vector<AnnotatedSpan>& candidates,
    const std::vector<Locale>& detected_text_language_tags, int start_index,
    int end_index, AnnotationUsecase annotation_usecase,
    InterpreterManager* interpreter_manager,
    std::vector<int>* chosen_indices) const {
  std::vector<int> conflicting_indices;
  std::unordered_map<int, std::pair<float, int>> scores_lengths;
  for (int i = start_index; i < end_index; ++i) {
    conflicting_indices.push_back(i);
    if (!candidates[i].classification.empty()) {
      scores_lengths[i] = {
          GetPriorityScore(candidates[i].classification),
          candidates[i].span.second - candidates[i].span.first};
      continue;
    }

    // OPTIMIZATION: So that we don't have to classify all the ML model
    // spans apriori, we wait until we get here, when they conflict with
    // something and we need the actual classification scores. So if the
    // candidate conflicts and comes from the model, we need to run a
    // classification to determine its priority:
    std::vector<ClassificationResult> classification;
    if (!ModelClassifyText(context, cached_tokens, detected_text_language_tags,
                           candidates[i].span, interpreter_manager,
                           /*embedding_cache=*/nullptr, &classification)) {
      return false;
    }

    if (!classification.empty()) {
      scores_lengths[i] = {
          GetPriorityScore(classification),
          candidates[i].span.second - candidates[i].span.first};
    }
  }

  std::sort(
      conflicting_indices.begin(), conflicting_indices.end(),
      [this, &scores_lengths, candidates, conflicting_indices](int i, int j) {
        if (scores_lengths[i].first == scores_lengths[j].first &&
            prioritize_longest_annotation_) {
          return scores_lengths[i].second > scores_lengths[j].second;
        }
        return scores_lengths[i].first > scores_lengths[j].first;
      });

  // Here we keep a set of indices that were chosen, per-source, to enable
  // effective computation.
  std::unordered_map<AnnotatedSpan::Source, SortedIntSet>
      chosen_indices_for_source_map;

  // Greedily place the candidates if they don't conflict with the already
  // placed ones.
  for (int i = 0; i < conflicting_indices.size(); ++i) {
    const int considered_candidate = conflicting_indices[i];

    // See if there is a conflict between the candidate and all already placed
    // candidates.
    bool conflict = false;
    SortedIntSet* chosen_indices_for_source_ptr = nullptr;
    for (auto& source_set_pair : chosen_indices_for_source_map) {
      if (source_set_pair.first == candidates[considered_candidate].source) {
        chosen_indices_for_source_ptr = &source_set_pair.second;
      }

      const bool needs_conflict_resolution =
          annotation_usecase == AnnotationUsecase_ANNOTATION_USECASE_SMART ||
          (annotation_usecase == AnnotationUsecase_ANNOTATION_USECASE_RAW &&
           do_conflict_resolution_in_raw_mode_);
      if (needs_conflict_resolution &&
          DoSourcesConflict(annotation_usecase, source_set_pair.first,
                            candidates[considered_candidate].source) &&
          DoesCandidateConflict(considered_candidate, candidates,
                                source_set_pair.second)) {
        conflict = true;
        break;
      }
    }

    // Skip the candidate if a conflict was found.
    if (conflict) {
      continue;
    }

    // If the set of indices for the current source doesn't exist yet,
    // initialize it.
    if (chosen_indices_for_source_ptr == nullptr) {
      SortedIntSet new_set([&candidates](int a, int b) {
        return candidates[a].span.first < candidates[b].span.first;
      });
      chosen_indices_for_source_map[candidates[considered_candidate].source] =
          std::move(new_set);
      chosen_indices_for_source_ptr =
          &chosen_indices_for_source_map[candidates[considered_candidate]
                                             .source];
    }

    // Place the candidate to the output and to the per-source conflict set.
    chosen_indices->push_back(considered_candidate);
    chosen_indices_for_source_ptr->insert(considered_candidate);
  }

  std::sort(chosen_indices->begin(), chosen_indices->end());

  return true;
}

bool Annotator::ModelSuggestSelection(
    const UnicodeText& context_unicode, CodepointSpan click_indices,
    const std::vector<Locale>& detected_text_language_tags,
    InterpreterManager* interpreter_manager, std::vector<Token>* tokens,
    std::vector<AnnotatedSpan>* result) const {
  if (model_->triggering_options() == nullptr ||
      !(model_->triggering_options()->enabled_modes() & ModeFlag_SELECTION)) {
    return true;
  }

  if (!Locale::IsAnyLocaleSupported(detected_text_language_tags,
                                    ml_model_triggering_locales_,
                                    /*default_value=*/true)) {
    return true;
  }

  int click_pos;
  *tokens = selection_feature_processor_->Tokenize(context_unicode);
  selection_feature_processor_->RetokenizeAndFindClick(
      context_unicode, click_indices,
      selection_feature_processor_->GetOptions()->only_use_line_with_click(),
      tokens, &click_pos);
  if (click_pos == kInvalidIndex) {
    TC3_VLOG(1) << "Could not calculate the click position.";
    return false;
  }

  const int symmetry_context_size =
      model_->selection_options()->symmetry_context_size();
  const FeatureProcessorOptions_::BoundsSensitiveFeatures*
      bounds_sensitive_features = selection_feature_processor_->GetOptions()
                                      ->bounds_sensitive_features();

  // The symmetry context span is the clicked token with symmetry_context_size
  // tokens on either side.
  const TokenSpan symmetry_context_span = IntersectTokenSpans(
      ExpandTokenSpan(SingleTokenSpan(click_pos),
                      /*num_tokens_left=*/symmetry_context_size,
                      /*num_tokens_right=*/symmetry_context_size),
      {0, tokens->size()});

  // Compute the extraction span based on the model type.
  TokenSpan extraction_span;
  if (bounds_sensitive_features && bounds_sensitive_features->enabled()) {
    // The extraction span is the symmetry context span expanded to include
    // max_selection_span tokens on either side, which is how far a selection
    // can stretch from the click, plus a relevant number of tokens outside of
    // the bounds of the selection.
    const int max_selection_span =
        selection_feature_processor_->GetOptions()->max_selection_span();
    extraction_span =
        ExpandTokenSpan(symmetry_context_span,
                        /*num_tokens_left=*/max_selection_span +
                            bounds_sensitive_features->num_tokens_before(),
                        /*num_tokens_right=*/max_selection_span +
                            bounds_sensitive_features->num_tokens_after());
  } else {
    // The extraction span is the symmetry context span expanded to include
    // context_size tokens on either side.
    const int context_size =
        selection_feature_processor_->GetOptions()->context_size();
    extraction_span = ExpandTokenSpan(symmetry_context_span,
                                      /*num_tokens_left=*/context_size,
                                      /*num_tokens_right=*/context_size);
  }
  extraction_span = IntersectTokenSpans(extraction_span, {0, tokens->size()});

  if (!selection_feature_processor_->HasEnoughSupportedCodepoints(
          *tokens, extraction_span)) {
    return true;
  }

  std::unique_ptr<CachedFeatures> cached_features;
  if (!selection_feature_processor_->ExtractFeatures(
          *tokens, extraction_span,
          /*selection_span_for_feature=*/{kInvalidIndex, kInvalidIndex},
          embedding_executor_.get(),
          /*embedding_cache=*/nullptr,
          selection_feature_processor_->EmbeddingSize() +
              selection_feature_processor_->DenseFeaturesCount(),
          &cached_features)) {
    TC3_LOG(ERROR) << "Could not extract features.";
    return false;
  }

  // Produce selection model candidates.
  std::vector<TokenSpan> chunks;
  if (!ModelChunk(tokens->size(), /*span_of_interest=*/symmetry_context_span,
                  interpreter_manager->SelectionInterpreter(), *cached_features,
                  &chunks)) {
    TC3_LOG(ERROR) << "Could not chunk.";
    return false;
  }

  for (const TokenSpan& chunk : chunks) {
    AnnotatedSpan candidate;
    candidate.span = selection_feature_processor_->StripBoundaryCodepoints(
        context_unicode, TokenSpanToCodepointSpan(*tokens, chunk));
    if (model_->selection_options()->strip_unpaired_brackets()) {
      candidate.span =
          StripUnpairedBrackets(context_unicode, candidate.span, *unilib_);
    }

    // Only output non-empty spans.
    if (candidate.span.first != candidate.span.second) {
      result->push_back(candidate);
    }
  }
  return true;
}

bool Annotator::ModelClassifyText(
    const std::string& context,
    const std::vector<Locale>& detected_text_language_tags,
    CodepointSpan selection_indices, InterpreterManager* interpreter_manager,
    FeatureProcessor::EmbeddingCache* embedding_cache,
    std::vector<ClassificationResult>* classification_results) const {
  return ModelClassifyText(context, {}, detected_text_language_tags,
                           selection_indices, interpreter_manager,
                           embedding_cache, classification_results);
}

namespace internal {
std::vector<Token> CopyCachedTokens(const std::vector<Token>& cached_tokens,
                                    CodepointSpan selection_indices,
                                    TokenSpan tokens_around_selection_to_copy) {
  const auto first_selection_token = std::upper_bound(
      cached_tokens.begin(), cached_tokens.end(), selection_indices.first,
      [](int selection_start, const Token& token) {
        return selection_start < token.end;
      });
  const auto last_selection_token = std::lower_bound(
      cached_tokens.begin(), cached_tokens.end(), selection_indices.second,
      [](const Token& token, int selection_end) {
        return token.start < selection_end;
      });

  const int64 first_token = std::max(
      static_cast<int64>(0),
      static_cast<int64>((first_selection_token - cached_tokens.begin()) -
                         tokens_around_selection_to_copy.first));
  const int64 last_token = std::min(
      static_cast<int64>(cached_tokens.size()),
      static_cast<int64>((last_selection_token - cached_tokens.begin()) +
                         tokens_around_selection_to_copy.second));

  std::vector<Token> tokens;
  tokens.reserve(last_token - first_token);
  for (int i = first_token; i < last_token; ++i) {
    tokens.push_back(cached_tokens[i]);
  }
  return tokens;
}
}  // namespace internal

TokenSpan Annotator::ClassifyTextUpperBoundNeededTokens() const {
  const FeatureProcessorOptions_::BoundsSensitiveFeatures*
      bounds_sensitive_features =
          classification_feature_processor_->GetOptions()
              ->bounds_sensitive_features();
  if (bounds_sensitive_features && bounds_sensitive_features->enabled()) {
    // The extraction span is the selection span expanded to include a relevant
    // number of tokens outside of the bounds of the selection.
    return {bounds_sensitive_features->num_tokens_before(),
            bounds_sensitive_features->num_tokens_after()};
  } else {
    // The extraction span is the clicked token with context_size tokens on
    // either side.
    const int context_size =
        selection_feature_processor_->GetOptions()->context_size();
    return {context_size, context_size};
  }
}

namespace {
// Sorts the classification results from high score to low score.
void SortClassificationResults(
    std::vector<ClassificationResult>* classification_results) {
  std::sort(classification_results->begin(), classification_results->end(),
            [](const ClassificationResult& a, const ClassificationResult& b) {
              return a.score > b.score;
            });
}
}  // namespace

bool Annotator::ModelClassifyText(
    const std::string& context, const std::vector<Token>& cached_tokens,
    const std::vector<Locale>& detected_text_language_tags,
    CodepointSpan selection_indices, InterpreterManager* interpreter_manager,
    FeatureProcessor::EmbeddingCache* embedding_cache,
    std::vector<ClassificationResult>* classification_results) const {
  std::vector<Token> tokens;
  return ModelClassifyText(context, cached_tokens, detected_text_language_tags,
                           selection_indices, interpreter_manager,
                           embedding_cache, classification_results, &tokens);
}

bool Annotator::ModelClassifyText(
    const std::string& context, const std::vector<Token>& cached_tokens,
    const std::vector<Locale>& detected_text_language_tags,
    CodepointSpan selection_indices, InterpreterManager* interpreter_manager,
    FeatureProcessor::EmbeddingCache* embedding_cache,
    std::vector<ClassificationResult>* classification_results,
    std::vector<Token>* tokens) const {
  if (model_->triggering_options() == nullptr ||
      !(model_->triggering_options()->enabled_modes() &
        ModeFlag_CLASSIFICATION)) {
    return true;
  }

  if (!Locale::IsAnyLocaleSupported(detected_text_language_tags,
                                    ml_model_triggering_locales_,
                                    /*default_value=*/true)) {
    return true;
  }

  if (cached_tokens.empty()) {
    *tokens = classification_feature_processor_->Tokenize(context);
  } else {
    *tokens = internal::CopyCachedTokens(cached_tokens, selection_indices,
                                         ClassifyTextUpperBoundNeededTokens());
  }

  int click_pos;
  classification_feature_processor_->RetokenizeAndFindClick(
      context, selection_indices,
      classification_feature_processor_->GetOptions()
          ->only_use_line_with_click(),
      tokens, &click_pos);
  const TokenSpan selection_token_span =
      CodepointSpanToTokenSpan(*tokens, selection_indices);
  const int selection_num_tokens = TokenSpanSize(selection_token_span);
  if (model_->classification_options()->max_num_tokens() > 0 &&
      model_->classification_options()->max_num_tokens() <
          selection_num_tokens) {
    *classification_results = {{Collections::Other(), 1.0}};
    return true;
  }

  const FeatureProcessorOptions_::BoundsSensitiveFeatures*
      bounds_sensitive_features =
          classification_feature_processor_->GetOptions()
              ->bounds_sensitive_features();
  if (selection_token_span.first == kInvalidIndex ||
      selection_token_span.second == kInvalidIndex) {
    TC3_LOG(ERROR) << "Could not determine span.";
    return false;
  }

  // Compute the extraction span based on the model type.
  TokenSpan extraction_span;
  if (bounds_sensitive_features && bounds_sensitive_features->enabled()) {
    // The extraction span is the selection span expanded to include a relevant
    // number of tokens outside of the bounds of the selection.
    extraction_span = ExpandTokenSpan(
        selection_token_span,
        /*num_tokens_left=*/bounds_sensitive_features->num_tokens_before(),
        /*num_tokens_right=*/bounds_sensitive_features->num_tokens_after());
  } else {
    if (click_pos == kInvalidIndex) {
      TC3_LOG(ERROR) << "Couldn't choose a click position.";
      return false;
    }
    // The extraction span is the clicked token with context_size tokens on
    // either side.
    const int context_size =
        classification_feature_processor_->GetOptions()->context_size();
    extraction_span = ExpandTokenSpan(SingleTokenSpan(click_pos),
                                      /*num_tokens_left=*/context_size,
                                      /*num_tokens_right=*/context_size);
  }
  extraction_span = IntersectTokenSpans(extraction_span, {0, tokens->size()});

  if (!classification_feature_processor_->HasEnoughSupportedCodepoints(
          *tokens, extraction_span)) {
    *classification_results = {{Collections::Other(), 1.0}};
    return true;
  }

  std::unique_ptr<CachedFeatures> cached_features;
  if (!classification_feature_processor_->ExtractFeatures(
          *tokens, extraction_span, selection_indices,
          embedding_executor_.get(), embedding_cache,
          classification_feature_processor_->EmbeddingSize() +
              classification_feature_processor_->DenseFeaturesCount(),
          &cached_features)) {
    TC3_LOG(ERROR) << "Could not extract features.";
    return false;
  }

  std::vector<float> features;
  features.reserve(cached_features->OutputFeaturesSize());
  if (bounds_sensitive_features && bounds_sensitive_features->enabled()) {
    cached_features->AppendBoundsSensitiveFeaturesForSpan(selection_token_span,
                                                          &features);
  } else {
    cached_features->AppendClickContextFeaturesForClick(click_pos, &features);
  }

  TensorView<float> logits = classification_executor_->ComputeLogits(
      TensorView<float>(features.data(),
                        {1, static_cast<int>(features.size())}),
      interpreter_manager->ClassificationInterpreter());
  if (!logits.is_valid()) {
    TC3_LOG(ERROR) << "Couldn't compute logits.";
    return false;
  }

  if (logits.dims() != 2 || logits.dim(0) != 1 ||
      logits.dim(1) != classification_feature_processor_->NumCollections()) {
    TC3_LOG(ERROR) << "Mismatching output";
    return false;
  }

  const std::vector<float> scores =
      ComputeSoftmax(logits.data(), logits.dim(1));

  if (scores.empty()) {
    *classification_results = {{Collections::Other(), 1.0}};
    return true;
  }

  const int best_score_index =
      std::max_element(scores.begin(), scores.end()) - scores.begin();
  const std::string top_collection =
      classification_feature_processor_->LabelToCollection(best_score_index);

  // Sanity checks.
  if (top_collection == Collections::Phone()) {
    const int digit_count = CountDigits(context, selection_indices);
    if (digit_count <
            model_->classification_options()->phone_min_num_digits() ||
        digit_count >
            model_->classification_options()->phone_max_num_digits()) {
      *classification_results = {{Collections::Other(), 1.0}};
      return true;
    }
  } else if (top_collection == Collections::Address()) {
    if (selection_num_tokens <
        model_->classification_options()->address_min_num_tokens()) {
      *classification_results = {{Collections::Other(), 1.0}};
      return true;
    }
  } else if (top_collection == Collections::Dictionary()) {
    if (!Locale::IsAnyLocaleSupported(detected_text_language_tags,
                                      dictionary_locales_,
                                      /*default_value=*/false)) {
      *classification_results = {{Collections::Other(), 1.0}};
      return true;
    }
  }

  *classification_results = {{top_collection, /*arg_score=*/1.0,
                              /*arg_priority_score=*/scores[best_score_index]}};

  // For some entities, we might want to clamp the priority score, for better
  // conflict resolution between entities.
  if (model_->triggering_options() != nullptr &&
      model_->triggering_options()->collection_to_priority() != nullptr) {
    if (auto entry =
            model_->triggering_options()->collection_to_priority()->LookupByKey(
                top_collection.c_str())) {
      (*classification_results)[0].priority_score *= entry->value();
    }
  }
  return true;
}

bool Annotator::RegexClassifyText(
    const std::string& context, CodepointSpan selection_indices,
    std::vector<ClassificationResult>* classification_result) const {
  const std::string selection_text =
      UTF8ToUnicodeText(context, /*do_copy=*/false)
          .UTF8Substring(selection_indices.first, selection_indices.second);
  const UnicodeText selection_text_unicode(
      UTF8ToUnicodeText(selection_text, /*do_copy=*/false));

  // Check whether any of the regular expressions match.
  for (const int pattern_id : classification_regex_patterns_) {
    const CompiledRegexPattern& regex_pattern = regex_patterns_[pattern_id];
    const std::unique_ptr<UniLib::RegexMatcher> matcher =
        regex_pattern.pattern->Matcher(selection_text_unicode);
    int status = UniLib::RegexMatcher::kNoError;
    bool matches;
    if (regex_pattern.config->use_approximate_matching()) {
      matches = matcher->ApproximatelyMatches(&status);
    } else {
      matches = matcher->Matches(&status);
    }
    if (status != UniLib::RegexMatcher::kNoError) {
      return false;
    }
    if (matches && VerifyRegexMatchCandidate(
                       context, regex_pattern.config->verification_options(),
                       selection_text, matcher.get())) {
      classification_result->push_back(
          {regex_pattern.config->collection_name()->str(),
           regex_pattern.config->target_classification_score(),
           regex_pattern.config->priority_score()});
      if (!SerializedEntityDataFromRegexMatch(
              regex_pattern.config, matcher.get(),
              &classification_result->back().serialized_entity_data)) {
        TC3_LOG(ERROR) << "Could not get entity data.";
        return false;
      }
    }
  }

  return true;
}

namespace {
std::string PickCollectionForDatetime(
    const DatetimeParseResult& datetime_parse_result) {
  switch (datetime_parse_result.granularity) {
    case GRANULARITY_HOUR:
    case GRANULARITY_MINUTE:
    case GRANULARITY_SECOND:
      return Collections::DateTime();
    default:
      return Collections::Date();
  }
}

std::string CreateDatetimeSerializedEntityData(
    const DatetimeParseResult& parse_result) {
  EntityDataT entity_data;
  entity_data.datetime.reset(new EntityData_::DatetimeT());
  entity_data.datetime->time_ms_utc = parse_result.time_ms_utc;
  entity_data.datetime->granularity =
      static_cast<EntityData_::Datetime_::Granularity>(
          parse_result.granularity);

  for (const auto& c : parse_result.datetime_components) {
    EntityData_::Datetime_::DatetimeComponentT datetime_component;
    datetime_component.absolute_value = c.value;
    datetime_component.relative_count = c.relative_count;
    datetime_component.component_type =
        static_cast<EntityData_::Datetime_::DatetimeComponent_::ComponentType>(
            c.component_type);
    datetime_component.relation_type =
        EntityData_::Datetime_::DatetimeComponent_::RelationType_ABSOLUTE;
    if (c.relative_qualifier !=
        DatetimeComponent::RelativeQualifier::UNSPECIFIED) {
      datetime_component.relation_type =
          EntityData_::Datetime_::DatetimeComponent_::RelationType_RELATIVE;
    }
    entity_data.datetime->datetime_component.emplace_back(
        new EntityData_::Datetime_::DatetimeComponentT(datetime_component));
  }
  flatbuffers::FlatBufferBuilder builder;
  FinishEntityDataBuffer(builder, EntityData::Pack(builder, &entity_data));
  return std::string(reinterpret_cast<const char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

}  // namespace

bool Annotator::DatetimeClassifyText(
    const std::string& context, CodepointSpan selection_indices,
    const ClassificationOptions& options,
    std::vector<ClassificationResult>* classification_results) const {
  if (!datetime_parser_ && !cfg_datetime_parser_) {
    return true;
  }

  const std::string selection_text =
      UTF8ToUnicodeText(context, /*do_copy=*/false)
          .UTF8Substring(selection_indices.first, selection_indices.second);

  std::vector<DatetimeParseResultSpan> datetime_spans;

  if (cfg_datetime_parser_) {
    if (!(model_->grammar_datetime_model()->enabled_modes() &
          ModeFlag_CLASSIFICATION)) {
      return true;
    }
    std::vector<Locale> parsed_locales;
    ParseLocales(options.locales, &parsed_locales);
    cfg_datetime_parser_->Parse(
        selection_text,
        ToDateAnnotationOptions(
            model_->grammar_datetime_model()->annotation_options(),
            options.reference_timezone, options.reference_time_ms_utc),
        parsed_locales, &datetime_spans);
  }

  if (datetime_parser_) {
    if (!datetime_parser_->Parse(selection_text, options.reference_time_ms_utc,
                                 options.reference_timezone, options.locales,
                                 ModeFlag_CLASSIFICATION,
                                 options.annotation_usecase,
                                 /*anchor_start_end=*/true, &datetime_spans)) {
      TC3_LOG(ERROR) << "Error during parsing datetime.";
      return false;
    }
  }

  for (const DatetimeParseResultSpan& datetime_span : datetime_spans) {
    // Only consider the result valid if the selection and extracted datetime
    // spans exactly match.
    if (std::make_pair(datetime_span.span.first + selection_indices.first,
                       datetime_span.span.second + selection_indices.first) ==
        selection_indices) {
      for (const DatetimeParseResult& parse_result : datetime_span.data) {
        classification_results->emplace_back(
            PickCollectionForDatetime(parse_result),
            datetime_span.target_classification_score);
        classification_results->back().datetime_parse_result = parse_result;
        classification_results->back().serialized_entity_data =
            CreateDatetimeSerializedEntityData(parse_result);
        classification_results->back().priority_score =
            datetime_span.priority_score;
      }
      return true;
    }
  }
  return true;
}

std::vector<ClassificationResult> Annotator::ClassifyText(
    const std::string& context, CodepointSpan selection_indices,
    const ClassificationOptions& options) const {
  if (!initialized_) {
    TC3_LOG(ERROR) << "Not initialized";
    return {};
  }
  if (options.annotation_usecase !=
      AnnotationUsecase_ANNOTATION_USECASE_SMART) {
    TC3_LOG(WARNING)
        << "Invoking ClassifyText, which is not supported in RAW mode.";
    return {};
  }
  if (!(model_->enabled_modes() & ModeFlag_CLASSIFICATION)) {
    return {};
  }

  std::vector<Locale> detected_text_language_tags;
  if (!ParseLocales(options.detected_text_language_tags,
                    &detected_text_language_tags)) {
    TC3_LOG(WARNING)
        << "Failed to parse the detected_text_language_tags in options: "
        << options.detected_text_language_tags;
  }
  if (!Locale::IsAnyLocaleSupported(detected_text_language_tags,
                                    model_triggering_locales_,
                                    /*default_value=*/true)) {
    return {};
  }

  if (!IsValidSpanInput(UTF8ToUnicodeText(context, /*do_copy=*/false),
                        selection_indices)) {
    TC3_VLOG(1) << "Trying to run ClassifyText with invalid input: "
                << std::get<0>(selection_indices) << " "
                << std::get<1>(selection_indices);
    return {};
  }

  // We'll accumulate a list of candidates, and pick the best candidate in the
  // end.
  std::vector<AnnotatedSpan> candidates;

  // Try the knowledge engine.
  // TODO(b/126579108): Propagate error status.
  ClassificationResult knowledge_result;
  if (knowledge_engine_ &&
      knowledge_engine_->ClassifyText(
          context, selection_indices, options.annotation_usecase,
          options.location_context, Permissions(), &knowledge_result)) {
    candidates.push_back({selection_indices, {knowledge_result}});
    candidates.back().source = AnnotatedSpan::Source::KNOWLEDGE;
  }

  AddContactMetadataToKnowledgeClassificationResults(&candidates);

  // Try the contact engine.
  // TODO(b/126579108): Propagate error status.
  ClassificationResult contact_result;
  if (contact_engine_ && contact_engine_->ClassifyText(
                             context, selection_indices, &contact_result)) {
    candidates.push_back({selection_indices, {contact_result}});
  }

  // Try the person name engine.
  ClassificationResult person_name_result;
  if (person_name_engine_ &&
      person_name_engine_->ClassifyText(context, selection_indices,
                                        &person_name_result)) {
    candidates.push_back({selection_indices, {person_name_result}});
    candidates.back().source = AnnotatedSpan::Source::PERSON_NAME;
  }

  // Try the installed app engine.
  // TODO(b/126579108): Propagate error status.
  ClassificationResult installed_app_result;
  if (installed_app_engine_ &&
      installed_app_engine_->ClassifyText(context, selection_indices,
                                          &installed_app_result)) {
    candidates.push_back({selection_indices, {installed_app_result}});
  }

  // Try the regular expression models.
  std::vector<ClassificationResult> regex_results;
  if (!RegexClassifyText(context, selection_indices, &regex_results)) {
    return {};
  }
  for (const ClassificationResult& result : regex_results) {
    candidates.push_back({selection_indices, {result}});
  }

  // Try the date model.
  //
  // DatetimeClassifyText only returns the first result, which can however have
  // more interpretations. They are inserted in the candidates as a single
  // AnnotatedSpan, so that they get treated together by the conflict resolution
  // algorithm.
  std::vector<ClassificationResult> datetime_results;
  if (!DatetimeClassifyText(context, selection_indices, options,
                            &datetime_results)) {
    return {};
  }
  if (!datetime_results.empty()) {
    candidates.push_back({selection_indices, std::move(datetime_results)});
    candidates.back().source = AnnotatedSpan::Source::DATETIME;
  }

  const UnicodeText context_unicode =
      UTF8ToUnicodeText(context, /*do_copy=*/false);

  // Try the number annotator.
  // TODO(b/126579108): Propagate error status.
  ClassificationResult number_annotator_result;
  if (number_annotator_ &&
      number_annotator_->ClassifyText(context_unicode, selection_indices,
                                      options.annotation_usecase,
                                      &number_annotator_result)) {
    candidates.push_back({selection_indices, {number_annotator_result}});
  }

  // Try the duration annotator.
  ClassificationResult duration_annotator_result;
  if (duration_annotator_ &&
      duration_annotator_->ClassifyText(context_unicode, selection_indices,
                                        options.annotation_usecase,
                                        &duration_annotator_result)) {
    candidates.push_back({selection_indices, {duration_annotator_result}});
    candidates.back().source = AnnotatedSpan::Source::DURATION;
  }

  // Try the translate annotator.
  ClassificationResult translate_annotator_result;
  if (translate_annotator_ &&
      translate_annotator_->ClassifyText(context_unicode, selection_indices,
                                         options.user_familiar_language_tags,
                                         &translate_annotator_result)) {
    candidates.push_back({selection_indices, {translate_annotator_result}});
  }

  // Try the grammar model.
  ClassificationResult grammar_annotator_result;
  if (grammar_annotator_ && grammar_annotator_->ClassifyText(
                                detected_text_language_tags, context_unicode,
                                selection_indices, &grammar_annotator_result)) {
    candidates.push_back({selection_indices, {grammar_annotator_result}});
  }

  ClassificationResult experimental_annotator_result;
  if (experimental_annotator_ &&
      experimental_annotator_->ClassifyText(context_unicode, selection_indices,
                                            &experimental_annotator_result)) {
    candidates.push_back({selection_indices, {experimental_annotator_result}});
  }

  // Try the ML model.
  //
  // The output of the model is considered as an exclusive 1-of-N choice. That's
  // why it's inserted as only 1 AnnotatedSpan into candidates, as opposed to 1
  // span for each candidate, like e.g. the regex model.
  InterpreterManager interpreter_manager(selection_executor_.get(),
                                         classification_executor_.get());
  std::vector<ClassificationResult> model_results;
  std::vector<Token> tokens;
  if (!ModelClassifyText(
          context, /*cached_tokens=*/{}, detected_text_language_tags,
          selection_indices, &interpreter_manager,
          /*embedding_cache=*/nullptr, &model_results, &tokens)) {
    return {};
  }
  if (!model_results.empty()) {
    candidates.push_back({selection_indices, std::move(model_results)});
  }

  std::vector<int> candidate_indices;
  if (!ResolveConflicts(candidates, context, tokens,
                        detected_text_language_tags, options.annotation_usecase,
                        &interpreter_manager, &candidate_indices)) {
    TC3_LOG(ERROR) << "Couldn't resolve conflicts.";
    return {};
  }

  std::vector<ClassificationResult> results;
  for (const int i : candidate_indices) {
    for (const ClassificationResult& result : candidates[i].classification) {
      if (!FilteredForClassification(result)) {
        results.push_back(result);
      }
    }
  }

  // Sort results according to score.
  std::sort(results.begin(), results.end(),
            [](const ClassificationResult& a, const ClassificationResult& b) {
              return a.score > b.score;
            });

  if (results.empty()) {
    results = {{Collections::Other(), 1.0}};
  }
  return results;
}

bool Annotator::ModelAnnotate(
    const std::string& context,
    const std::vector<Locale>& detected_text_language_tags,
    InterpreterManager* interpreter_manager, std::vector<Token>* tokens,
    std::vector<AnnotatedSpan>* result) const {
  if (model_->triggering_options() == nullptr ||
      !(model_->triggering_options()->enabled_modes() & ModeFlag_ANNOTATION)) {
    return true;
  }

  if (!Locale::IsAnyLocaleSupported(detected_text_language_tags,
                                    ml_model_triggering_locales_,
                                    /*default_value=*/true)) {
    return true;
  }

  const UnicodeText context_unicode = UTF8ToUnicodeText(context,
                                                        /*do_copy=*/false);
  std::vector<UnicodeTextRange> lines;
  if (!selection_feature_processor_->GetOptions()->only_use_line_with_click()) {
    lines.push_back({context_unicode.begin(), context_unicode.end()});
  } else {
    lines = selection_feature_processor_->SplitContext(
        context_unicode, selection_feature_processor_->GetOptions()
                             ->use_pipe_character_for_newline());
  }

  const float min_annotate_confidence =
      (model_->triggering_options() != nullptr
           ? model_->triggering_options()->min_annotate_confidence()
           : 0.f);

  for (const UnicodeTextRange& line : lines) {
    FeatureProcessor::EmbeddingCache embedding_cache;
    const std::string line_str =
        UnicodeText::UTF8Substring(line.first, line.second);

    *tokens = selection_feature_processor_->Tokenize(line_str);
    selection_feature_processor_->RetokenizeAndFindClick(
        line_str, {0, std::distance(line.first, line.second)},
        selection_feature_processor_->GetOptions()->only_use_line_with_click(),
        tokens,
        /*click_pos=*/nullptr);
    const TokenSpan full_line_span = {0, tokens->size()};

    // TODO(zilka): Add support for greater granularity of this check.
    if (!selection_feature_processor_->HasEnoughSupportedCodepoints(
            *tokens, full_line_span)) {
      continue;
    }

    std::unique_ptr<CachedFeatures> cached_features;
    if (!selection_feature_processor_->ExtractFeatures(
            *tokens, full_line_span,
            /*selection_span_for_feature=*/{kInvalidIndex, kInvalidIndex},
            embedding_executor_.get(),
            /*embedding_cache=*/nullptr,
            selection_feature_processor_->EmbeddingSize() +
                selection_feature_processor_->DenseFeaturesCount(),
            &cached_features)) {
      TC3_LOG(ERROR) << "Could not extract features.";
      return false;
    }

    std::vector<TokenSpan> local_chunks;
    if (!ModelChunk(tokens->size(), /*span_of_interest=*/full_line_span,
                    interpreter_manager->SelectionInterpreter(),
                    *cached_features, &local_chunks)) {
      TC3_LOG(ERROR) << "Could not chunk.";
      return false;
    }

    const int offset = std::distance(context_unicode.begin(), line.first);
    for (const TokenSpan& chunk : local_chunks) {
      const CodepointSpan codepoint_span =
          selection_feature_processor_->StripBoundaryCodepoints(
              line_str, TokenSpanToCodepointSpan(*tokens, chunk));

      // Skip empty spans.
      if (codepoint_span.first != codepoint_span.second) {
        std::vector<ClassificationResult> classification;
        if (!ModelClassifyText(line_str, *tokens, detected_text_language_tags,
                               codepoint_span, interpreter_manager,
                               &embedding_cache, &classification)) {
          TC3_LOG(ERROR) << "Could not classify text: "
                         << (codepoint_span.first + offset) << " "
                         << (codepoint_span.second + offset);
          return false;
        }

        // Do not include the span if it's classified as "other".
        if (!classification.empty() && !ClassifiedAsOther(classification) &&
            classification[0].score >= min_annotate_confidence) {
          AnnotatedSpan result_span;
          result_span.span = {codepoint_span.first + offset,
                              codepoint_span.second + offset};
          result_span.classification = std::move(classification);
          result->push_back(std::move(result_span));
        }
      }
    }
  }
  return true;
}

const FeatureProcessor* Annotator::SelectionFeatureProcessorForTests() const {
  return selection_feature_processor_.get();
}

const FeatureProcessor* Annotator::ClassificationFeatureProcessorForTests()
    const {
  return classification_feature_processor_.get();
}

const DatetimeParser* Annotator::DatetimeParserForTests() const {
  return datetime_parser_.get();
}

void Annotator::RemoveNotEnabledEntityTypes(
    const EnabledEntityTypes& is_entity_type_enabled,
    std::vector<AnnotatedSpan>* annotated_spans) const {
  for (AnnotatedSpan& annotated_span : *annotated_spans) {
    std::vector<ClassificationResult>& classifications =
        annotated_span.classification;
    classifications.erase(
        std::remove_if(classifications.begin(), classifications.end(),
                       [&is_entity_type_enabled](
                           const ClassificationResult& classification_result) {
                         return !is_entity_type_enabled(
                             classification_result.collection);
                       }),
        classifications.end());
  }
  annotated_spans->erase(
      std::remove_if(annotated_spans->begin(), annotated_spans->end(),
                     [](const AnnotatedSpan& annotated_span) {
                       return annotated_span.classification.empty();
                     }),
      annotated_spans->end());
}

void Annotator::AddContactMetadataToKnowledgeClassificationResults(
    std::vector<AnnotatedSpan>* candidates) const {
  if (candidates == nullptr || contact_engine_ == nullptr) {
    return;
  }
  for (auto& candidate : *candidates) {
    for (auto& classification_result : candidate.classification) {
      contact_engine_->AddContactMetadataToKnowledgeClassificationResult(
          &classification_result);
    }
  }
}

Status Annotator::AnnotateSingleInput(
    const std::string& context, const AnnotationOptions& options,
    std::vector<AnnotatedSpan>* candidates) const {
  if (!(model_->enabled_modes() & ModeFlag_ANNOTATION)) {
    return Status(StatusCode::UNAVAILABLE, "Model annotation was not enabled.");
  }

  const UnicodeText context_unicode =
      UTF8ToUnicodeText(context, /*do_copy=*/false);
  if (!context_unicode.is_valid()) {
    return Status(StatusCode::INVALID_ARGUMENT,
                  "Context string isn't valid UTF8.");
  }

  std::vector<Locale> detected_text_language_tags;
  if (!ParseLocales(options.detected_text_language_tags,
                    &detected_text_language_tags)) {
    TC3_LOG(WARNING)
        << "Failed to parse the detected_text_language_tags in options: "
        << options.detected_text_language_tags;
  }
  if (!Locale::IsAnyLocaleSupported(detected_text_language_tags,
                                    model_triggering_locales_,
                                    /*default_value=*/true)) {
    return Status(
        StatusCode::UNAVAILABLE,
        "The detected language tags are not in the supported locales.");
  }

  InterpreterManager interpreter_manager(selection_executor_.get(),
                                         classification_executor_.get());

  // Annotate with the selection model.
  std::vector<Token> tokens;
  if (!ModelAnnotate(context, detected_text_language_tags, &interpreter_manager,
                     &tokens, candidates)) {
    return Status(StatusCode::INTERNAL, "Couldn't run ModelAnnotate.");
  }

  // Annotate with the regular expression models.
  if (!RegexChunk(UTF8ToUnicodeText(context, /*do_copy=*/false),
                  annotation_regex_patterns_, candidates,
                  options.is_serialized_entity_data_enabled)) {
    return Status(StatusCode::INTERNAL, "Couldn't run RegexChunk.");
  }

  // Annotate with the datetime model.
  const EnabledEntityTypes is_entity_type_enabled(options.entity_types);
  if ((is_entity_type_enabled(Collections::Date()) ||
       is_entity_type_enabled(Collections::DateTime())) &&
      !DatetimeChunk(UTF8ToUnicodeText(context, /*do_copy=*/false),
                     options.reference_time_ms_utc, options.reference_timezone,
                     options.locales, ModeFlag_ANNOTATION,
                     options.annotation_usecase,
                     options.is_serialized_entity_data_enabled, candidates)) {
    return Status(StatusCode::INTERNAL, "Couldn't run DatetimeChunk.");
  }

  // Annotate with the contact engine.
  if (contact_engine_ &&
      !contact_engine_->Chunk(context_unicode, tokens, candidates)) {
    return Status(StatusCode::INTERNAL, "Couldn't run contact engine Chunk.");
  }

  // Annotate with the installed app engine.
  if (installed_app_engine_ &&
      !installed_app_engine_->Chunk(context_unicode, tokens, candidates)) {
    return Status(StatusCode::INTERNAL,
                  "Couldn't run installed app engine Chunk.");
  }

  // Annotate with the number annotator.
  if (number_annotator_ != nullptr &&
      !number_annotator_->FindAll(context_unicode, options.annotation_usecase,
                                  candidates)) {
    return Status(StatusCode::INTERNAL,
                  "Couldn't run number annotator FindAll.");
  }

  // Annotate with the duration annotator.
  if (is_entity_type_enabled(Collections::Duration()) &&
      duration_annotator_ != nullptr &&
      !duration_annotator_->FindAll(context_unicode, tokens,
                                    options.annotation_usecase, candidates)) {
    return Status(StatusCode::INTERNAL,
                  "Couldn't run duration annotator FindAll.");
  }

  // Annotate with the person name engine.
  if (is_entity_type_enabled(Collections::PersonName()) &&
      person_name_engine_ &&
      !person_name_engine_->Chunk(context_unicode, tokens, candidates)) {
    return Status(StatusCode::INTERNAL,
                  "Couldn't run person name engine Chunk.");
  }

  // Annotate with the grammar annotators.
  if (grammar_annotator_ != nullptr &&
      !grammar_annotator_->Annotate(detected_text_language_tags,
                                    context_unicode, candidates)) {
    return Status(StatusCode::INTERNAL, "Couldn't run grammar annotators.");
  }

  if (experimental_annotator_ != nullptr &&
      !experimental_annotator_->Annotate(context_unicode, candidates)) {
    return Status(StatusCode::INTERNAL, "Couldn't run experimental annotator.");
  }

  // Sort candidates according to their position in the input, so that the next
  // code can assume that any connected component of overlapping spans forms a
  // contiguous block.
  // Also sort them according to the end position and collection, so that the
  // deduplication code below can assume that same spans and classifications
  // form contiguous blocks.
  std::sort(candidates->begin(), candidates->end(),
            [](const AnnotatedSpan& a, const AnnotatedSpan& b) {
              if (a.span.first != b.span.first) {
                return a.span.first < b.span.first;
              }

              if (a.span.second != b.span.second) {
                return a.span.second < b.span.second;
              }

              return a.classification[0].collection <
                     b.classification[0].collection;
            });

  std::vector<int> candidate_indices;
  if (!ResolveConflicts(*candidates, context, tokens,
                        detected_text_language_tags, options.annotation_usecase,
                        &interpreter_manager, &candidate_indices)) {
    return Status(StatusCode::INTERNAL, "Couldn't resolve conflicts.");
  }

  // Remove candidates that overlap exactly and have the same collection.
  // This can e.g. happen for phone coming from both ML model and regex.
  candidate_indices.erase(
      std::unique(candidate_indices.begin(), candidate_indices.end(),
                  [&candidates](const int a_index, const int b_index) {
                    const AnnotatedSpan& a = (*candidates)[a_index];
                    const AnnotatedSpan& b = (*candidates)[b_index];
                    return a.span == b.span &&
                           a.classification[0].collection ==
                               b.classification[0].collection;
                  }),
      candidate_indices.end());

  std::vector<AnnotatedSpan> result;
  result.reserve(candidate_indices.size());
  for (const int i : candidate_indices) {
    if ((*candidates)[i].classification.empty() ||
        ClassifiedAsOther((*candidates)[i].classification) ||
        FilteredForAnnotation((*candidates)[i])) {
      continue;
    }
    result.push_back(std::move((*candidates)[i]));
  }

  // We generate all candidates and remove them later (with the exception of
  // date/time/duration entities) because there are complex interdependencies
  // between the entity types. E.g., the TLD of an email can be interpreted as a
  // URL, but most likely a user of the API does not want such annotations if
  // "url" is enabled and "email" is not.
  RemoveNotEnabledEntityTypes(is_entity_type_enabled, &result);

  for (AnnotatedSpan& annotated_span : result) {
    SortClassificationResults(&annotated_span.classification);
  }
  *candidates = result;
  return Status::OK;
}

StatusOr<std::vector<std::vector<AnnotatedSpan>>>
Annotator::AnnotateStructuredInput(
    const std::vector<InputFragment>& string_fragments,
    const AnnotationOptions& options) const {
  std::vector<std::vector<AnnotatedSpan>> annotation_candidates(
      string_fragments.size());

  std::vector<std::string> text_to_annotate;
  text_to_annotate.reserve(string_fragments.size());
  for (const auto& string_fragment : string_fragments) {
    text_to_annotate.push_back(string_fragment.text);
  }

  // KnowledgeEngine is special, because it supports annotation of multiple
  // fragments at once.
  if (knowledge_engine_ &&
      !knowledge_engine_
           ->ChunkMultipleSpans(text_to_annotate, options.annotation_usecase,
                                options.location_context, options.permissions,
                                &annotation_candidates)
           .ok()) {
    return Status(StatusCode::INTERNAL, "Couldn't run knowledge engine Chunk.");
  }
  // The annotator engines shouldn't change the number of annotation vectors.
  if (annotation_candidates.size() != text_to_annotate.size()) {
    TC3_LOG(ERROR) << "Received " << text_to_annotate.size()
                   << " texts to annotate but generated a different number of  "
                      "lists of annotations:"
                   << annotation_candidates.size();
    return Status(StatusCode::INTERNAL,
                  "Number of annotation candidates differs from "
                  "number of texts to annotate.");
  }

  // Other annotators run on each fragment independently.
  for (int i = 0; i < text_to_annotate.size(); ++i) {
    AnnotationOptions annotation_options = options;
    if (string_fragments[i].datetime_options.has_value()) {
      DatetimeOptions reference_datetime =
          string_fragments[i].datetime_options.value();
      annotation_options.reference_time_ms_utc =
          reference_datetime.reference_time_ms_utc;
      annotation_options.reference_timezone =
          reference_datetime.reference_timezone;
    }

    AddContactMetadataToKnowledgeClassificationResults(
        &annotation_candidates[i]);

    Status annotation_status = AnnotateSingleInput(
        text_to_annotate[i], annotation_options, &annotation_candidates[i]);
    if (!annotation_status.ok()) {
      return annotation_status;
    }
  }
  return annotation_candidates;
}

std::vector<AnnotatedSpan> Annotator::Annotate(
    const std::string& context, const AnnotationOptions& options) const {
  std::vector<InputFragment> string_fragments;
  string_fragments.push_back({.text = context});
  StatusOr<std::vector<std::vector<AnnotatedSpan>>> annotations =
      AnnotateStructuredInput(string_fragments, options);
  if (!annotations.ok()) {
    TC3_LOG(ERROR) << "Returned error when calling AnnotateStructuredInput: "
                   << annotations.status().error_message();
    return {};
  }
  return annotations.ValueOrDie()[0];
}

CodepointSpan Annotator::ComputeSelectionBoundaries(
    const UniLib::RegexMatcher* match,
    const RegexModel_::Pattern* config) const {
  if (config->capturing_group() == nullptr) {
    // Use first capturing group to specify the selection.
    int status = UniLib::RegexMatcher::kNoError;
    const CodepointSpan result = {match->Start(1, &status),
                                  match->End(1, &status)};
    if (status != UniLib::RegexMatcher::kNoError) {
      return {kInvalidIndex, kInvalidIndex};
    }
    return result;
  }

  CodepointSpan result = {kInvalidIndex, kInvalidIndex};
  const int num_groups = config->capturing_group()->size();
  for (int i = 0; i < num_groups; i++) {
    if (!config->capturing_group()->Get(i)->extend_selection()) {
      continue;
    }

    int status = UniLib::RegexMatcher::kNoError;
    // Check match and adjust bounds.
    const int group_start = match->Start(i, &status);
    const int group_end = match->End(i, &status);
    if (status != UniLib::RegexMatcher::kNoError) {
      return {kInvalidIndex, kInvalidIndex};
    }
    if (group_start == kInvalidIndex || group_end == kInvalidIndex) {
      continue;
    }
    if (result.first == kInvalidIndex) {
      result = {group_start, group_end};
    } else {
      result.first = std::min(result.first, group_start);
      result.second = std::max(result.second, group_end);
    }
  }
  return result;
}

bool Annotator::HasEntityData(const RegexModel_::Pattern* pattern) const {
  if (pattern->serialized_entity_data() != nullptr ||
      pattern->entity_data() != nullptr) {
    return true;
  }
  if (pattern->capturing_group() != nullptr) {
    for (const CapturingGroup* group : *pattern->capturing_group()) {
      if (group->entity_field_path() != nullptr) {
        return true;
      }
      if (group->serialized_entity_data() != nullptr ||
          group->entity_data() != nullptr) {
        return true;
      }
    }
  }
  return false;
}

bool Annotator::SerializedEntityDataFromRegexMatch(
    const RegexModel_::Pattern* pattern, UniLib::RegexMatcher* matcher,
    std::string* serialized_entity_data) const {
  if (!HasEntityData(pattern)) {
    serialized_entity_data->clear();
    return true;
  }
  TC3_CHECK(entity_data_builder_ != nullptr);

  std::unique_ptr<ReflectiveFlatbuffer> entity_data =
      entity_data_builder_->NewRoot();

  TC3_CHECK(entity_data != nullptr);

  // Set fixed entity data.
  if (pattern->serialized_entity_data() != nullptr) {
    entity_data->MergeFromSerializedFlatbuffer(
        StringPiece(pattern->serialized_entity_data()->c_str(),
                    pattern->serialized_entity_data()->size()));
  }
  if (pattern->entity_data() != nullptr) {
    entity_data->MergeFrom(
        reinterpret_cast<const flatbuffers::Table*>(pattern->entity_data()));
  }

  // Add entity data from rule capturing groups.
  if (pattern->capturing_group() != nullptr) {
    const int num_groups = pattern->capturing_group()->size();
    for (int i = 0; i < num_groups; i++) {
      const CapturingGroup* group = pattern->capturing_group()->Get(i);

      // Check whether the group matched.
      Optional<std::string> group_match_text =
          GetCapturingGroupText(matcher, /*group_id=*/i);
      if (!group_match_text.has_value()) {
        continue;
      }

      // Set fixed entity data from capturing group match.
      if (group->serialized_entity_data() != nullptr) {
        entity_data->MergeFromSerializedFlatbuffer(
            StringPiece(group->serialized_entity_data()->c_str(),
                        group->serialized_entity_data()->size()));
      }
      if (group->entity_data() != nullptr) {
        entity_data->MergeFrom(reinterpret_cast<const flatbuffers::Table*>(
            pattern->entity_data()));
      }

      // Set entity field from capturing group text.
      if (group->entity_field_path() != nullptr) {
        UnicodeText normalized_group_match_text =
            UTF8ToUnicodeText(group_match_text.value(), /*do_copy=*/false);

        // Apply normalization if specified.
        if (group->normalization_options() != nullptr) {
          normalized_group_match_text =
              NormalizeText(*unilib_, group->normalization_options(),
                            normalized_group_match_text);
        }

        if (!entity_data->ParseAndSet(
                group->entity_field_path(),
                normalized_group_match_text.ToUTF8String())) {
          TC3_LOG(ERROR)
              << "Could not set entity data from rule capturing group.";
          return false;
        }
      }
    }
  }

  *serialized_entity_data = entity_data->Serialize();
  return true;
}

UnicodeText RemoveMoneySeparators(
    const std::unordered_set<char32>& decimal_separators,
    const UnicodeText& amount,
    UnicodeText::const_iterator it_decimal_separator) {
  UnicodeText whole_amount;
  for (auto it = amount.begin();
       it != amount.end() && it != it_decimal_separator; ++it) {
    if (std::find(decimal_separators.begin(), decimal_separators.end(),
                  static_cast<char32>(*it)) == decimal_separators.end()) {
      whole_amount.push_back(*it);
    }
  }
  return whole_amount;
}

bool Annotator::ParseAndFillInMoneyAmount(
    std::string* serialized_entity_data) const {
  std::unique_ptr<EntityDataT> data =
      LoadAndVerifyMutableFlatbuffer<libtextclassifier3::EntityData>(
          *serialized_entity_data);
  if (data == nullptr) {
    if (model_->version() >= 706) {
      // This way of parsing money entity data is enabled for models newer than
      // v706, consequently logging errors only for them (b/156634162).
      TC3_LOG(ERROR)
          << "Data field is null when trying to parse Money Entity Data";
    }
    return false;
  }
  if (data->money->unnormalized_amount.empty()) {
    if (model_->version() >= 706) {
      // This way of parsing money entity data is enabled for models newer than
      // v706, consequently logging errors only for them (b/156634162).
      TC3_LOG(ERROR)
          << "Data unnormalized_amount is empty when trying to parse "
             "Money Entity Data";
    }
    return false;
  }

  UnicodeText amount =
      UTF8ToUnicodeText(data->money->unnormalized_amount, /*do_copy=*/false);
  int separator_back_index = 0;
  auto it_decimal_separator = --amount.end();
  for (; it_decimal_separator != amount.begin();
       --it_decimal_separator, ++separator_back_index) {
    if (std::find(money_separators_.begin(), money_separators_.end(),
                  static_cast<char32>(*it_decimal_separator)) !=
        money_separators_.end()) {
      break;
    }
  }

  // If there are 3 digits after the last separator, we consider that a
  // thousands separator => the number is an int (e.g. 1.234 is considered int).
  // If there is no separator in number, also that number is an int.
  if (separator_back_index == 3 || it_decimal_separator == amount.begin()) {
    it_decimal_separator = amount.end();
  }

  if (!unilib_->ParseInt32(RemoveMoneySeparators(money_separators_, amount,
                                                 it_decimal_separator),
                           &data->money->amount_whole_part)) {
    TC3_LOG(ERROR) << "Could not parse the money whole part as int32 from the "
                      "amount: "
                   << data->money->unnormalized_amount;
    return false;
  }
  if (it_decimal_separator == amount.end()) {
    data->money->amount_decimal_part = 0;
  } else {
    const int amount_codepoints_size = amount.size_codepoints();
    if (!unilib_->ParseInt32(
            UnicodeText::Substring(
                amount, amount_codepoints_size - separator_back_index,
                amount_codepoints_size, /*do_copy=*/false),
            &data->money->amount_decimal_part)) {
      TC3_LOG(ERROR) << "Could not parse the money decimal part as int32 from "
                        "the amount: "
                     << data->money->unnormalized_amount;
      return false;
    }
  }

  *serialized_entity_data =
      PackFlatbuffer<libtextclassifier3::EntityData>(data.get());
  return true;
}

bool Annotator::RegexChunk(const UnicodeText& context_unicode,
                           const std::vector<int>& rules,
                           std::vector<AnnotatedSpan>* result,
                           bool is_serialized_entity_data_enabled) const {
  for (int pattern_id : rules) {
    const CompiledRegexPattern& regex_pattern = regex_patterns_[pattern_id];
    const auto matcher = regex_pattern.pattern->Matcher(context_unicode);
    if (!matcher) {
      TC3_LOG(ERROR) << "Could not get regex matcher for pattern: "
                     << pattern_id;
      return false;
    }

    int status = UniLib::RegexMatcher::kNoError;
    while (matcher->Find(&status) && status == UniLib::RegexMatcher::kNoError) {
      if (regex_pattern.config->verification_options()) {
        if (!VerifyRegexMatchCandidate(
                context_unicode.ToUTF8String(),
                regex_pattern.config->verification_options(),
                matcher->Group(1, &status).ToUTF8String(), matcher.get())) {
          continue;
        }
      }

      std::string serialized_entity_data;
      if (is_serialized_entity_data_enabled) {
        if (!SerializedEntityDataFromRegexMatch(
                regex_pattern.config, matcher.get(), &serialized_entity_data)) {
          TC3_LOG(ERROR) << "Could not get entity data.";
          return false;
        }

        // Further parsing unnormalized_amount for money into amount_whole_part
        // and amount_decimal_part. Can't do this with regexes because we cannot
        // have empty groups (amount_decimal_part might be an empty group).
        if (regex_pattern.config->collection_name()->str() ==
            Collections::Money()) {
          if (!ParseAndFillInMoneyAmount(&serialized_entity_data)) {
            if (model_->version() >= 706) {
              // This way of parsing money entity data is enabled for models
              // newer than v706 => logging errors only for them (b/156634162).
              TC3_LOG(ERROR) << "Could not parse and fill in money amount.";
            }
          }
        }
      }

      result->emplace_back();

      // Selection/annotation regular expressions need to specify a capturing
      // group specifying the selection.
      result->back().span =
          ComputeSelectionBoundaries(matcher.get(), regex_pattern.config);

      result->back().classification = {
          {regex_pattern.config->collection_name()->str(),
           regex_pattern.config->target_classification_score(),
           regex_pattern.config->priority_score()}};

      result->back().classification[0].serialized_entity_data =
          serialized_entity_data;
    }
  }
  return true;
}

bool Annotator::ModelChunk(int num_tokens, const TokenSpan& span_of_interest,
                           tflite::Interpreter* selection_interpreter,
                           const CachedFeatures& cached_features,
                           std::vector<TokenSpan>* chunks) const {
  const int max_selection_span =
      selection_feature_processor_->GetOptions()->max_selection_span();
  // The inference span is the span of interest expanded to include
  // max_selection_span tokens on either side, which is how far a selection can
  // stretch from the click.
  const TokenSpan inference_span = IntersectTokenSpans(
      ExpandTokenSpan(span_of_interest,
                      /*num_tokens_left=*/max_selection_span,
                      /*num_tokens_right=*/max_selection_span),
      {0, num_tokens});

  std::vector<ScoredChunk> scored_chunks;
  if (selection_feature_processor_->GetOptions()->bounds_sensitive_features() &&
      selection_feature_processor_->GetOptions()
          ->bounds_sensitive_features()
          ->enabled()) {
    if (!ModelBoundsSensitiveScoreChunks(
            num_tokens, span_of_interest, inference_span, cached_features,
            selection_interpreter, &scored_chunks)) {
      return false;
    }
  } else {
    if (!ModelClickContextScoreChunks(num_tokens, span_of_interest,
                                      cached_features, selection_interpreter,
                                      &scored_chunks)) {
      return false;
    }
  }
  std::sort(scored_chunks.rbegin(), scored_chunks.rend(),
            [](const ScoredChunk& lhs, const ScoredChunk& rhs) {
              return lhs.score < rhs.score;
            });

  // Traverse the candidate chunks from highest-scoring to lowest-scoring. Pick
  // them greedily as long as they do not overlap with any previously picked
  // chunks.
  std::vector<bool> token_used(TokenSpanSize(inference_span));
  chunks->clear();
  for (const ScoredChunk& scored_chunk : scored_chunks) {
    bool feasible = true;
    for (int i = scored_chunk.token_span.first;
         i < scored_chunk.token_span.second; ++i) {
      if (token_used[i - inference_span.first]) {
        feasible = false;
        break;
      }
    }

    if (!feasible) {
      continue;
    }

    for (int i = scored_chunk.token_span.first;
         i < scored_chunk.token_span.second; ++i) {
      token_used[i - inference_span.first] = true;
    }

    chunks->push_back(scored_chunk.token_span);
  }

  std::sort(chunks->begin(), chunks->end());

  return true;
}

namespace {
// Updates the value at the given key in the map to maximum of the current value
// and the given value, or simply inserts the value if the key is not yet there.
template <typename Map>
void UpdateMax(Map* map, typename Map::key_type key,
               typename Map::mapped_type value) {
  const auto it = map->find(key);
  if (it != map->end()) {
    it->second = std::max(it->second, value);
  } else {
    (*map)[key] = value;
  }
}
}  // namespace

bool Annotator::ModelClickContextScoreChunks(
    int num_tokens, const TokenSpan& span_of_interest,
    const CachedFeatures& cached_features,
    tflite::Interpreter* selection_interpreter,
    std::vector<ScoredChunk>* scored_chunks) const {
  const int max_batch_size = model_->selection_options()->batch_size();

  std::vector<float> all_features;
  std::map<TokenSpan, float> chunk_scores;
  for (int batch_start = span_of_interest.first;
       batch_start < span_of_interest.second; batch_start += max_batch_size) {
    const int batch_end =
        std::min(batch_start + max_batch_size, span_of_interest.second);

    // Prepare features for the whole batch.
    all_features.clear();
    all_features.reserve(max_batch_size * cached_features.OutputFeaturesSize());
    for (int click_pos = batch_start; click_pos < batch_end; ++click_pos) {
      cached_features.AppendClickContextFeaturesForClick(click_pos,
                                                         &all_features);
    }

    // Run batched inference.
    const int batch_size = batch_end - batch_start;
    const int features_size = cached_features.OutputFeaturesSize();
    TensorView<float> logits = selection_executor_->ComputeLogits(
        TensorView<float>(all_features.data(), {batch_size, features_size}),
        selection_interpreter);
    if (!logits.is_valid()) {
      TC3_LOG(ERROR) << "Couldn't compute logits.";
      return false;
    }
    if (logits.dims() != 2 || logits.dim(0) != batch_size ||
        logits.dim(1) !=
            selection_feature_processor_->GetSelectionLabelCount()) {
      TC3_LOG(ERROR) << "Mismatching output.";
      return false;
    }

    // Save results.
    for (int click_pos = batch_start; click_pos < batch_end; ++click_pos) {
      const std::vector<float> scores = ComputeSoftmax(
          logits.data() + logits.dim(1) * (click_pos - batch_start),
          logits.dim(1));
      for (int j = 0;
           j < selection_feature_processor_->GetSelectionLabelCount(); ++j) {
        TokenSpan relative_token_span;
        if (!selection_feature_processor_->LabelToTokenSpan(
                j, &relative_token_span)) {
          TC3_LOG(ERROR) << "Couldn't map the label to a token span.";
          return false;
        }
        const TokenSpan candidate_span = ExpandTokenSpan(
            SingleTokenSpan(click_pos), relative_token_span.first,
            relative_token_span.second);
        if (candidate_span.first >= 0 && candidate_span.second <= num_tokens) {
          UpdateMax(&chunk_scores, candidate_span, scores[j]);
        }
      }
    }
  }

  scored_chunks->clear();
  scored_chunks->reserve(chunk_scores.size());
  for (const auto& entry : chunk_scores) {
    scored_chunks->push_back(ScoredChunk{entry.first, entry.second});
  }

  return true;
}

bool Annotator::ModelBoundsSensitiveScoreChunks(
    int num_tokens, const TokenSpan& span_of_interest,
    const TokenSpan& inference_span, const CachedFeatures& cached_features,
    tflite::Interpreter* selection_interpreter,
    std::vector<ScoredChunk>* scored_chunks) const {
  const int max_selection_span =
      selection_feature_processor_->GetOptions()->max_selection_span();
  const int max_chunk_length = selection_feature_processor_->GetOptions()
                                       ->selection_reduced_output_space()
                                   ? max_selection_span + 1
                                   : 2 * max_selection_span + 1;
  const bool score_single_token_spans_as_zero =
      selection_feature_processor_->GetOptions()
          ->bounds_sensitive_features()
          ->score_single_token_spans_as_zero();

  scored_chunks->clear();
  if (score_single_token_spans_as_zero) {
    scored_chunks->reserve(TokenSpanSize(span_of_interest));
  }

  // Prepare all chunk candidates into one batch:
  //   - Are contained in the inference span
  //   - Have a non-empty intersection with the span of interest
  //   - Are at least one token long
  //   - Are not longer than the maximum chunk length
  std::vector<TokenSpan> candidate_spans;
  for (int start = inference_span.first; start < span_of_interest.second;
       ++start) {
    const int leftmost_end_index = std::max(start, span_of_interest.first) + 1;
    for (int end = leftmost_end_index;
         end <= inference_span.second && end - start <= max_chunk_length;
         ++end) {
      const TokenSpan candidate_span = {start, end};
      if (score_single_token_spans_as_zero &&
          TokenSpanSize(candidate_span) == 1) {
        // Do not include the single token span in the batch, add a zero score
        // for it directly to the output.
        scored_chunks->push_back(ScoredChunk{candidate_span, 0.0f});
      } else {
        candidate_spans.push_back(candidate_span);
      }
    }
  }

  const int max_batch_size = model_->selection_options()->batch_size();

  std::vector<float> all_features;
  scored_chunks->reserve(scored_chunks->size() + candidate_spans.size());
  for (int batch_start = 0; batch_start < candidate_spans.size();
       batch_start += max_batch_size) {
    const int batch_end = std::min(batch_start + max_batch_size,
                                   static_cast<int>(candidate_spans.size()));

    // Prepare features for the whole batch.
    all_features.clear();
    all_features.reserve(max_batch_size * cached_features.OutputFeaturesSize());
    for (int i = batch_start; i < batch_end; ++i) {
      cached_features.AppendBoundsSensitiveFeaturesForSpan(candidate_spans[i],
                                                           &all_features);
    }

    // Run batched inference.
    const int batch_size = batch_end - batch_start;
    const int features_size = cached_features.OutputFeaturesSize();
    TensorView<float> logits = selection_executor_->ComputeLogits(
        TensorView<float>(all_features.data(), {batch_size, features_size}),
        selection_interpreter);
    if (!logits.is_valid()) {
      TC3_LOG(ERROR) << "Couldn't compute logits.";
      return false;
    }
    if (logits.dims() != 2 || logits.dim(0) != batch_size ||
        logits.dim(1) != 1) {
      TC3_LOG(ERROR) << "Mismatching output.";
      return false;
    }

    // Save results.
    for (int i = batch_start; i < batch_end; ++i) {
      scored_chunks->push_back(
          ScoredChunk{candidate_spans[i], logits.data()[i - batch_start]});
    }
  }

  return true;
}

bool Annotator::DatetimeChunk(const UnicodeText& context_unicode,
                              int64 reference_time_ms_utc,
                              const std::string& reference_timezone,
                              const std::string& locales, ModeFlag mode,
                              AnnotationUsecase annotation_usecase,
                              bool is_serialized_entity_data_enabled,
                              std::vector<AnnotatedSpan>* result) const {
  std::vector<DatetimeParseResultSpan> datetime_spans;
  if (cfg_datetime_parser_) {
    if (!(model_->grammar_datetime_model()->enabled_modes() & mode)) {
      return true;
    }
    std::vector<Locale> parsed_locales;
    ParseLocales(locales, &parsed_locales);
    cfg_datetime_parser_->Parse(
        context_unicode.ToUTF8String(),
        ToDateAnnotationOptions(
            model_->grammar_datetime_model()->annotation_options(),
            reference_timezone, reference_time_ms_utc),
        parsed_locales, &datetime_spans);
  }

  if (datetime_parser_) {
    if (!datetime_parser_->Parse(context_unicode, reference_time_ms_utc,
                                 reference_timezone, locales, mode,
                                 annotation_usecase,
                                 /*anchor_start_end=*/false, &datetime_spans)) {
      return false;
    }
  }

  for (const DatetimeParseResultSpan& datetime_span : datetime_spans) {
    AnnotatedSpan annotated_span;
    annotated_span.span = datetime_span.span;
    for (const DatetimeParseResult& parse_result : datetime_span.data) {
      annotated_span.classification.emplace_back(
          PickCollectionForDatetime(parse_result),
          datetime_span.target_classification_score,
          datetime_span.priority_score);
      annotated_span.classification.back().datetime_parse_result = parse_result;
      if (is_serialized_entity_data_enabled) {
        annotated_span.classification.back().serialized_entity_data =
            CreateDatetimeSerializedEntityData(parse_result);
      }
    }
    annotated_span.source = AnnotatedSpan::Source::DATETIME;
    result->push_back(std::move(annotated_span));
  }
  return true;
}

const Model* Annotator::model() const { return model_; }
const reflection::Schema* Annotator::entity_data_schema() const {
  return entity_data_schema_;
}

const Model* ViewModel(const void* buffer, int size) {
  if (!buffer) {
    return nullptr;
  }

  return LoadAndVerifyModel(buffer, size);
}

bool Annotator::LookUpKnowledgeEntity(
    const std::string& id, std::string* serialized_knowledge_result) const {
  return knowledge_engine_ &&
         knowledge_engine_->LookUpEntity(id, serialized_knowledge_result);
}

}  // namespace libtextclassifier3
