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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_TYPES_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_TYPES_H_

#include <time.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "annotator/entity-data_generated.h"
#include "utils/base/integral_types.h"
#include "utils/base/logging.h"
#include "utils/flatbuffers.h"
#include "utils/optional.h"
#include "utils/variant.h"

namespace libtextclassifier3 {

constexpr int kInvalidIndex = -1;
constexpr int kSunday = 1;
constexpr int kMonday = 2;
constexpr int kTuesday = 3;
constexpr int kWednesday = 4;
constexpr int kThursday = 5;
constexpr int kFriday = 6;
constexpr int kSaturday = 7;

// Index for a 0-based array of tokens.
using TokenIndex = int;

// Index for a 0-based array of codepoints.
using CodepointIndex = int;

// Marks a span in a sequence of codepoints. The first element is the index of
// the first codepoint of the span, and the second element is the index of the
// codepoint one past the end of the span.
// TODO(b/71982294): Make it a struct.
using CodepointSpan = std::pair<CodepointIndex, CodepointIndex>;

inline bool SpansOverlap(const CodepointSpan& a, const CodepointSpan& b) {
  return a.first < b.second && b.first < a.second;
}

inline bool ValidNonEmptySpan(const CodepointSpan& span) {
  return span.first < span.second && span.first >= 0 && span.second >= 0;
}

template <typename T>
bool DoesCandidateConflict(
    const int considered_candidate, const std::vector<T>& candidates,
    const std::set<int, std::function<bool(int, int)>>& chosen_indices_set) {
  if (chosen_indices_set.empty()) {
    return false;
  }

  auto conflicting_it = chosen_indices_set.lower_bound(considered_candidate);
  // Check conflict on the right.
  if (conflicting_it != chosen_indices_set.end() &&
      SpansOverlap(candidates[considered_candidate].span,
                   candidates[*conflicting_it].span)) {
    return true;
  }

  // Check conflict on the left.
  // If we can't go more left, there can't be a conflict:
  if (conflicting_it == chosen_indices_set.begin()) {
    return false;
  }
  // Otherwise move one span left and insert if it doesn't overlap with the
  // candidate.
  --conflicting_it;
  if (!SpansOverlap(candidates[considered_candidate].span,
                    candidates[*conflicting_it].span)) {
    return false;
  }

  return true;
}

// Marks a span in a sequence of tokens. The first element is the index of the
// first token in the span, and the second element is the index of the token one
// past the end of the span.
// TODO(b/71982294): Make it a struct.
using TokenSpan = std::pair<TokenIndex, TokenIndex>;

// Returns the size of the token span. Assumes that the span is valid.
inline int TokenSpanSize(const TokenSpan& token_span) {
  return token_span.second - token_span.first;
}

// Returns a token span consisting of one token.
inline TokenSpan SingleTokenSpan(int token_index) {
  return {token_index, token_index + 1};
}

// Returns an intersection of two token spans. Assumes that both spans are valid
// and overlapping.
inline TokenSpan IntersectTokenSpans(const TokenSpan& token_span1,
                                     const TokenSpan& token_span2) {
  return {std::max(token_span1.first, token_span2.first),
          std::min(token_span1.second, token_span2.second)};
}

// Returns and expanded token span by adding a certain number of tokens on its
// left and on its right.
inline TokenSpan ExpandTokenSpan(const TokenSpan& token_span,
                                 int num_tokens_left, int num_tokens_right) {
  return {token_span.first - num_tokens_left,
          token_span.second + num_tokens_right};
}

// Token holds a token, its position in the original string and whether it was
// part of the input span.
struct Token {
  std::string value;
  CodepointIndex start;
  CodepointIndex end;

  // Whether the token is a padding token.
  bool is_padding;

  // Whether the token contains only white characters.
  bool is_whitespace;

  // Default constructor constructs the padding-token.
  Token()
      : Token(/*arg_value=*/"", /*arg_start=*/kInvalidIndex,
              /*arg_end=*/kInvalidIndex, /*is_padding=*/true,
              /*is_whitespace=*/false) {}

  Token(const std::string& arg_value, CodepointIndex arg_start,
        CodepointIndex arg_end)
      : Token(/*arg_value=*/arg_value, /*arg_start=*/arg_start,
              /*arg_end=*/arg_end, /*is_padding=*/false,
              /*is_whitespace=*/false) {}

  Token(const std::string& arg_value, CodepointIndex arg_start,
        CodepointIndex arg_end, bool is_padding, bool is_whitespace)
      : value(arg_value),
        start(arg_start),
        end(arg_end),
        is_padding(is_padding),
        is_whitespace(is_whitespace) {}

