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

#include "utils/tflite/text_encoder.h"

#include <memory>
#include <vector>

#include "utils/base/logging.h"
#include "utils/container/double-array-trie.h"
#include "utils/container/sorted-strings-table.h"
#include "utils/sentencepiece/encoder.h"
#include "utils/sentencepiece/normalizer.h"
#include "utils/strings/stringpiece.h"
#include "utils/tflite/encoder_common.h"
#include "utils/tflite/text_encoder_config_generated.h"
#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/flexbuffers.h"
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/string_util.h"

namespace libtextclassifier3 {
namespace {

struct TextEncoderOp {
  std::unique_ptr<SentencePieceNormalizer> normalizer;
  std::unique_ptr<Encoder> encoder;
  std::unique_ptr<StringSet> matcher;
};

// Input parameters for the op.
// The conversation message as a (1, conversation length) string tensor.
constexpr const int kInputTexts = 0;

// The number of messages, the conversation length, int scalar.
constexpr const int kInputNumInputs = 1;

// Maximum output length of the encoding, int scalar.
constexpr const int kInputMaxLength = 2;

// Additional attributes to align to the sentence pieces, e.g. user ids per
// message.
constexpr const int kInputAttr = 3;

// Output parameters for the op.
// The text sentence piece encodings as ids, (1, max output length) int tensor.
constexpr const int kOutputEncoded = 0;

// Relative position of each sentence piece in the input text,
// (1, max output length) int tensor.
constexpr const int kOutputPosition = 1;

// Output length after trimming to the maximum output length specified.
// int scalar.
constexpr const int kOutputLengths = 2;

// Padded and sentence piece aligned provided attributes, e.g. user id per
// sentence piece.
constexpr const int kOutputAttr = 3;

const char kTextEncoderConfigAttr[] = "text_encoder_config";

// Initializes text encoder object from serialized options:
//   The options are a flexbuffers attribute map that contain the op config
//   with the key `text_encoder_config` as `TextEncoderConfig`.
void* Initialize(TfLiteContext* context, const char* buffer, size_t length) {
  const flexbuffers::Map& attr_map =
      flexbuffers::GetRoot(reinterpret_cast<const uint8_t*>(buffer), length)
          .AsMap();
  const flexbuffers::Blob serialized_config =
      attr_map[kTextEncoderConfigAttr].AsBlob();
  const TextEncoderConfig* config =
      flatbuffers::GetRoot<TextEncoderConfig>(serialized_config.data());

  std::unique_ptr<TextEncoderOp> encoder_op(new TextEncoderOp());

  // Create normalizer from options.
  const TrieNode* charsmap_trie_nodes = reinterpret_cast<const TrieNode*>(
      config->normalization_charsmap()->Data());
  const int charsmap_trie_nodes_length =
      config->normalization_charsmap()->size() / sizeof(TrieNode);
  encoder_op->normalizer.reset(new SentencePieceNormalizer(
      DoubleArrayTrie(charsmap_trie_nodes, charsmap_trie_nodes_length),
      StringPiece(config->normalization_charsmap_values()->data(),
                  config->normalization_charsmap_values()->size()),
      config->add_dummy_prefix(), config->remove_extra_whitespaces(),
      config->escape_whitespaces()));

  const int num_pieces = config->pieces_scores()->Length();

  switch (config->matcher_type()) {
    case SentencePieceMatcherType_MAPPED_TRIE: {
      const TrieNode* pieces_trie_nodes =
          reinterpret_cast<const TrieNode*>(config->pieces()->Data());
      const int pieces_trie_nodes_length =
          config->pieces()->Length() / sizeof(TrieNode);
      encoder_op->matcher.reset(
          new DoubleArrayTrie(pieces_trie_nodes, pieces_trie_nodes_length));
      break;
    }
    case SentencePieceMatcherType_SORTED_STRING_TABLE: {
      encoder_op->matcher.reset(new SortedStringsTable(
          num_pieces, config->pieces_offsets()->data(),
          StringPiece(config->pieces()->data(), config->pieces()->Length())));
      break;
    }
    default: {
      TC3_LOG(ERROR) << "Unknown sentence piece matcher type.";
      return nullptr;
    }
  }
  encoder_op->encoder.reset(new Encoder(
      encoder_op->matcher.get(), num_pieces, config->pieces_scores()->data(),
      config->start_code(), config->end_code(), config->encoding_offset(),
      config->unknown_code(), config->unknown_score()));
  return encoder_op.release();
}

void Free(TfLiteContext* context, void* buffer) {
  delete reinterpret_cast<TextEncoderOp*>(buffer);
}

TfLiteStatus ResizeOutputTensors(TfLiteContext* context, TfLiteNode* node,
                                 int max_output_length) {
  TF_LITE_ENSURE_OK(
      context,
      ResizeOutputTensor(max_output_length,
                         &context->tensors[node->outputs->data[kOutputEncoded]],
                         context));

  TF_LITE_ENSURE_OK(
      context,
      ResizeOutputTensor(
          max_output_length,
          &context->tensors[node->outputs->data[kOutputPosition]], context));

  const int num_output_attrs = node->outputs->size - kOutputAttr;
  for (int i = 0; i < num_output_attrs; ++i) {
    TF_LITE_ENSURE_OK(
        context,
        ResizeOutputTensor(
            max_output_length,
            &context->tensors[node->outputs->data[kOutputAttr + i]], context));
  }
  return kTfLiteOk;
}

TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  // Check that the batch dimension is kBatchSize.
  const TfLiteTensor& input_text =
      context->tensors[node->inputs->data[kInputTexts]];
  TF_LITE_ENSURE_EQ(context, input_text.dims->size, kEncoderInputRank);
  TF_LITE_ENSURE_EQ(context, input_text.dims->data[0], kEncoderBatchSize);

