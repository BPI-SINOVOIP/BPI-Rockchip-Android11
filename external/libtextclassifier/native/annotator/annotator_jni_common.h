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

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_ANNOTATOR_JNI_COMMON_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_ANNOTATOR_JNI_COMMON_H_

#include <jni.h>

#include "annotator/annotator.h"
#include "annotator/types.h"
#include "utils/base/statusor.h"

#ifndef TC3_ANNOTATOR_CLASS_NAME
#define TC3_ANNOTATOR_CLASS_NAME AnnotatorModel
#endif

#define TC3_ANNOTATOR_CLASS_NAME_STR TC3_ADD_QUOTES(TC3_ANNOTATOR_CLASS_NAME)

namespace libtextclassifier3 {

StatusOr<SelectionOptions> FromJavaSelectionOptions(JNIEnv* env,
                                                    jobject joptions);

StatusOr<ClassificationOptions> FromJavaClassificationOptions(JNIEnv* env,
                                                              jobject joptions);

StatusOr<AnnotationOptions> FromJavaAnnotationOptions(JNIEnv* env,
                                                      jobject joptions);

StatusOr<InputFragment> FromJavaInputFragment(JNIEnv* env, jobject jfragment);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_ANNOTATOR_JNI_COMMON_H_