  bool operator==(const Token& other) const {
    return value == other.value && start == other.start && end == other.end &&
           is_padding == other.is_padding;
  }

  bool IsContainedInSpan(CodepointSpan span) const {
    return start >= span.first && end <= span.second;
  }
};

// Pretty-printing function for Token.
logging::LoggingStringStream& operator<<(logging::LoggingStringStream& stream,
                                         const Token& token);

enum DatetimeGranularity {
  GRANULARITY_UNKNOWN = -1,  // GRANULARITY_UNKNOWN is used as a proxy for this
                             // structure being uninitialized.
  GRANULARITY_YEAR = 0,
  GRANULARITY_MONTH = 1,
  GRANULARITY_WEEK = 2,
  GRANULARITY_DAY = 3,
  GRANULARITY_HOUR = 4,
  GRANULARITY_MINUTE = 5,
  GRANULARITY_SECOND = 6
};

// This struct represents a unit of date and time expression.
// Examples include:
// - In {March 21, 2019} datetime components are month: {March},
//   day of month: {21} and year: {2019}.
// - {8:00 am} contains hour: {8}, minutes: {0} and am/pm: {am}
struct DatetimeComponent {
  enum class ComponentType {
    UNSPECIFIED = 0,
    // Year of the date seen in the text match.
    YEAR = 1,
    // Month of the year starting with January = 1.
    MONTH = 2,
    // Week (7 days).
    WEEK = 3,
    // Day of week, start of the week is Sunday &  its value is 1.
    DAY_OF_WEEK = 4,
    // Day of the month starting with 1.
    DAY_OF_MONTH = 5,
    // Hour of the day with a range of 0-23,
    // values less than 12 need the AMPM field below or heuristics
    // to definitively determine the time.
    HOUR = 6,
    // Minute of the hour with a range of 0-59.
    MINUTE = 7,
    // Seconds of the minute with a range of 0-59.
    SECOND = 8,
    // Meridiem field where 0 == AM, 1 == PM.
    MERIDIEM = 9,
    // Number of hours offset from UTC this date time is in.
    ZONE_OFFSET = 10,
    // Number of hours offest for DST.
    DST_OFFSET = 11,
  };

  // TODO(hassan): Remove RelativeQualifier as in the presence of relative
  //               count RelativeQualifier is redundant.
  // Enum to represent the relative DateTimeComponent e.g. "next Monday",
  // "the following day", "tomorrow".
  enum class RelativeQualifier {
    UNSPECIFIED = 0,
    NEXT = 1,
    THIS = 2,
    LAST = 3,
    NOW = 4,
    TOMORROW = 5,
    YESTERDAY = 6,
    PAST = 7,
    FUTURE = 8
  };

  bool operator==(const DatetimeComponent& other) const {
    return component_type == other.component_type &&
           relative_qualifier == other.relative_qualifier &&
           relative_count == other.relative_count && value == other.value;
  }

  bool ShouldRoundToGranularity() const;

  ComponentType component_type = ComponentType::UNSPECIFIED;
  RelativeQualifier relative_qualifier = RelativeQualifier::UNSPECIFIED;

  // Represents the absolute value of DateTime components.
  int value = 0;
  // The number of units of change present in the relative DateTimeComponent.
  int relative_count = 0;

  DatetimeComponent() = default;

  explicit DatetimeComponent(ComponentType arg_component_type,
                             RelativeQualifier arg_relative_qualifier,
                             int arg_value, int arg_relative_count)
      : component_type(arg_component_type),
        relative_qualifier(arg_relative_qualifier),
        value(arg_value),
        relative_count(arg_relative_count) {}
};

// Utility method to calculate Returns the finest granularity of
// DatetimeComponents.
DatetimeGranularity GetFinestGranularity(
    const std::vector<DatetimeComponent>& datetime_component);

// Return the 'DatetimeComponent' from collection filter by component type.
Optional<DatetimeComponent> GetDatetimeComponent(
    const std::vector<DatetimeComponent>& datetime_components,
    const DatetimeComponent::ComponentType& component_type);

struct DatetimeParseResult {
  // The absolute time in milliseconds since the epoch in UTC.
  int64 time_ms_utc;

  // The precision of the estimate then in to calculating the milliseconds
  DatetimeGranularity granularity;

  // List of parsed DateTimeComponent.
  std::vector<DatetimeComponent> datetime_components;

  DatetimeParseResult() : time_ms_utc(0), granularity(GRANULARITY_UNKNOWN) {}

