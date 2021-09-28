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

#include "annotator/zlib-utils.h"

#include <memory>

#include "utils/base/logging.h"
#include "utils/intents/zlib-utils.h"
#include "utils/resources.h"
#include "utils/zlib/zlib.h"

namespace libtextclassifier3 {

// Compress rule fields in the model.
bool CompressModel(ModelT* model) {
  std::unique_ptr<ZlibCompressor> zlib_compressor = ZlibCompressor::Instance();
  if (!zlib_compressor) {
    TC3_LOG(ERROR) << "Cannot compress model.";
    return false;
  }

  // Compress regex rules.
  if (model->regex_model != nullptr) {
    for (int i = 0; i < model->regex_model->patterns.size(); i++) {
      RegexModel_::PatternT* pattern = model->regex_model->patterns[i].get();
      pattern->compressed_pattern.reset(new CompressedBufferT);
      zlib_compressor->Compress(pattern->pattern,
                                pattern->compressed_pattern.get());
      pattern->pattern.clear();
    }
  }

  // Compress date-time rules.
  if (model->datetime_model != nullptr) {
    for (int i = 0; i < model->datetime_model->patterns.size(); i++) {
      DatetimeModelPatternT* pattern = model->datetime_model->patterns[i].get();
      for (int j = 0; j < pattern->regexes.size(); j++) {
        DatetimeModelPattern_::RegexT* regex = pattern->regexes[j].get();
        regex->compressed_pattern.reset(new CompressedBufferT);
        zlib_compressor->Compress(regex->pattern,
                                  regex->compressed_pattern.get());
        regex->pattern.clear();
      }
    }
    for (int i = 0; i < model->datetime_model->extractors.size(); i++) {
      DatetimeModelExtractorT* extractor =
          model->datetime_model->extractors[i].get();
      extractor->compressed_pattern.reset(new CompressedBufferT);
      zlib_compressor->Compress(extractor->pattern,
                                extractor->compressed_pattern.get());
      extractor->pattern.clear();
    }
  }

  // Compress resources.
  if (model->resources != nullptr) {
    CompressResources(model->resources.get());
  }

  // Compress intent generator.
  if (model->intent_options != nullptr) {
    CompressIntentModel(model->intent_options.get());
  }

  return true;
}

bool DecompressModel(ModelT* model) {
  std::unique_ptr<ZlibDecompressor> zlib_decompressor =
      ZlibDecompressor::Instance();
  if (!zlib_decompressor) {
    TC3_LOG(ERROR) << "Cannot initialize decompressor.";
    return false;
  }

  // Decompress regex rules.
  if (model->regex_model != nullptr) {
    for (int i = 0; i < model->regex_model->patterns.size(); i++) {
      RegexModel_::PatternT* pattern = model->regex_model->patterns[i].get();
      if (!zlib_decompressor->MaybeDecompress(pattern->compressed_pattern.get(),
                                              &pattern->pattern)) {
        TC3_LOG(ERROR) << "Cannot decompress pattern: " << i;
        return false;
      }
      pattern->compressed_pattern.reset(nullptr);
    }
  }

  // Decompress date-time rules.
  if (model->datetime_model != nullptr) {
    for (int i = 0; i < model->datetime_model->patterns.size(); i++) {
      DatetimeModelPatternT* pattern = model->datetime_model->patterns[i].get();
      for (int j = 0; j < pattern->regexes.size(); j++) {
        DatetimeModelPattern_::RegexT* regex = pattern->regexes[j].get();
        if (!zlib_decompressor->MaybeDecompress(regex->compressed_pattern.get(),
                                                &regex->pattern)) {
          TC3_LOG(ERROR) << "Cannot decompress pattern: " << i << " " << j;
          return false;
        }
        regex->compressed_pattern.reset(nullptr);
      }
    }
    for (int i = 0; i < model->datetime_model->extractors.size(); i++) {
      DatetimeModelExtractorT* extractor =
          model->datetime_model->extractors[i].get();
      if (!zlib_decompressor->MaybeDecompress(
              extractor->compressed_pattern.get(), &extractor->pattern)) {
        TC3_LOG(ERROR) << "Cannot decompress pattern: " << i;
        return false;
      }
      extractor->compressed_pattern.reset(nullptr);
    }
  }

  if (model->resources != nullptr) {
    DecompressResources(model->resources.get());
  }

  if (model->intent_options != nullptr) {
    DecompressIntentModel(model->intent_options.get());
  }

  return true;
}

std::string CompressSerializedModel(const std::string& model) {
  std::unique_ptr<ModelT> unpacked_model = UnPackModel(model.c_str());
  TC3_CHECK(unpacked_model != nullptr);
  TC3_CHECK(CompressModel(unpacked_model.get()));
  flatbuffers::FlatBufferBuilder builder;
  FinishModelBuffer(builder, Model::Pack(builder, unpacked_model.get()));
  return std::string(reinterpret_cast<const char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

}  // namespace libtextclassifier3
