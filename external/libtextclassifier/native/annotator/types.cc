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

#include "annotator/types.h"

#include <vector>

#include "utils/optional.h"

namespace libtextclassifier3 {

logging::LoggingStringStream& operator<<(logging::LoggingStringStream& stream,
                                         const Token& token) {
  if (!token.is_padding) {
    return stream << "Token(\"" << token.value << "\", " << token.start << ", "
                  << token.end << ")";
  } else {
    return stream << "Token()";
  }
}

bool DatetimeComponent::ShouldRoundToGranularity() const {
  // Don't round to the granularity for relative expressions that specify the
  // distance. So that, e.g. "in 2 hours" when it's 8:35:03 will result in
  // 10:35:03.
  if (relative_qualifier == RelativeQualifier::UNSPECIFIED) {
    return false;
  }
  if (relative_qualifier == RelativeQualifier::NEXT ||
      relative_qualifier == RelativeQualifier::TOMORROW ||
      relative_qualifier == RelativeQualifier::YESTERDAY ||
      relative_qualifier == RelativeQualifier::LAST ||
      relative_qualifier == RelativeQualifier::THIS ||
      relative_qualifier == RelativeQualifier::NOW) {
    return true;
  }
  return false;
}

namespace {
std::string FormatMillis(int64 time_ms_utc) {
  long time_seconds = time_ms_utc / 1000;  // NOLINT
  char buffer[512];
  strftime(buffer, sizeof(buffer), "%a %Y-%m-%d %H:%M:%S %Z",
           localtime(&time_seconds));
  return std::string(buffer);
}
}  // namespace

std::string ComponentTypeToString(
    const DatetimeComponent::ComponentType& component_type) {
  switch (component_type) {
    case DatetimeComponent::ComponentType::UNSPECIFIED:
      return "UNSPECIFIED";
    case DatetimeComponent::ComponentType::YEAR:
      return "YEAR";
    case DatetimeComponent::ComponentType::MONTH:
      return "MONTH";
    case DatetimeComponent::ComponentType::WEEK:
      return "WEEK";
    case DatetimeComponent::ComponentType::DAY_OF_WEEK:
      return "DAY_OF_WEEK";
    case DatetimeComponent::ComponentType::DAY_OF_MONTH:
      return "DAY_OF_MONTH";
    case DatetimeComponent::ComponentType::HOUR:
      return "HOUR";
    case DatetimeComponent::ComponentType::MINUTE:
      return "MINUTE";
    case DatetimeComponent::ComponentType::SECOND:
      return "SECOND";
    case DatetimeComponent::ComponentType::MERIDIEM:
      return "MERIDIEM";
    case DatetimeComponent::ComponentType::ZONE_OFFSET:
      return "ZONE_OFFSET";
    case DatetimeComponent::ComponentType::DST_OFFSET:
      return "DST_OFFSET";
    default:
      return "";
  }
}

std::string RelativeQualifierToString(
    const DatetimeComponent::RelativeQualifier& relative_qualifier) {
  switch (relative_qualifier) {
    case DatetimeComponent::RelativeQualifier::UNSPECIFIED:
      return "UNSPECIFIED";
    case DatetimeComponent::RelativeQualifier::NEXT:
      return "NEXT";
    case DatetimeComponent::RelativeQualifier::THIS:
      return "THIS";
    case DatetimeComponent::RelativeQualifier::LAST:
      return "LAST";
    case DatetimeComponent::RelativeQualifier::NOW:
      return "NOW";
    case DatetimeComponent::RelativeQualifier::TOMORROW:
      return "TOMORROW";
    case DatetimeComponent::RelativeQualifier::YESTERDAY:
      return "YESTERDAY";
    case DatetimeComponent::RelativeQualifier::PAST:
      return "PAST";
    case DatetimeComponent::RelativeQualifier::FUTURE:
      return "FUTURE";
    default:
      return "";
  }
}

logging::LoggingStringStream& operator<<(logging::LoggingStringStream& stream,
                                         const DatetimeParseResultSpan& value) {
  stream << "DatetimeParseResultSpan({" << value.span.first << ", "
         << value.span.second << "}, "
         << "/*target_classification_score=*/ "
         << value.target_classification_score << "/*priority_score=*/"
         << value.priority_score << " {";
  for (const DatetimeParseResult& data : value.data) {
    stream << "{/*time_ms_utc=*/ " << data.time_ms_utc << " /* "
           << FormatMillis(data.time_ms_utc) << " */, /*granularity=*/ "
           << data.granularity << ", /*datetime_components=*/ ";
    for (const DatetimeComponent& datetime_comp : data.datetime_components) {
      stream << "{/*component_type=*/ "
             << ComponentTypeToString(datetime_comp.component_type)
             << " /*relative_qualifier=*/ "
             << RelativeQualifierToString(datetime_comp.relative_qualifier)
             << " /*value=*/ " << datetime_comp.value << " /*relative_count=*/ "
             << datetime_comp.relative_count << "}, ";
    }
    stream << "}, ";
  }
  stream << "})";
  return stream;
}

bool ClassificationResult::operator==(const ClassificationResult& other) const {
  return ClassificationResultsEqualIgnoringScoresAndSerializedEntityData(
             *this, other) &&
         fabs(score - other.score) < 0.001 &&
         fabs(priority_score - other.priority_score) < 0.001 &&
         serialized_entity_data == other.serialized_entity_data;
}

bool ClassificationResultsEqualIgnoringScoresAndSerializedEntityData(
    const ClassificationResult& a, const ClassificationResult& b) {
  return a.collection == b.collection &&
         a.datetime_parse_result == b.datetime_parse_result &&
         a.serialized_knowledge_result == b.serialized_knowledge_result &&
         a.contact_pointer == b.contact_pointer &&
         a.contact_name == b.contact_name &&
         a.contact_given_name == b.contact_given_name &&
         a.contact_family_name == b.contact_family_name &&
         a.contact_nickname == b.contact_nickname &&
         a.contact_email_address == b.contact_email_address &&
         a.contact_phone_number == b.contact_phone_number &&
         a.contact_id == b.contact_id &&
         a.app_package_name == b.app_package_name &&
         a.numeric_value == b.numeric_value &&
         fabs(a.numeric_double_value - b.numeric_double_value) < 0.001 &&
         a.duration_ms == b.duration_ms;
}

logging::LoggingStringStream& operator<<(logging::LoggingStringStream& stream,
                                         const ClassificationResult& result) {
  return stream << "ClassificationResult(" << result.collection
                << ", /*score=*/ " << result.score << ", /*priority_score=*/ "
                << result.priority_score << ")";
}

logging::LoggingStringStream& operator<<(
    logging::LoggingStringStream& stream,
    const std::vector<ClassificationResult>& results) {
  stream = stream << "{\n";
  for (const ClassificationResult& result : results) {
    stream = stream << "    " << result << "\n";
  }
  stream = stream << "}";
  return stream;
}

logging::LoggingStringStream& operator<<(logging::LoggingStringStream& stream,
                                         const AnnotatedSpan& span) {
  std::string best_class;
  float best_score = -1;
  if (!span.classification.empty()) {
    best_class = span.classification[0].collection;
    best_score = span.classification[0].score;
  }
  return stream << "Span(" << span.span.first << ", " << span.span.second
                << ", " << best_class << ", " << best_score << ")";
}

logging::LoggingStringStream& operator<<(logging::LoggingStringStream& stream,
                                         const DatetimeParsedData& data) {
  std::vector<DatetimeComponent> date_time_components;
  data.GetDatetimeComponents(&date_time_components);
  stream = stream << "DatetimeParsedData { \n";
  for (const DatetimeComponent& c : date_time_components) {
    stream = stream << " DatetimeComponent { \n";
    stream = stream << "  Component Type:" << static_cast<int>(c.component_type)
                    << "\n";
    stream = stream << "  Value:" << c.value << "\n";
    stream = stream << "  Relative Qualifier:"
                    << static_cast<int>(c.relative_qualifier) << "\n";
    stream = stream << "  Relative Count:" << c.relative_count << "\n";
    stream = stream << " } \n";
  }
  stream = stream << "}";
  return stream;
}

void DatetimeParsedData::SetAbsoluteValue(
    const DatetimeComponent::ComponentType& field_type, int value) {
  GetOrCreateDatetimeComponent(field_type).value = value;
}

void DatetimeParsedData::SetRelativeValue(
    const DatetimeComponent::ComponentType& field_type,
    const DatetimeComponent::RelativeQualifier& relative_value) {
  GetOrCreateDatetimeComponent(field_type).relative_qualifier = relative_value;
}

void DatetimeParsedData::SetRelativeCount(
    const DatetimeComponent::ComponentType& field_type, int relative_count) {
  GetOrCreateDatetimeComponent(field_type).relative_count = relative_count;
}

void DatetimeParsedData::AddDatetimeComponents(
    const std::vector<DatetimeComponent>& datetime_components) {
  for (const DatetimeComponent& datetime_component : datetime_components) {
    date_time_components_.insert(
        {datetime_component.component_type, datetime_component});
  }
}

bool DatetimeParsedData::HasFieldType(
    const DatetimeComponent::ComponentType& field_type) const {
  if (date_time_components_.find(field_type) == date_time_components_.end()) {
    return false;
  }
  return true;
}

bool DatetimeParsedData::GetFieldValue(
    const DatetimeComponent::ComponentType& field_type,
    int* field_value) const {
  if (HasFieldType(field_type)) {
    *field_value = date_time_components_.at(field_type).value;
    return true;
  }
  return false;
}

bool DatetimeParsedData::GetRelativeValue(
    const DatetimeComponent::ComponentType& field_type,
    DatetimeComponent::RelativeQualifier* relative_value) const {
  if (HasFieldType(field_type)) {
    *relative_value = date_time_components_.at(field_type).relative_qualifier;
    return true;
  }
  return false;
}

bool DatetimeParsedData::HasRelativeValue(
    const DatetimeComponent::ComponentType& field_type) const {
  if (HasFieldType(field_type)) {
    return date_time_components_.at(field_type).relative_qualifier !=
           DatetimeComponent::RelativeQualifier::UNSPECIFIED;
  }
  return false;
}

bool DatetimeParsedData::HasAbsoluteValue(
    const DatetimeComponent::ComponentType& field_type) const {
  return HasFieldType(field_type) && !HasRelativeValue(field_type);
}

bool DatetimeParsedData::IsEmpty() const {
  return date_time_components_.empty();
}

void DatetimeParsedData::GetRelativeDatetimeComponents(
    std::vector<DatetimeComponent>* date_time_components) const {
  for (auto it = date_time_components_.begin();
       it != date_time_components_.end(); it++) {
    if (it->second.relative_qualifier !=
        DatetimeComponent::RelativeQualifier::UNSPECIFIED) {
      date_time_components->push_back(it->second);
    }
  }
}

void DatetimeParsedData::GetDatetimeComponents(
    std::vector<DatetimeComponent>* date_time_components) const {
  for (auto it = date_time_components_.begin();
       it != date_time_components_.end(); it++) {
    date_time_components->push_back(it->second);
  }
}

DatetimeComponent& DatetimeParsedData::GetOrCreateDatetimeComponent(
    const DatetimeComponent::ComponentType& component_type) {
  auto result =
      date_time_components_
          .insert(
              {component_type,
               DatetimeComponent(
                   component_type,
                   DatetimeComponent::RelativeQualifier::UNSPECIFIED, 0, 0)})
          .first;
  return result->second;
}

namespace {
DatetimeGranularity GetFinestGranularityFromComponentTypes(
    const std::vector<DatetimeComponent::ComponentType>&
        datetime_component_types) {
  DatetimeGranularity granularity = DatetimeGranularity::GRANULARITY_UNKNOWN;
  for (const auto& component_type : datetime_component_types) {
    switch (component_type) {
      case DatetimeComponent::ComponentType::YEAR:
        if (granularity < DatetimeGranularity::GRANULARITY_YEAR) {
          granularity = DatetimeGranularity::GRANULARITY_YEAR;
        }
        break;

      case DatetimeComponent::ComponentType::MONTH:
        if (granularity < DatetimeGranularity::GRANULARITY_MONTH) {
          granularity = DatetimeGranularity::GRANULARITY_MONTH;
        }
        break;

      case DatetimeComponent::ComponentType::WEEK:
        if (granularity < DatetimeGranularity::GRANULARITY_WEEK) {
          granularity = DatetimeGranularity::GRANULARITY_WEEK;
        }
        break;

      case DatetimeComponent::ComponentType::DAY_OF_WEEK:
      case DatetimeComponent::ComponentType::DAY_OF_MONTH:
        if (granularity < DatetimeGranularity::GRANULARITY_DAY) {
          granularity = DatetimeGranularity::GRANULARITY_DAY;
        }
        break;

      case DatetimeComponent::ComponentType::HOUR:
        if (granularity < DatetimeGranularity::GRANULARITY_HOUR) {
          granularity = DatetimeGranularity::GRANULARITY_HOUR;
        }
        break;

      case DatetimeComponent::ComponentType::MINUTE:
        if (granularity < DatetimeGranularity::GRANULARITY_MINUTE) {
          granularity = DatetimeGranularity::GRANULARITY_MINUTE;
        }
        break;

      case DatetimeComponent::ComponentType::SECOND:
        if (granularity < DatetimeGranularity::GRANULARITY_SECOND) {
          granularity = DatetimeGranularity::GRANULARITY_SECOND;
        }
        break;

      case DatetimeComponent::ComponentType::MERIDIEM:
      case DatetimeComponent::ComponentType::ZONE_OFFSET:
      case DatetimeComponent::ComponentType::DST_OFFSET:
      default:
        break;
    }
  }
  return granularity;
}
}  // namespace

DatetimeGranularity DatetimeParsedData::GetFinestGranularity() const {
  std::vector<DatetimeComponent::ComponentType> component_types;
  std::transform(date_time_components_.begin(), date_time_components_.end(),
                 std::back_inserter(component_types),
                 [](const std::map<DatetimeComponent::ComponentType,
                                   DatetimeComponent>::value_type& pair) {
                   return pair.first;
                 });
  return GetFinestGranularityFromComponentTypes(component_types);
}

Optional<DatetimeComponent> GetDatetimeComponent(
    const std::vector<DatetimeComponent>& datetime_components,
    const DatetimeComponent::ComponentType& component_type) {
  for (auto datetime_component : datetime_components) {
    if (datetime_component.component_type == component_type) {
      return Optional<DatetimeComponent>(datetime_component);
    }
  }
  return Optional<DatetimeComponent>();
}

// Returns the granularity of the DatetimeComponents.
DatetimeGranularity GetFinestGranularity(
    const std::vector<DatetimeComponent>& datetime_component) {
  std::vector<DatetimeComponent::ComponentType> component_types;
  std::transform(datetime_component.begin(), datetime_component.end(),
                 std::back_inserter(component_types),
                 [](const DatetimeComponent& component) {
                   return component.component_type;
                 });
  return GetFinestGranularityFromComponentTypes(component_types);
}

}  // namespace libtextclassifier3