  DatetimeParseResult(int64 arg_time_ms_utc,
                      DatetimeGranularity arg_granularity,
                      std::vector<DatetimeComponent> arg_datetime__components)
      : time_ms_utc(arg_time_ms_utc),
        granularity(arg_granularity),
        datetime_components(arg_datetime__components) {}

  bool IsSet() const { return granularity != GRANULARITY_UNKNOWN; }

  bool operator==(const DatetimeParseResult& other) const {
    return granularity == other.granularity &&
           time_ms_utc == other.time_ms_utc &&
           datetime_components == other.datetime_components;
  }
};

const float kFloatCompareEpsilon = 1e-5;

struct DatetimeParseResultSpan {
  CodepointSpan span;
  std::vector<DatetimeParseResult> data;
  float target_classification_score;
  float priority_score;

  DatetimeParseResultSpan()
      : target_classification_score(-1.0), priority_score(-1.0) {}

  DatetimeParseResultSpan(const CodepointSpan& span,
                          const std::vector<DatetimeParseResult>& data,
                          const float target_classification_score,
                          const float priority_score) {
    this->span = span;
    this->data = data;
    this->target_classification_score = target_classification_score;
    this->priority_score = priority_score;
  }

  bool operator==(const DatetimeParseResultSpan& other) const {
    return span == other.span && data == other.data &&
           std::abs(target_classification_score -
                    other.target_classification_score) < kFloatCompareEpsilon &&
           std::abs(priority_score - other.priority_score) <
               kFloatCompareEpsilon;
  }
};

// Pretty-printing function for DatetimeParseResultSpan.
logging::LoggingStringStream& operator<<(logging::LoggingStringStream& stream,
                                         const DatetimeParseResultSpan& value);

// This struct contains information intended to uniquely identify a device
// contact. Instances are created by the Knowledge Engine, and dereferenced by
// the Contact Engine.
struct ContactPointer {
  std::string focus_contact_id;
  std::string device_id;
  std::string device_contact_id;
  std::string contact_name;
  std::string contact_name_hash;

  bool operator==(const ContactPointer& other) const {
    return focus_contact_id == other.focus_contact_id &&
           device_id == other.device_id &&
           device_contact_id == other.device_contact_id &&
           contact_name == other.contact_name &&
           contact_name_hash == other.contact_name_hash;
  }
};

struct ClassificationResult {
  std::string collection;
  float score;
  DatetimeParseResult datetime_parse_result;
  std::string serialized_knowledge_result;
  ContactPointer contact_pointer;
  std::string contact_name, contact_given_name, contact_family_name,
      contact_nickname, contact_email_address, contact_phone_number, contact_id;
  std::string app_name, app_package_name;
  int64 numeric_value;
  double numeric_double_value;

  // Length of the parsed duration in milliseconds.
  int64 duration_ms;

  // Internal score used for conflict resolution.
  float priority_score;


  // Entity data information.
  std::string serialized_entity_data;
  const EntityData* entity_data() const {
    return LoadAndVerifyFlatbuffer<EntityData>(serialized_entity_data.data(),
                                               serialized_entity_data.size());
  }

  explicit ClassificationResult()
      : score(-1.0f),
        numeric_value(0),
        numeric_double_value(0.),
        duration_ms(0),
        priority_score(-1.0) {}

  ClassificationResult(const std::string& arg_collection, float arg_score)
      : collection(arg_collection),
        score(arg_score),
        numeric_value(0),
        numeric_double_value(0.),
        duration_ms(0),
        priority_score(arg_score) {}

  ClassificationResult(const std::string& arg_collection, float arg_score,
                       float arg_priority_score)
      : collection(arg_collection),
        score(arg_score),
        numeric_value(0),
        numeric_double_value(0.),
        duration_ms(0),
        priority_score(arg_priority_score) {}

  bool operator!=(const ClassificationResult& other) const {
    return !(*this == other);
  }

  bool operator==(const ClassificationResult& other) const;
};

// Aliases for long enum values.
const AnnotationUsecase ANNOTATION_USECASE_SMART =
    AnnotationUsecase_ANNOTATION_USECASE_SMART;
const AnnotationUsecase ANNOTATION_USECASE_RAW =
    AnnotationUsecase_ANNOTATION_USECASE_RAW;

struct LocationContext {
  // User location latitude in degrees.
  double user_location_lat = 180.;

  // User location longitude in degrees.
  double user_location_lng = 360.;

  // The estimated horizontal accuracy of the user location in meters.
  // Analogous to android.location.Location accuracy.
  float user_location_accuracy_meters = 0.f;

