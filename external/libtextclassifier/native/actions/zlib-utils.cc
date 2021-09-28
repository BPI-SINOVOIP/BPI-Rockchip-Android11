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

#include "actions/zlib-utils.h"

#include <memory>

#include "utils/base/logging.h"
#include "utils/intents/zlib-utils.h"
#include "utils/resources.h"

namespace libtextclassifier3 {

// Compress rule fields in the model.
bool CompressActionsModel(ActionsModelT* model) {
  std::unique_ptr<ZlibCompressor> zlib_compressor = ZlibCompressor::Instance();
  if (!zlib_compressor) {
    TC3_LOG(ERROR) << "Cannot compress model.";
    return false;
  }

  // Compress regex rules.
  if (model->rules != nullptr) {
    for (int i = 0; i < model->rules->regex_rule.size(); i++) {
      RulesModel_::RegexRuleT* rule = model->rules->regex_rule[i].get();
      rule->compressed_pattern.reset(new CompressedBufferT);
      zlib_compressor->Compress(rule->pattern, rule->compressed_pattern.get());
      rule->pattern.clear();
    }
  }

  if (model->low_confidence_rules != nullptr) {
    for (int i = 0; i < model->low_confidence_rules->regex_rule.size(); i++) {
      RulesModel_::RegexRuleT* rule =
          model->low_confidence_rules->regex_rule[i].get();
      if (!rule->pattern.empty()) {
        rule->compressed_pattern.reset(new CompressedBufferT);
        zlib_compressor->Compress(rule->pattern,
                                  rule->compressed_pattern.get());
        rule->pattern.clear();
      }
      if (!rule->output_pattern.empty()) {
        rule->compressed_output_pattern.reset(new CompressedBufferT);
        zlib_compressor->Compress(rule->output_pattern,
                                  rule->compressed_output_pattern.get());
        rule->output_pattern.clear();
      }
    }
  }

  if (!model->lua_actions_script.empty()) {
    model->compressed_lua_actions_script.reset(new CompressedBufferT);
    zlib_compressor->Compress(model->lua_actions_script,
                              model->compressed_lua_actions_script.get());
  }

  if (model->ranking_options != nullptr &&
      !model->ranking_options->lua_ranking_script.empty()) {
    model->ranking_options->compressed_lua_ranking_script.reset(
        new CompressedBufferT);
    zlib_compressor->Compress(
        model->ranking_options->lua_ranking_script,
        model->ranking_options->compressed_lua_ranking_script.get());
  }

  // Compress resources.
  if (model->resources != nullptr) {
    CompressResources(model->resources.get());
  }

  // Compress intent generator.
  if (model->android_intent_options != nullptr) {
    CompressIntentModel(model->android_intent_options.get());
  }

  return true;
}

bool DecompressActionsModel(ActionsModelT* model) {
  std::unique_ptr<ZlibDecompressor> zlib_decompressor =
      ZlibDecompressor::Instance();
  if (!zlib_decompressor) {
    TC3_LOG(ERROR) << "Cannot initialize decompressor.";
    return false;
  }

  // Decompress regex rules.
  if (model->rules != nullptr) {
    for (int i = 0; i < model->rules->regex_rule.size(); i++) {
      RulesModel_::RegexRuleT* rule = model->rules->regex_rule[i].get();
      if (!zlib_decompressor->MaybeDecompress(rule->compressed_pattern.get(),
                                              &rule->pattern)) {
        TC3_LOG(ERROR) << "Cannot decompress pattern: " << i;
        return false;
      }
      rule->compressed_pattern.reset(nullptr);
    }
  }

  // Decompress low confidence rules.
  if (model->low_confidence_rules != nullptr) {
    for (int i = 0; i < model->low_confidence_rules->regex_rule.size(); i++) {
      RulesModel_::RegexRuleT* rule =
          model->low_confidence_rules->regex_rule[i].get();
      if (!zlib_decompressor->MaybeDecompress(rule->compressed_pattern.get(),
                                              &rule->pattern)) {
        TC3_LOG(ERROR) << "Cannot decompress pattern: " << i;
        return false;
      }
      if (!zlib_decompressor->MaybeDecompress(
              rule->compressed_output_pattern.get(), &rule->output_pattern)) {
        TC3_LOG(ERROR) << "Cannot decompress pattern: " << i;
        return false;
      }
      rule->compressed_pattern.reset(nullptr);
      rule->compressed_output_pattern.reset(nullptr);
    }
  }

  if (!zlib_decompressor->MaybeDecompress(
          model->compressed_lua_actions_script.get(),
          &model->lua_actions_script)) {
    TC3_LOG(ERROR) << "Cannot decompress actions script.";
    return false;
  }

  if (model->ranking_options != nullptr &&
      !zlib_decompressor->MaybeDecompress(
          model->ranking_options->compressed_lua_ranking_script.get(),
          &model->ranking_options->lua_ranking_script)) {
    TC3_LOG(ERROR) << "Cannot decompress actions script.";
    return false;
  }

  return true;
}

std::string CompressSerializedActionsModel(const std::string& model) {
  std::unique_ptr<ActionsModelT> unpacked_model =
      UnPackActionsModel(model.c_str());
  TC3_CHECK(unpacked_model != nullptr);
  TC3_CHECK(CompressActionsModel(unpacked_model.get()));
  flatbuffers::FlatBufferBuilder builder;
  FinishActionsModelBuffer(builder,
                           ActionsModel::Pack(builder, unpacked_model.get()));
  return std::string(reinterpret_cast<const char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

bool GetUncompressedString(const flatbuffers::String* uncompressed_buffer,
                           const CompressedBuffer* compressed_buffer,
                           ZlibDecompressor* decompressor, std::string* out) {
  if (uncompressed_buffer == nullptr && compressed_buffer == nullptr) {
    out->clear();
    return true;
  }

  return decompressor->MaybeDecompressOptionallyCompressedBuffer(
      uncompressed_buffer, compressed_buffer, out);
}

}  // namespace libtextclassifier3
