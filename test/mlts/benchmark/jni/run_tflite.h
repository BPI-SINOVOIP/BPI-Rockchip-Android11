/**
 * Copyright 2017 The Android Open Source Project
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

#ifndef COM_EXAMPLE_ANDROID_NN_BENCHMARK_RUN_TFLITE_H
#define COM_EXAMPLE_ANDROID_NN_BENCHMARK_RUN_TFLITE_H

#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/model.h"

#include <unistd.h>
#include <vector>

struct InferenceOutput {
  uint8_t* ptr;
  size_t size;
};

// Inputs and expected outputs for inference
struct InferenceInOut {
  // Input can either be directly specified as a pointer or indirectly with
  // the createInput callback. This is needed for large datasets where
  // allocating memory for all inputs at once is not feasible.
  uint8_t* input;
  size_t input_size;

  std::vector<InferenceOutput> outputs;
  std::function<bool(uint8_t*, size_t)> createInput;
};

// Inputs and expected outputs for an inference sequence.
using InferenceInOutSequence = std::vector<InferenceInOut>;

// Result of a single inference
struct InferenceResult {
  float computeTimeSec;
  // MSE for each output
  std::vector<float> meanSquareErrors;
  // Max single error for each output
  std::vector<float> maxSingleErrors;
  // Outputs
  std::vector<std::vector<uint8_t>> inferenceOutputs;
  int inputOutputSequenceIndex;
  int inputOutputIndex;
};

/** Discard inference output in inference results. */
const int FLAG_DISCARD_INFERENCE_OUTPUT = 1 << 0;
/** Do not expect golden output for inference inputs. */
const int FLAG_IGNORE_GOLDEN_OUTPUT = 1 << 1;

class BenchmarkModel {
 public:
  ~BenchmarkModel();

  static BenchmarkModel* create(const char* modelfile, bool use_nnapi,
                                bool enable_intermediate_tensors_dump,
                                const char* nnapi_device_name = nullptr);

  bool resizeInputTensors(std::vector<int> shape);
  bool setInput(const uint8_t* dataPtr, size_t length);
  bool runInference();
  // Resets TFLite states (RNN/LSTM states etc).
  bool resetStates();

  bool benchmark(const std::vector<InferenceInOutSequence>& inOutData,
                 int seqInferencesMaxCount, float timeout, int flags,
                 std::vector<InferenceResult>* result);

  bool dumpAllLayers(const char* path,
                     const std::vector<InferenceInOutSequence>& inOutData);

 private:
  BenchmarkModel();
  bool init(const char* modelfile, bool use_nnapi,
            bool enable_intermediate_tensors_dump,
            const char* nnapi_device_name);

  void getOutputError(const uint8_t* dataPtr, size_t length,
                      InferenceResult* result, int output_index);
  void saveInferenceOutput(InferenceResult* result, int output_index);

  std::unique_ptr<tflite::FlatBufferModel> mTfliteModel;
  std::unique_ptr<tflite::Interpreter> mTfliteInterpreter;
  std::unique_ptr<tflite::StatefulNnApiDelegate> mTfliteNnapiDelegate;
  // Store indices of output tensors, used to dump intermediate tensors
  std::vector<int> outputs;
};

#endif  // COM_EXAMPLE_ANDROID_NN_BENCHMARK_RUN_TFLITE_H