  bool operator==(const LocationContext& other) const {
    return std::fabs(this->user_location_lat - other.user_location_lat) <
               1e-8 &&
           std::fabs(this->user_location_lng - other.user_location_lng) <
               1e-8 &&
           std::fabs(this->user_location_accuracy_meters -
                     other.user_location_accuracy_meters) < 1e-8;
  }
};

struct BaseOptions {
  // Comma-separated list of locale specification for the input text (BCP 47
  // tags).
  std::string locales;

  // Comma-separated list of BCP 47 language tags.
  std::string detected_text_language_tags;

  // Tailors the output annotations according to the specified use-case.
  AnnotationUsecase annotation_usecase = ANNOTATION_USECASE_SMART;

  // The location context passed along with each annotation.
  Optional<LocationContext> location_context;

  bool operator==(const BaseOptions& other) const {
    bool location_context_equality = this->location_context.has_value() ==
                                     other.location_context.has_value();
    if (this->location_context.has_value() &&
        other.location_context.has_value()) {
      location_context_equality =
          this->location_context.value() == other.location_context.value();
    }
    return this->locales == other.locales &&
           this->annotation_usecase == other.annotation_usecase &&
           this->detected_text_language_tags ==
               other.detected_text_language_tags &&
           location_context_equality;
  }
};

struct DatetimeOptions {
  // For parsing relative datetimes, the reference now time against which the
  // relative datetimes get resolved.
  // UTC milliseconds since epoch.
  int64 reference_time_ms_utc = 0;

  // Timezone in which the input text was written (format as accepted by ICU).
  std::string reference_timezone;

  bool operator==(const DatetimeOptions& other) const {
    return this->reference_time_ms_utc == other.reference_time_ms_utc &&
           this->reference_timezone == other.reference_timezone;
  }
};

struct SelectionOptions : public BaseOptions {};

struct ClassificationOptions : public BaseOptions, public DatetimeOptions {
  // Comma-separated list of language tags which the user can read and
  // understand (BCP 47).
  std::string user_familiar_language_tags;

  bool operator==(const ClassificationOptions& other) const {
    return this->user_familiar_language_tags ==
               other.user_familiar_language_tags &&
           BaseOptions::operator==(other) && DatetimeOptions::operator==(other);
  }
};

struct Permissions {
  // If true the user location can be used to provide better annotations.
  bool has_location_permission = true;
  // If true, annotators can use personal data to provide personalized
  // annotations.
  bool has_personalization_permission = true;

  bool operator==(const Permissions& other) const {
    return this->has_location_permission == other.has_location_permission &&
           this->has_personalization_permission ==
               other.has_personalization_permission;
  }
};

struct AnnotationOptions : public BaseOptions, public DatetimeOptions {
  // List of entity types that should be used for annotation.
  std::unordered_set<std::string> entity_types;

  // If true, serialized_entity_data in the results is populated."
  bool is_serialized_entity_data_enabled = false;

  // Defines the permissions for the annotators.
  Permissions permissions;

  bool operator==(const AnnotationOptions& other) const {
    return this->is_serialized_entity_data_enabled ==
               other.is_serialized_entity_data_enabled &&
           this->permissions == other.permissions &&
           this->entity_types == other.entity_types &&
           BaseOptions::operator==(other) && DatetimeOptions::operator==(other);
  }
};

// Returns true when ClassificationResults are euqal up to scores.
bool ClassificationResultsEqualIgnoringScoresAndSerializedEntityData(
    const ClassificationResult& a, const ClassificationResult& b);

// Pretty-printing function for ClassificationResult.
logging::LoggingStringStream& operator<<(logging::LoggingStringStream& stream,
                                         const ClassificationResult& result);

// Pretty-printing function for std::vector<ClassificationResult>.
logging::LoggingStringStream& operator<<(
    logging::LoggingStringStream& stream,
    const std::vector<ClassificationResult>& results);

// Represents a result of Annotate call.
struct AnnotatedSpan {
  enum class Source { OTHER, KNOWLEDGE, DURATION, DATETIME, PERSON_NAME };

  // Unicode codepoint indices in the input string.
  CodepointSpan span = {kInvalidIndex, kInvalidIndex};

  // Classification result for the span.
  std::vector<ClassificationResult> classification;

  // The source of the annotation, used in conflict resolution.
  Source source = Source::OTHER;

  AnnotatedSpan() = default;

  AnnotatedSpan(CodepointSpan arg_span,
                std::vector<ClassificationResult> arg_classification)
      : span(arg_span), classification(std::move(arg_classification)) {}

