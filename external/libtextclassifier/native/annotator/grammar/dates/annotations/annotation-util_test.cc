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

#include "annotator/grammar/dates/annotations/annotation.h"
#include "gtest/gtest.h"

namespace libtextclassifier3 {
namespace {

TEST(AnnotationUtilTest, VerifyIntFunctions) {
  Annotation annotation;

  int index_key1 = AddIntProperty("key1", 1, &annotation);
  int index_key2 = AddIntProperty("key2", 2, &annotation);

  static const int kValuesKey3[] = {3, 4, 5};
  int index_key3 =
      AddRepeatedIntProperty("key3", kValuesKey3, /*size=*/3, &annotation);

  EXPECT_EQ(2, GetIntProperty("key2", annotation));
  EXPECT_EQ(1, GetIntProperty("key1", annotation));

  EXPECT_EQ(index_key1, GetPropertyIndex("key1", annotation));
  EXPECT_EQ(index_key2, GetPropertyIndex("key2", annotation));
  EXPECT_EQ(index_key3, GetPropertyIndex("key3", annotation));
  EXPECT_EQ(-1, GetPropertyIndex("invalid_key", annotation));
}

TEST(AnnotationUtilTest, VerifyAnnotationDataFunctions) {
  Annotation annotation;

  AnnotationData true_annotation_data;
  Property true_property;
  true_property.bool_values.push_back(true);
  true_annotation_data.properties.push_back(true_property);
  int index_key1 =
      AddAnnotationDataProperty("key1", true_annotation_data, &annotation);

  AnnotationData false_annotation_data;
  Property false_property;
  false_property.bool_values.push_back(false);
  true_annotation_data.properties.push_back(false_property);
  int index_key2 =
      AddAnnotationDataProperty("key2", false_annotation_data, &annotation);

  EXPECT_EQ(index_key1, GetPropertyIndex("key1", annotation));
  EXPECT_EQ(index_key2, GetPropertyIndex("key2", annotation));
  EXPECT_EQ(-1, GetPropertyIndex("invalid_key", annotation));
}

}  // namespace
}  // namespace libtextclassifier3
