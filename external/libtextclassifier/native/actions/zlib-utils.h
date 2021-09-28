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

#ifndef LIBTEXTCLASSIFIER_ACTIONS_ZLIB_UTILS_H_
#define LIBTEXTCLASSIFIER_ACTIONS_ZLIB_UTILS_H_

#include "actions/actions_model_generated.h"
#include "utils/zlib/buffer_generated.h"
#include "utils/zlib/zlib.h"

namespace libtextclassifier3 {

// Compresses regex rules in the model in place.
bool CompressActionsModel(ActionsModelT* model);

// Decompresses regex rules in the model in place.
bool DecompressActionsModel(ActionsModelT* model);

// Compresses regex rules in the model.
std::string CompressSerializedActionsModel(const std::string& model);

bool GetUncompressedString(const flatbuffers::String* uncompressed_buffer,
                           const CompressedBuffer* compressed_buffer,
                           ZlibDecompressor* decompressor, std::string* out);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ACTIONS_ZLIB_UTILS_H_
