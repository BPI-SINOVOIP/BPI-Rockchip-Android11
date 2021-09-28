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

// Contains classes that can execute different models/parts of a model.

#ifndef LIBTEXTCLASSIFIER_ANNOTATOR_MODEL_EXECUTOR_H_
#define LIBTEXTCLASSIFIER_ANNOTATOR_MODEL_EXECUTOR_H_

#include <memory>

#include "annotator/types.h"
#include "utils/base/logging.h"
#include "utils/tensor-view.h"
#include "utils/tflite-model-executor.h"

namespace libtextclassifier3 {

// Executor for the text selection prediction and classification models.
class ModelExecutor : public TfLiteModelExecutor {
 public:
  static std::unique_ptr<ModelExecutor> FromModelSpec(
      const tflite::Model* model_spec) {
    auto model = TfLiteModelFromModelSpec(model_spec);
    if (!model) {
      return nullptr;
    }
    return std::unique_ptr<ModelExecutor>(new ModelExecutor(std::move(model)));
  }

  static std::unique_ptr<ModelExecutor> FromBuffer(
      const flatbuffers::Vector<uint8_t>* model_spec_buffer) {
    auto model = TfLiteModelFromBuffer(model_spec_buffer);
    if (!model) {
      return nullptr;
    }
    return std::unique_ptr<ModelExecutor>(new ModelExecutor(std::move(model)));
  }

  TensorView<float> ComputeLogits(const TensorView<float>& features,
                                  tflite::Interpreter* interpreter) const;

 protected:
  explicit ModelExecutor(std::unique_ptr<const tflite::FlatBufferModel> model)
      : TfLiteModelExecutor(std::move(model)) {}

  static constexpr int kInputIndexFeatures = 0;
  static constexpr int kOutputIndexLogits = 0;
};

// Executor for embedding sparse features into a dense vector.
class EmbeddingExecutor {
 public:
  virtual ~EmbeddingExecutor() {}

  // Embeds the sparse_features into a dense embedding and adds (+) it
  // element-wise to the dest vector.
  virtual bool AddEmbedding(const TensorView<int>& sparse_features, float* dest,
                            int dest_size) const = 0;

  // Returns true when the model is ready to be used, false otherwise.
  virtual bool IsReady() const { return true; }
};

class TFLiteEmbeddingExecutor : public EmbeddingExecutor {
 public:
  static std::unique_ptr<TFLiteEmbeddingExecutor> FromBuffer(
      const flatbuffers::Vector<uint8_t>* model_spec_buffer, int embedding_size,
      int quantization_bits,
      const Model_::EmbeddingPruningMask* embedding_pruning_mask = nullptr);

  // Embeds the sparse_features into a dense embedding and adds (+) it
  // element-wise to the dest vector.
  bool AddEmbedding(const TensorView<int>& sparse_features, float* dest,
                    int dest_size) const;

  // Auxiliary function for computing prefixes used in implementation of
  // efficient mask indexing data structure.
  void ComputePrefixCounts();

  // Function implementing mask indexing based on efficient data structure
  int PruneBucketId(int bucket_id) const;

 protected:
  explicit TFLiteEmbeddingExecutor(
      std::unique_ptr<TfLiteModelExecutor> executor, int quantization_bits,
      int num_buckets, int bytes_per_embedding, int output_embedding_size,
      const TfLiteTensor* scales, const TfLiteTensor* embeddings,
      std::unique_ptr<tflite::Interpreter> interpreter,
      const Model_::EmbeddingPruningMask* embedding_pruning_mask = nullptr);

  std::unique_ptr<TfLiteModelExecutor> executor_;

  int quantization_bits_;
  int num_buckets_ = -1;
  int bytes_per_embedding_ = -1;
  int output_embedding_size_ = -1;
  const TfLiteTensor* scales_ = nullptr;
  const TfLiteTensor* embeddings_ = nullptr;

  // NOTE: This interpreter is used in a read-only way (as a storage for the
  // model params), thus is still thread-safe.
  std::unique_ptr<tflite::Interpreter> interpreter_;

  std::vector<uint64> pruning_mask_;
  std::vector<uint16> prefix_counts_;
  int full_num_buckets_ = -1;

  // Index of row of embedding table corresponding to all pruned buckets.
  int pruned_row_bucket_id_ = -1;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_ANNOTATOR_MODEL_EXECUTOR_H_