  AnnotatedSpan(CodepointSpan arg_span,
                std::vector<ClassificationResult> arg_classification,
                Source arg_source)
      : span(arg_span),
        classification(std::move(arg_classification)),
        source(arg_source) {}
};

struct InputFragment {
  std::string text;

  // If present will override the AnnotationOptions reference time and timezone
  // when annotating this specific string fragment.
  Optional<DatetimeOptions> datetime_options;
};

// Pretty-printing function for AnnotatedSpan.
logging::LoggingStringStream& operator<<(logging::LoggingStringStream& stream,
                                         const AnnotatedSpan& span);

// StringPiece analogue for std::vector<T>.
template <class T>
class VectorSpan {
 public:
  VectorSpan() : begin_(), end_() {}
  VectorSpan(const std::vector<T>& v)  // NOLINT(runtime/explicit)
      : begin_(v.begin()), end_(v.end()) {}
  VectorSpan(typename std::vector<T>::const_iterator begin,
             typename std::vector<T>::const_iterator end)
      : begin_(begin), end_(end) {}

  const T& operator[](typename std::vector<T>::size_type i) const {
    return *(begin_ + i);
  }

  int size() const { return end_ - begin_; }
  typename std::vector<T>::const_iterator begin() const { return begin_; }
  typename std::vector<T>::const_iterator end() const { return end_; }
  const float* data() const { return &(*begin_); }

 private:
  typename std::vector<T>::const_iterator begin_;
  typename std::vector<T>::const_iterator end_;
};

// Class to provide representation of date and time expressions
class DatetimeParsedData {
 public:
  // Function to set the absolute value of DateTimeComponent for the given
  // FieldType, if the field is not present it will create the field and set
  // the value.
  void SetAbsoluteValue(const DatetimeComponent::ComponentType& field_type,
                        int value);

  // Function to set the relative value of DateTimeComponent, if the field is
  // not present the function will create the field and set the relative value.
  void SetRelativeValue(
      const DatetimeComponent::ComponentType& field_type,
      const DatetimeComponent::RelativeQualifier& relative_value);

  // Add collection of 'DatetimeComponent' to 'DatetimeParsedData'.
  void AddDatetimeComponents(
      const std::vector<DatetimeComponent>& datetime_components);

  // Function to set the relative count of DateTimeComponent, if the field is
  // not present the function will create the field and set the count.
  void SetRelativeCount(const DatetimeComponent::ComponentType& field_type,
                        int relative_count);

  // Function to populate the absolute value of the FieldType and return true.
  // In case of no FieldType function will return false.
  bool GetFieldValue(const DatetimeComponent::ComponentType& field_type,
                     int* field_value) const;

  // Function to populate the relative value of the FieldType and return true.
  // In case of no relative value function will return false.
  bool GetRelativeValue(
      const DatetimeComponent::ComponentType& field_type,
      DatetimeComponent::RelativeQualifier* relative_value) const;

  // Returns relative DateTimeComponent from the parsed DateTime span.
  void GetRelativeDatetimeComponents(
      std::vector<DatetimeComponent>* date_time_components) const;

  // Returns DateTimeComponent from the parsed DateTime span.
  void GetDatetimeComponents(
      std::vector<DatetimeComponent>* date_time_components) const;

  // Represent the granularity of the Parsed DateTime span. The function will
  // return “GRANULARITY_UNKNOWN” if no datetime field is set.
  DatetimeGranularity GetFinestGranularity() const;

  // Utility function to check if DateTimeParsedData has FieldType initialized.
  bool HasFieldType(const DatetimeComponent::ComponentType& field_type) const;

  // Function to check if DateTimeParsedData has relative DateTimeComponent for
  // given FieldType.
  bool HasRelativeValue(
      const DatetimeComponent::ComponentType& field_type) const;

  // Function to check if DateTimeParsedData has absolute value
  // DateTimeComponent for given FieldType.
  bool HasAbsoluteValue(
      const DatetimeComponent::ComponentType& field_type) const;

  // Function to check if DateTimeParsedData has any DateTimeComponent.
  bool IsEmpty() const;

 private:
  DatetimeComponent& GetOrCreateDatetimeComponent(

      const DatetimeComponent::ComponentType& component_type);

  std::map<DatetimeComponent::ComponentType, DatetimeComponent>
      date_time_components_;
};

// Pretty-printing function for DateTimeParsedData.
logging::LoggingStringStream& operator<<(logging::LoggingStringStream& stream,
                                         const DatetimeParsedData& data);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_TYPES_H_
