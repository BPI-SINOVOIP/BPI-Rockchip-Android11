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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_ANNOTATIONS_ANNOTATION_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_ANNOTATIONS_ANNOTATION_H_

#include <string>
#include <vector>

#include "utils/base/integral_types.h"

namespace libtextclassifier3 {

struct AnnotationData;

// Define enum for each annotation.
enum GrammarAnnotationType {
  // Date&time like "May 1", "12:20pm", etc.
  DATETIME = 0,
  // Datetime range like "2pm - 3pm".
  DATETIME_RANGE = 1,
};

struct Property {
  // TODO(hassan): Replace the name with enum e.g. PropertyType.
  std::string name;
  // At most one of these will have any values.
  std::vector<bool> bool_values;
  std::vector<int64> int_values;
  std::vector<double> double_values;
  std::vector<std::string> string_values;
  std::vector<AnnotationData> annotation_data_values;
};

struct AnnotationData {
  // TODO(hassan): Replace it type with GrammarAnnotationType
  std::string type;
  std::vector<Property> properties;
};

// Represents an annotation instance.
// lets call it either AnnotationDetails
struct Annotation {
  // Codepoint offsets into the original text specifying the substring of the
  // text that was annotated.
  int32 begin;
  int32 end;

  // Annotation priority score which can be used to resolve conflict between
  // annotators.
  float annotator_priority_score;

  // Represents the details of the annotation instance, including the type of
  // the annotation instance and its properties.
  AnnotationData data;
};
}  // namespace libtextclassifier3
#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_ANNOTATIONS_ANNOTATION_H_
