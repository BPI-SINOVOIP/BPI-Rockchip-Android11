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

#include "utils/tflite/encoder_common.h"

#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/string_util.h"

namespace libtextclassifier3 {

TfLiteIntArray* CreateIntArray(const std::initializer_list<int>& values) {
  TfLiteIntArray* array_size = TfLiteIntArrayCreate(values.size());
  int index = 0;
  for (const int size : values) {
    array_size->data[index++] = size;
  }
  return array_size;
}

TfLiteStatus CopyValuesToTensorAndPadOrTruncate(
    const TfLiteTensor& in, const std::vector<int>& encoding_end_offsets,
    int start_offset, TfLiteContext* context, TfLiteTensor* out) {
  TF_LITE_ENSURE_EQ(context, in.dims->size, kEncoderInputRank);
  TF_LITE_ENSURE_EQ(context, in.dims->data[0], kEncoderBatchSize);
  const int output_size = out->dims->data[1];
  int output_offset = 0;
  for (int value_index = 0;
       value_index < encoding_end_offsets.size() && output_offset < output_size;
       ++value_index) {
    // Calculate how many elements need to be set with this value.
    // The low bound depends on the offset from the beginning. If this is 0, it
    // means that this value it truncated.
    // The upper bound depends on how many elements are in the output tensor.
    const int from_this_element =
        std::min(std::max(0, encoding_end_offsets[value_index] - start_offset -
                                 output_offset),
                 output_size - output_offset);
    if (from_this_element == 0) {
      continue;
    }

    switch (in.type) {
      case kTfLiteInt32: {
        std::fill(out->data.i32 + output_offset,
                  out->data.i32 + output_offset + from_this_element,
                  in.data.i32[value_index]);
      } break;
      case kTfLiteFloat32: {
        std::fill(out->data.f + output_offset,
                  out->data.f + output_offset + from_this_element,
                  in.data.f[value_index]);
      } break;
      default:
        context->ReportError(
            (context), __FILE__ " Not supported attribute type %d", in.type);
        return kTfLiteError;
    }
    output_offset += from_this_element;
  }
  // Do final padding.
  switch (in.type) {
    case kTfLiteInt32: {
      const int32_t value =
          (output_offset > 0) ? out->data.i32[output_offset - 1] : 0;
      std::fill(out->data.i32 + output_offset, out->data.i32 + output_size,
                value);
    } break;
    case kTfLiteFloat32: {
      const float value =
          (output_offset > 0) ? out->data.f[output_offset - 1] : 0;
      std::fill(out->data.f + output_offset, out->data.f + output_size, value);
    } break;
    default:
      break;
  }
  return kTfLiteOk;
}

TfLiteStatus ResizeOutputTensor(const int max_output_length,
                                TfLiteTensor* tensor, TfLiteContext* context) {
  TF_LITE_ENSURE_OK(
      context, context->ResizeTensor(
                   context, tensor,
                   CreateIntArray({kEncoderBatchSize, max_output_length})));
  return kTfLiteOk;
}

int CopyDataToTensorAndPadOrTruncate(const int32_t max_output_length,
                                     const std::vector<int32_t>& data,
                                     const int32_t padding_value,
                                     TfLiteTensor* output_tensor) {
  const int num_skip =
      std::max(0, static_cast<int>(data.size()) - max_output_length);
  int output_offset = 0;
  int32_t* output_buffer = output_tensor->data.i32;
  for (int i = num_skip; i < data.size(); ++i, ++output_offset) {
    output_buffer[output_offset] = data[i];
  }

  // Do padding.
  for (; output_offset < max_output_length; ++output_offset) {
    output_buffer[output_offset] = padding_value;
  }

  // Return number of skipped entries from the beginning.
  return num_skip;
}

}  // namespace libtextclassifier3
