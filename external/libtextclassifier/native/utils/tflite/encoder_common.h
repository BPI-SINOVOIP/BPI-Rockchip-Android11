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

// Shared methods for the text and token encoders.

#ifndef LIBTEXTCLASSIFIER_UTILS_TFLITE_ENCODER_COMMON_H_
#define LIBTEXTCLASSIFIER_UTILS_TFLITE_ENCODER_COMMON_H_

#include <memory>
#include <vector>

#include "tensorflow/lite/model.h"

namespace libtextclassifier3 {

// Input rank for the encoder ops is 2, because the first dimension is
// always considered to be for batching, and during inference is always set to
// 1, and the second dimension indexes the input values (texts or token
// lengths).
constexpr const int kEncoderInputRank = 2;
constexpr const int kEncoderBatchSize = 1;

// Creates a TensorFlow Lite array from an initializer list.
TfLiteIntArray* CreateIntArray(const std::initializer_list<int>& values);

// Copies values associated with the input to the output.
// Typically we have attribute values associated with each item in the input,
// e.g. user id per message in the conversation.
// This aligns and replicates the attribute values with the encoded input, e.g.
// replicates the same user id per token or sentence piece of the input.
// As the input for the whole conversation is concatenated and (potentially)
// trimmed, `encoding_end_offset` indicates where each item ends and
// `start_offset` indicates how many elements at the beginning were dropped.
TfLiteStatus CopyValuesToTensorAndPadOrTruncate(
    const TfLiteTensor& in, const std::vector<int>& encoding_end_offsets,
    int start_offset, TfLiteContext* context, TfLiteTensor* out);

// Resizes an output tensor to shape {kBatchSize, max_output_length}.
TfLiteStatus ResizeOutputTensor(const int max_output_length,
                                TfLiteTensor* tensor, TfLiteContext* context);

// Copy a slice of data to output.
// If the size of the data is smaller than `max_output_length` then the output
// is padded with `padding_value`.
// If the size of the data is larger than `max_output_length` then entries at
// the beginning a dropped to fit into the limit.
int CopyDataToTensorAndPadOrTruncate(const int32_t max_output_length,
                                     const std::vector<int32_t>& data,
                                     const int32_t padding_value,
                                     TfLiteTensor* output_tensor);

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_TFLITE_ENCODER_COMMON_H_
