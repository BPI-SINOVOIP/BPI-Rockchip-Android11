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

// Functions to compress and decompress low entropy entries in the model.

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_ZLIB_UTILS_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_ZLIB_UTILS_H_

#include "annotator/model_generated.h"

namespace libtextclassifier3 {

// Compresses regex and datetime rules in the model in place.
bool CompressModel(ModelT* model);

// Decompresses regex and datetime rules in the model in place.
bool DecompressModel(ModelT* model);

// Compresses regex and datetime rules in the model.
std::string CompressSerializedModel(const std::string& model);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_ZLIB_UTILS_H_
