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

#include "utils/tflite/token_encoder.h"

#include <vector>

#include "utils/tflite/encoder_common.h"
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/model.h"

namespace libtextclassifier3 {
namespace {

// Input parameters for the op.
// The number of tokens per message as (1, conversation length) int tensor.
constexpr const int kInputNumTokens = 0;

// The number of messages, the conversation length, int scalar.
constexpr const int kInputNumInputs = 1;

// Maximum output length of the encoding, int scalar.
constexpr const int kInputMaxLength = 2;

// Additional attributes to align to the sentence pieces, e.g. user ids per
// message.
constexpr const int kInputAttr = 3;

// Output parameters for the op.
// Relative position of each token in the input text,
// (1, max output length) int tensor.
constexpr const int kOutputPosition = 0;

// Output length after trimming to the maximum output length specified.
// int scalar.
constexpr const int kOutputLengths = 1;

// Padded and sentence piece aligned provided attributes, e.g. user id per
// sentence piece.
constexpr const int kOutputAttr = 2;

TfLiteStatus ResizeOutputTensors(TfLiteContext* context, TfLiteNode* node,
                                 int max_output_length) {
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
  const TfLiteTensor& num_tokens =
      context->tensors[node->inputs->data[kInputNumTokens]];
  TF_LITE_ENSURE_EQ(context, num_tokens.dims->size, kEncoderInputRank);
  TF_LITE_ENSURE_EQ(context, num_tokens.dims->data[0], kEncoderBatchSize);

  TfLiteTensor& output_lengths =
      context->tensors[node->outputs->data[kOutputLengths]];
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
  const TfLiteTensor& num_tokens =
      context->tensors[node->inputs->data[kInputNumTokens]];
  const int num_inputs =
      context->tensors[node->inputs->data[kInputNumInputs]].data.i32[0];

  const TfLiteTensor& output_length =
      context->tensors[node->inputs->data[kInputMaxLength]];
  TfLiteTensor& output_positions =
      context->tensors[node->outputs->data[kOutputPosition]];
  if (!tflite::IsConstantTensor(&output_length)) {
    TF_LITE_ENSURE_OK(
        context, ResizeOutputTensors(context, node, output_length.data.i64[0]));
  }

  std::vector<int> encoded_offsets;
  std::vector<int> encoded_positions;
  encoded_offsets.reserve(num_inputs);
  const int max_output_length = output_positions.dims->data[1];
  const int max_encoded_position = max_output_length;
  int total_tokens = 0;

  for (int i = 0; i < num_inputs; ++i) {
    const int num_message_tokens =
        num_tokens.data.i32[i] + 2; /* num_tokens + start and end token. */
    total_tokens += num_message_tokens;
    encoded_offsets.push_back(total_tokens);
    for (int k = 0; k < num_message_tokens; k++) {
      encoded_positions.push_back(std::min(k, max_encoded_position - 1));
    }
  }

  const int num_skip = CopyDataToTensorAndPadOrTruncate(
      max_output_length, encoded_positions,
      /*padding_value=*/max_encoded_position, &output_positions);
  TfLiteTensor& output_lengths =
      context->tensors[node->outputs->data[kOutputLengths]];
  output_lengths.data.i32[0] = encoded_positions.size() - num_skip;

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

TfLiteRegistration* Register_TOKEN_ENCODER() {
  static TfLiteRegistration registration = {/*init=*/nullptr, /*free=*/nullptr,
                                            libtextclassifier3::Prepare,
                                            libtextclassifier3::Eval};
  return &registration;
}

}  // namespace custom
}  // namespace ops
}  // namespace tflite
