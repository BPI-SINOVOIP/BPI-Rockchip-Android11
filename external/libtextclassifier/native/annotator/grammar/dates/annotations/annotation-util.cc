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

#include "annotator/grammar/dates/annotations/annotation-util.h"

#include <algorithm>

#include "utils/base/logging.h"

namespace libtextclassifier3 {

int GetPropertyIndex(StringPiece name, const AnnotationData& annotation_data) {
  for (int i = 0; i < annotation_data.properties.size(); ++i) {
    if (annotation_data.properties[i].name == name.ToString()) {
      return i;
    }
  }
  return -1;
}

int GetPropertyIndex(StringPiece name, const Annotation& annotation) {
  return GetPropertyIndex(name, annotation.data);
}

int GetIntProperty(StringPiece name, const Annotation& annotation) {
  return GetIntProperty(name, annotation.data);
}

int GetIntProperty(StringPiece name, const AnnotationData& annotation_data) {
  const int index = GetPropertyIndex(name, annotation_data);
  if (index < 0) {
    TC3_DCHECK_GE(index, 0)
        << "No property with name " << name.ToString() << ".";
    return 0;
  }

  if (annotation_data.properties.at(index).int_values.size() != 1) {
    TC3_DCHECK_EQ(annotation_data.properties[index].int_values.size(), 1);
    return 0;
  }

  return annotation_data.properties.at(index).int_values.at(0);
}

int AddIntProperty(StringPiece name, int value, Annotation* annotation) {
  return AddRepeatedIntProperty(name, &value, 1, annotation);
}

int AddIntProperty(StringPiece name, int value,
                   AnnotationData* annotation_data) {
  return AddRepeatedIntProperty(name, &value, 1, annotation_data);
}

int AddRepeatedIntProperty(StringPiece name, const int* start, int size,
                           Annotation* annotation) {
  return AddRepeatedIntProperty(name, start, size, &annotation->data);
}

int AddRepeatedIntProperty(StringPiece name, const int* start, int size,
                           AnnotationData* annotation_data) {
  Property property;
  property.name = name.ToString();
  auto first = start;
  auto last = start + size;
  while (first != last) {
    property.int_values.push_back(*first);
    first++;
  }
  annotation_data->properties.push_back(property);
  return annotation_data->properties.size() - 1;
}

int AddAnnotationDataProperty(const std::string& key,
                              const AnnotationData& value,
                              AnnotationData* annotation_data) {
  Property property;
  property.name = key;
  property.annotation_data_values.push_back(value);
  annotation_data->properties.push_back(property);
  return annotation_data->properties.size() - 1;
}

int AddAnnotationDataProperty(const std::string& key,
                              const AnnotationData& value,
                              Annotation* annotation) {
  return AddAnnotationDataProperty(key, value, &annotation->data);
}
}  // namespace libtextclassifier3
