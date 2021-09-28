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

#include "utils/tflite/dist_diversification.h"

#include <algorithm>
#include "tensorflow/lite/context.h"
#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/model.h"

namespace libtextclassifier3 {
namespace {

// Returns a vector of row indices in a distance matrix.
// Indices are increasing and the distance of every selected index to others
// is larger than `min_distance`.
template <typename DistanceMatrixType>
std::vector<int> DiversifyByDistance(const DistanceMatrixType& distance_matrix,
                                     const int matrix_size,
                                     const float min_distance,
                                     const int max_num_results) {
  std::vector<int> result{0};
  result.reserve(max_num_results);
  int index = 1;
  while (result.size() < max_num_results && index < matrix_size) {
    for (; index < matrix_size; ++index) {
      bool too_close = false;
      for (const int selected_index : result) {
        if (distance_matrix(index, selected_index) < min_distance) {
          too_close = true;
          break;
        }
      }
      if (!too_close) {
        result.push_back(index);
        ++index;
        break;
      }
    }
  }
  return result;
}

// Input parameters for the op.
enum DistDiversificationInputs {
  DIST_DIVERSIFICATION_INPUT_DISTANCE_MATRIX = 0,
  DIST_DIVERSIFICATION_INPUT_MIN_DISTANCE = 1,
  DIST_DIVERSIFICATION_INPUT_NUM_RESULTS = 2
};

// Output parameters for the op.
enum DistDiversificationOutputs {
  DIST_DIVERSIFICATION_OUTPUT_INDICES = 0,
  DIST_DIVERSIFICATION_OUTPUT_LENGTH = 1,
};

TfLiteIntArray* CreateSizeArray(const std::initializer_list<int>& sizes) {
  TfLiteIntArray* array_size = TfLiteIntArrayCreate(sizes.size());
  int index = 0;
  for (const int size : sizes) {
    array_size->data[index++] = size;
  }
  return array_size;
}

TfLiteStatus AllocateOutputIndexes(TfLiteContext* context, TfLiteNode* node) {
  const TfLiteTensor& num_results =
      context
          ->tensors[node->inputs->data[DIST_DIVERSIFICATION_INPUT_NUM_RESULTS]];
  TfLiteTensor& output_indices =
      context
          ->tensors[node->outputs->data[DIST_DIVERSIFICATION_OUTPUT_INDICES]];
  return context->ResizeTensor(context, &output_indices,
                               CreateSizeArray({num_results.data.i32[0]}));
}

TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  const TfLiteTensor& num_results =
      context
          ->tensors[node->inputs->data[DIST_DIVERSIFICATION_INPUT_NUM_RESULTS]];
  if (tflite::IsConstantTensor(&num_results)) {
    TF_LITE_ENSURE_OK(context, AllocateOutputIndexes(context, node));
  } else {
    TfLiteTensor& output_indices =
        context
            ->tensors[node->outputs->data[DIST_DIVERSIFICATION_OUTPUT_INDICES]];
    tflite::SetTensorToDynamic(&output_indices);
  }
  TfLiteTensor& output_length =
      context->tensors[node->outputs->data[DIST_DIVERSIFICATION_OUTPUT_LENGTH]];
  TF_LITE_ENSURE_OK(context, context->ResizeTensor(context, &output_length,
                                                   CreateSizeArray({1})));
  return kTfLiteOk;
}

TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node) {
  TfLiteTensor& output_indices =
      context
          ->tensors[node->outputs->data[DIST_DIVERSIFICATION_OUTPUT_INDICES]];
  if (tflite::IsDynamicTensor(&output_indices)) {
    TF_LITE_ENSURE_OK(context, AllocateOutputIndexes(context, node));
  }
  const TfLiteTensor& distance_matrix =
      context->tensors[node->inputs
                           ->data[DIST_DIVERSIFICATION_INPUT_DISTANCE_MATRIX]];
  const int distance_matrix_dim = distance_matrix.dims->data[0];
  const float min_distance =
      context
          ->tensors[node->inputs->data[DIST_DIVERSIFICATION_INPUT_MIN_DISTANCE]]
          .data.f[0];
  const int num_results =
      context
          ->tensors[node->inputs->data[DIST_DIVERSIFICATION_INPUT_NUM_RESULTS]]
          .data.i32[0];
  const auto indices = DiversifyByDistance(
      [&](int row, int col) {
        return distance_matrix.data.f[row * distance_matrix_dim + col];
      },
      distance_matrix_dim, min_distance, num_results);
  std::copy(indices.begin(), indices.end(), output_indices.data.i32);
  std::fill_n(output_indices.data.i32 + indices.size(),
              num_results - indices.size(), -1);
  TfLiteTensor& output_length =
      context->tensors[node->outputs->data[DIST_DIVERSIFICATION_OUTPUT_LENGTH]];
  *output_length.data.i32 = indices.size();
  return kTfLiteOk;
}

}  // namespace
}  // namespace libtextclassifier3

namespace tflite {
namespace ops {
namespace custom {
TfLiteRegistration* Register_DISTANCE_DIVERSIFICATION() {
  static TfLiteRegistration r = {nullptr, nullptr, libtextclassifier3::Prepare,
                                 libtextclassifier3::Eval};
  return &r;
}
}  // namespace custom
}  // namespace ops
}  // namespace tflite
