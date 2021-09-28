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

#ifndef LIBTEXTCLASSIFIER_UTILS_ZLIB_ZLIB_REGEX_H_
#define LIBTEXTCLASSIFIER_UTILS_ZLIB_ZLIB_REGEX_H_

#include <memory>

#include "utils/utf8/unilib.h"
#include "utils/zlib/buffer_generated.h"
#include "utils/zlib/zlib.h"

namespace libtextclassifier3 {

// Create and compile a regex pattern from optionally compressed pattern.
std::unique_ptr<UniLib::RegexPattern> UncompressMakeRegexPattern(
    const UniLib& unilib, const flatbuffers::String* uncompressed_pattern,
    const CompressedBuffer* compressed_pattern, bool lazy_compile_regex,
    ZlibDecompressor* decompressor, std::string* result_pattern_text = nullptr);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_ZLIB_ZLIB_REGEX_H_
