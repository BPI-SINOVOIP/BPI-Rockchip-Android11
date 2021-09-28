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

#ifndef LIBTEXTCLASSIFIER_UTILS_TFLITE_DIST_DIVERSIFICATION_H_
#define LIBTEXTCLASSIFIER_UTILS_TFLITE_DIST_DIVERSIFICATION_H_

#include "tensorflow/lite/context.h"

namespace tflite {
namespace ops {
namespace custom {

TfLiteRegistration* Register_DISTANCE_DIVERSIFICATION();

}  // namespace custom
}  // namespace ops
}  // namespace tflite

#endif  // LIBTEXTCLASSIFIER_UTILS_TFLITE_DIST_DIVERSIFICATION_H_
