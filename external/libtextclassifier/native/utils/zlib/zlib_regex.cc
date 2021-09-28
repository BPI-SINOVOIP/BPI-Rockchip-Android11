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

#include "utils/zlib/zlib_regex.h"

#include <memory>

#include "utils/base/logging.h"
#include "utils/flatbuffers.h"
#include "utils/utf8/unicodetext.h"

namespace libtextclassifier3 {

std::unique_ptr<UniLib::RegexPattern> UncompressMakeRegexPattern(
    const UniLib& unilib, const flatbuffers::String* uncompressed_pattern,
    const CompressedBuffer* compressed_pattern, bool lazy_compile_regex,
    ZlibDecompressor* decompressor, std::string* result_pattern_text) {
  UnicodeText unicode_regex_pattern;
  std::string decompressed_pattern;
  if (compressed_pattern != nullptr &&
      compressed_pattern->buffer() != nullptr) {
    if (decompressor == nullptr ||
        !decompressor->MaybeDecompress(compressed_pattern,
                                       &decompressed_pattern)) {
      TC3_LOG(ERROR) << "Cannot decompress pattern.";
      return nullptr;
    }
    unicode_regex_pattern =
        UTF8ToUnicodeText(decompressed_pattern.data(),
                          decompressed_pattern.size(), /*do_copy=*/false);
  } else {
    if (uncompressed_pattern == nullptr) {
      TC3_LOG(ERROR) << "Cannot load uncompressed pattern.";
      return nullptr;
    }
    unicode_regex_pattern =
        UTF8ToUnicodeText(uncompressed_pattern->c_str(),
                          uncompressed_pattern->Length(), /*do_copy=*/false);
  }

  if (result_pattern_text != nullptr) {
    *result_pattern_text = unicode_regex_pattern.ToUTF8String();
  }

  std::unique_ptr<UniLib::RegexPattern> regex_pattern;
  if (lazy_compile_regex) {
    regex_pattern = unilib.CreateLazyRegexPattern(unicode_regex_pattern);
  } else {
    regex_pattern = unilib.CreateRegexPattern(unicode_regex_pattern);
  }

  if (!regex_pattern) {
    TC3_LOG(ERROR) << "Could not create pattern: "
                   << unicode_regex_pattern.ToUTF8String();
  }
  return regex_pattern;
}

}  // namespace libtextclassifier3
