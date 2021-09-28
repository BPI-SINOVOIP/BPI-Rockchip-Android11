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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_ANNOTATIONS_ANNOTATION_UTIL_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_ANNOTATIONS_ANNOTATION_UTIL_H_

#include "annotator/grammar/dates/annotations/annotation.h"
#include "utils/strings/stringpiece.h"

namespace libtextclassifier3 {

// Return the index of property in annotation.data().properties().
// Return -1 if the property does not exist.
int GetPropertyIndex(StringPiece name, const Annotation& annotation);

// Return the index of property in thing.properties().
// Return -1 if the property does not exist.
int GetPropertyIndex(StringPiece name, const AnnotationData& annotation_data);

// Return the single int value for property 'name' of the annotation.
// Returns 0 if the property does not exist or does not contain a single int
// value.
int GetIntProperty(StringPiece name, const Annotation& annotation);

// Return the single float value for property 'name' of the annotation.
// Returns 0 if the property does not exist or does not contain a single int
// value.
int GetIntProperty(StringPiece name, const AnnotationData& annotation_data);

// Add a new property with a single int value to an Annotation instance.
// Return the index of the property.
int AddIntProperty(StringPiece name, const int value, Annotation* annotation);

// Add a new property with a single int value to a Thing instance.
// Return the index of the property.
int AddIntProperty(StringPiece name, const int value,
                   AnnotationData* annotation_data);

// Add a new property with repeated int values to an Annotation instance.
// Return the index of the property.
int AddRepeatedIntProperty(StringPiece name, const int* start, int size,
                           Annotation* annotation);

// Add a new property with repeated int values to a Thing instance.
// Return the index of the property.
int AddRepeatedIntProperty(StringPiece name, const int* start, int size,
                           AnnotationData* annotation_data);

// Add a new property with Thing value.
// Return the index of the property.
int AddAnnotationDataProperty(const std::string& key,
                              const AnnotationData& value,
                              Annotation* annotation);

// Add a new property with Thing value.
// Return the index of the property.
int AddAnnotationDataProperty(const std::string& key,
                              const AnnotationData& value,
                              AnnotationData* annotation_data);

}  // namespace libtextclassifier3
#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_GRAMMAR_DATES_ANNOTATIONS_ANNOTATION_UTIL_H_