  TfLiteTensor& output_lengths =
      context->tensors[node->outputs->data[kOutputLengths]];
  TfLiteTensor& output_encoded =
      context->tensors[node->outputs->data[kOutputEncoded]];
  TfLiteTensor& output_positions =
      context->tensors[node->outputs->data[kOutputPosition]];

  TF_LITE_ENSURE_OK(context,
                    context->ResizeTensor(context, &output_lengths,
                                          CreateIntArray({kEncoderBatchSize})));

  // Check that there are enough outputs for attributes.
  const int num_output_attrs = node->outputs->size - kOutputAttr;
  TF_LITE_ENSURE_EQ(context, node->inputs->size - kInputAttr, num_output_attrs);

  // Copy attribute types from input to output tensors.
  for (int i = 0; i < num_output_attrs; ++i) {
    TfLiteTensor& input = context->tensors[node->inputs->data[kInputAttr + i]];
    TfLiteTensor& output =
        context->tensors[node->outputs->data[kOutputAttr + i]];
    output.type = input.type;
  }

  const TfLiteTensor& output_length =
      context->tensors[node->inputs->data[kInputMaxLength]];

  if (tflite::IsConstantTensor(&output_length)) {
    return ResizeOutputTensors(context, node, output_length.data.i64[0]);
  } else {
    tflite::SetTensorToDynamic(&output_encoded);
    tflite::SetTensorToDynamic(&output_positions);
    for (int i = 0; i < num_output_attrs; ++i) {
      TfLiteTensor& output_attr =
          context->tensors[node->outputs->data[kOutputAttr + i]];
      tflite::SetTensorToDynamic(&output_attr);
    }
  }

  return kTfLiteOk;
}

TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node) {
  if (node->user_data == nullptr) {
    return kTfLiteError;
  }
  const TextEncoderOp* encoder_op =
      reinterpret_cast<TextEncoderOp*>(node->user_data);
  const TfLiteTensor& input_text =
      context->tensors[node->inputs->data[kInputTexts]];
  const int num_strings = tflite::GetStringCount(&input_text);
  // Check that the number of strings matches the length parameter.
  const int num_strings_param =
      context->tensors[node->inputs->data[kInputNumInputs]].data.i32[0];
  TF_LITE_ENSURE_EQ(context, num_strings, num_strings_param);

  TfLiteTensor& output_encoded =
      context->tensors[node->outputs->data[kOutputEncoded]];
  if (tflite::IsDynamicTensor(&output_encoded)) {
    const TfLiteTensor& output_length =
        context->tensors[node->inputs->data[kInputMaxLength]];
    TF_LITE_ENSURE_OK(
        context, ResizeOutputTensors(context, node, output_length.data.i64[0]));
  }
  TfLiteTensor& output_positions =
      context->tensors[node->outputs->data[kOutputPosition]];

  std::vector<int> encoded_total;
  std::vector<int> encoded_offsets;
  std::vector<int> encoded_positions;
  encoded_offsets.reserve(num_strings);
  const int max_output_length = output_encoded.dims->data[1];
  const int max_encoded_position = max_output_length;

  for (int i = 0; i < num_strings; ++i) {
    const auto& strref = tflite::GetString(&input_text, i);
    std::string normalized;
    TF_LITE_ENSURE(context,
                   encoder_op->normalizer->Normalize(
                       StringPiece(strref.str, strref.len), &normalized));
    std::vector<int> encoded;
    TF_LITE_ENSURE(context, encoder_op->encoder->Encode(normalized, &encoded));
    encoded_total.insert(encoded_total.end(), encoded.begin(), encoded.end());
    encoded_offsets.push_back(encoded_total.size());
    for (int i = 0; i < encoded.size(); i++) {
      encoded_positions.push_back(std::min(i, max_encoded_position - 1));
    }
  }

  const int num_skip = CopyDataToTensorAndPadOrTruncate(
      max_output_length, encoded_total,
      /*padding_value=*/encoded_total.back(), &output_encoded);
  TfLiteTensor& output_lengths =
      context->tensors[node->outputs->data[kOutputLengths]];
  output_lengths.data.i32[0] = encoded_total.size() - num_skip;
  CopyDataToTensorAndPadOrTruncate(max_output_length, encoded_positions,
                                   /*padding_value=*/max_encoded_position,
                                   &output_positions);

  // Process attributes, all checks of sizes and types are done in Prepare.
  const int num_output_attrs = node->outputs->size - kOutputAttr;
  TF_LITE_ENSURE_EQ(context, node->inputs->size - kInputAttr, num_output_attrs);
  for (int i = 0; i < num_output_attrs; ++i) {
    TfLiteStatus attr_status = CopyValuesToTensorAndPadOrTruncate(
        context->tensors[node->inputs->data[kInputAttr + i]], encoded_offsets,
        num_skip, context,
        &context->tensors[node->outputs->data[kOutputAttr + i]]);
    if (attr_status != kTfLiteOk) {
      return attr_status;
    }
  }

  return kTfLiteOk;
}

}  // namespace
}  // namespace libtextclassifier3

namespace tflite {
namespace ops {
namespace custom {

TfLiteRegistration* Register_TEXT_ENCODER() {
  static TfLiteRegistration registration = {
      libtextclassifier3::Initialize, libtextclassifier3::Free,
      libtextclassifier3::Prepare, libtextclassifier3::Eval};
  return &registration;
}

}  // namespace custom
}  // namespace ops
}  // namespace tflite
