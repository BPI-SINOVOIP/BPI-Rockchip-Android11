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

#include "annotator/model-executor.h"

#include "annotator/quantization.h"
#include "utils/base/logging.h"

namespace libtextclassifier3 {

TensorView<float> ModelExecutor::ComputeLogits(
    const TensorView<float>& features, tflite::Interpreter* interpreter) const {
  if (!interpreter) {
    return TensorView<float>::Invalid();
  }
  interpreter->ResizeInputTensor(kInputIndexFeatures, features.shape());
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    TC3_VLOG(1) << "Allocation failed.";
    return TensorView<float>::Invalid();
  }

  SetInput<float>(kInputIndexFeatures, features, interpreter);

  if (interpreter->Invoke() != kTfLiteOk) {
    TC3_VLOG(1) << "Interpreter failed.";
    return TensorView<float>::Invalid();
  }

  return OutputView<float>(kOutputIndexLogits, interpreter);
}

std::unique_ptr<TFLiteEmbeddingExecutor> TFLiteEmbeddingExecutor::FromBuffer(
    const flatbuffers::Vector<uint8_t>* model_spec_buffer, int embedding_size,
    int quantization_bits,
    const Model_::EmbeddingPruningMask* embedding_pruning_mask) {
  std::unique_ptr<TfLiteModelExecutor> executor =
      TfLiteModelExecutor::FromBuffer(model_spec_buffer);
  if (!executor) {
    TC3_LOG(ERROR) << "Could not load TFLite model for embeddings.";
    return nullptr;
  }

  std::unique_ptr<tflite::Interpreter> interpreter =
      executor->CreateInterpreter();
  if (!interpreter) {
    TC3_LOG(ERROR) << "Could not build TFLite interpreter for embeddings.";
    return nullptr;
  }

  if (interpreter->tensors_size() != 2) {
    return nullptr;
  }
  const TfLiteTensor* embeddings = interpreter->tensor(0);
  if (embeddings->dims->size != 2) {
    return nullptr;
  }
  int num_buckets = embeddings->dims->data[0];
  const TfLiteTensor* scales = interpreter->tensor(1);
  if (scales->dims->size != 2 || scales->dims->data[0] != num_buckets ||
      scales->dims->data[1] != 1) {
    return nullptr;
  }
  int bytes_per_embedding = embeddings->dims->data[1];
  if (!CheckQuantizationParams(bytes_per_embedding, quantization_bits,
                               embedding_size)) {
    TC3_LOG(ERROR) << "Mismatch in quantization parameters.";
    return nullptr;
  }

  return std::unique_ptr<TFLiteEmbeddingExecutor>(new TFLiteEmbeddingExecutor(
      std::move(executor), quantization_bits, num_buckets, bytes_per_embedding,
      embedding_size, scales, embeddings, std::move(interpreter),
      embedding_pruning_mask));
}

TFLiteEmbeddingExecutor::TFLiteEmbeddingExecutor(
    std::unique_ptr<TfLiteModelExecutor> executor, int quantization_bits,
    int num_buckets, int bytes_per_embedding, int output_embedding_size,
    const TfLiteTensor* scales, const TfLiteTensor* embeddings,
    std::unique_ptr<tflite::Interpreter> interpreter,
    const Model_::EmbeddingPruningMask* embedding_pruning_mask)
    : executor_(std::move(executor)),
      quantization_bits_(quantization_bits),
      num_buckets_(num_buckets),
      bytes_per_embedding_(bytes_per_embedding),
      output_embedding_size_(output_embedding_size),
      scales_(scales),
      embeddings_(embeddings),
      interpreter_(std::move(interpreter)) {
  if ((embedding_pruning_mask != nullptr) &&
      (embedding_pruning_mask->enabled())) {
    for (int i = 0; i < embedding_pruning_mask->pruning_mask()->size(); i++) {
      pruning_mask_.push_back((*(embedding_pruning_mask->pruning_mask()))[i]);
    }
    ComputePrefixCounts();
    full_num_buckets_ = embedding_pruning_mask->full_num_buckets();
    pruned_row_bucket_id_ = embedding_pruning_mask->pruned_row_bucket_id();
  } else {
    full_num_buckets_ = num_buckets;
  }
}

void TFLiteEmbeddingExecutor::ComputePrefixCounts() {
  // Pre-compute the prefix sums.
  // For each i in {0, 1,...,pruning_mask_.size()-1}, we compute number of 1s
  // in binary representations of the uint64 values in pruning_mask_ before
  // index i. We set pruned_row_bucket_id_ to the total number of 1s
  // in binary representations of all values in pruning_mask_.
  int count = 0;
  for (const uint64 mask : pruning_mask_) {
    prefix_counts_.push_back(count);
    count += __builtin_popcountll(mask);
  }
}

int TFLiteEmbeddingExecutor::PruneBucketId(int bucket_id) const {
  // Implements auxiliary data structure for computing the pruned index of a
  // given bucket_id.
  // If bucket_id is present in pruning_mask_, we compute floor(bucket_id/64),
  // look it up in the auxiliary array prefix_counts_, and add to it the number
  // of 1s before before bucket_id % 64 in the 64-bit sequence
  // pruning_mask_[floor(bucket_id/64)].
  // If bucket_id is absent from pruning_mask_, we return pruned_row_bucket_id_.
  const int bucket_id_major = bucket_id >> 6;
  const int bucket_id_minor = bucket_id & 63;
  uint64_t one = 1;
  if (!(pruning_mask_[bucket_id_major] & (one << bucket_id_minor)))
    return pruned_row_bucket_id_;
  const uint64 zero = 0;
  uint64 minor_mask;
  if (bucket_id_minor == 0)
    minor_mask = zero;
  else
    minor_mask = ((~zero) >> (64 - bucket_id_minor));
  return prefix_counts_[bucket_id_major] +
         __builtin_popcountll(pruning_mask_[bucket_id_major] & minor_mask);
}

bool TFLiteEmbeddingExecutor::AddEmbedding(
    const TensorView<int>& sparse_features, float* dest, int dest_size) const {
  if (dest_size != output_embedding_size_) {
    TC3_LOG(ERROR) << "Mismatching dest_size and output_embedding_size: "
                   << dest_size << " " << output_embedding_size_;
    return false;
  }
  const int num_sparse_features = sparse_features.size();
  for (int i = 0; i < num_sparse_features; ++i) {
    const int bucket_id = sparse_features.data()[i];
    int full_num_buckets;
    if (!pruning_mask_.empty()) {
      full_num_buckets = full_num_buckets_;
    } else {
      full_num_buckets = num_buckets_;
    }
    if (bucket_id >= full_num_buckets) {
      return false;
    }
    int final_bucket_id;
    if (!pruning_mask_.empty()) {
      final_bucket_id = PruneBucketId(bucket_id);
    } else {
      final_bucket_id = bucket_id;
    }
    if (!DequantizeAdd(scales_->data.f, embeddings_->data.uint8,
                       bytes_per_embedding_, num_sparse_features,
                       quantization_bits_, final_bucket_id, dest, dest_size)) {
      return false;
    }
  }
  return true;
}

}  // namespace libtextclassifier3
