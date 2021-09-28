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

#include "run_tflite.h"

#include "tensorflow/lite/delegates/nnapi/nnapi_delegate.h"
#include "tensorflow/lite/kernels/register.h"

#include <android/log.h>
#include <dlfcn.h>
#include <sys/time.h>
#include <cstdio>

#define LOG_TAG "NN_BENCHMARK"

#define FATAL(fmt, ...)                                                  \
  do {                                                                   \
    __android_log_print(ANDROID_LOG_FATAL, LOG_TAG, fmt, ##__VA_ARGS__); \
    assert(false);                                                       \
  } while (0)

namespace {

long long currentTimeInUsec() {
  timeval tv;
  gettimeofday(&tv, NULL);
  return ((tv.tv_sec * 1000000L) + tv.tv_usec);
}

// Workaround for build systems that make difficult to pick the correct NDK API
// level. NDK tracing methods are dynamically loaded from libandroid.so.
typedef void* (*fp_ATrace_beginSection)(const char* sectionName);
typedef void* (*fp_ATrace_endSection)();
struct TraceFunc {
  fp_ATrace_beginSection ATrace_beginSection;
  fp_ATrace_endSection ATrace_endSection;
};
TraceFunc setupTraceFunc() {
  void* lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
  if (lib == nullptr) {
    FATAL("unable to open libandroid.so");
  }
  return {
      reinterpret_cast<fp_ATrace_beginSection>(
          dlsym(lib, "ATrace_beginSection")),
      reinterpret_cast<fp_ATrace_endSection>(dlsym(lib, "ATrace_endSection"))};
}
static TraceFunc kTraceFunc{setupTraceFunc()};

}  // namespace

BenchmarkModel* BenchmarkModel::create(const char* modelfile, bool use_nnapi,
                                       bool enable_intermediate_tensors_dump,
                                       const char* nnapi_device_name) {
    BenchmarkModel* model = new BenchmarkModel();
    if (!model->init(modelfile, use_nnapi, enable_intermediate_tensors_dump,
                     nnapi_device_name)) {
      delete model;
      return nullptr;
    }
    return model;
}

bool BenchmarkModel::init(const char* modelfile, bool use_nnapi,
                          bool enable_intermediate_tensors_dump,
                          const char* nnapi_device_name) {
  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "BenchmarkModel %s",
                      modelfile);

  // Memory map the model. NOTE this needs lifetime greater than or equal
  // to interpreter context.
  mTfliteModel = tflite::FlatBufferModel::BuildFromFile(modelfile);
  if (!mTfliteModel) {
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Failed to load model %s",
                        modelfile);
    return false;
  }

  tflite::ops::builtin::BuiltinOpResolver resolver;
  tflite::InterpreterBuilder(*mTfliteModel, resolver)(&mTfliteInterpreter);
  if (!mTfliteInterpreter) {
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                        "Failed to create TFlite interpreter");
    return false;
  }

  if (enable_intermediate_tensors_dump) {
    // Make output of every op a model output. This way we will be able to
    // fetch each intermediate tensor when running with delegates.
    outputs.clear();
    for (size_t node = 0; node < mTfliteInterpreter->nodes_size(); ++node) {
      auto node_outputs =
          mTfliteInterpreter->node_and_registration(node)->first.outputs;
      outputs.insert(outputs.end(), node_outputs->data,
                     node_outputs->data + node_outputs->size);
    }
    mTfliteInterpreter->SetOutputs(outputs);
  }

  // Allow Fp16 precision for all models
  mTfliteInterpreter->SetAllowFp16PrecisionForFp32(true);

  if (use_nnapi) {
    if (nnapi_device_name != nullptr) {
      __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Running NNAPI on device %s",
                          nnapi_device_name);
    }
    tflite::StatefulNnApiDelegate::Options nnapi_options;
    nnapi_options.accelerator_name = nnapi_device_name;
    mTfliteNnapiDelegate = std::make_unique<tflite::StatefulNnApiDelegate>(nnapi_options);
    if (mTfliteInterpreter->ModifyGraphWithDelegate(mTfliteNnapiDelegate.get()) != kTfLiteOk) {
      __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                          "Failed to initialize NNAPI Delegate");
      return false;
    }
  }
  return true;
}

BenchmarkModel::BenchmarkModel() {}
BenchmarkModel::~BenchmarkModel() {}

bool BenchmarkModel::setInput(const uint8_t* dataPtr, size_t length) {
  int input = mTfliteInterpreter->inputs()[0];
  auto* input_tensor = mTfliteInterpreter->tensor(input);

  switch (input_tensor->type) {
    case kTfLiteFloat32:
    case kTfLiteUInt8: {
      void* raw = input_tensor->data.raw;
      memcpy(raw, dataPtr, length);
      break;
    }
    default:
      __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                          "Input tensor type not supported");
      return false;
  }
  return true;
}
void BenchmarkModel::saveInferenceOutput(InferenceResult* result,
                                         int output_index) {
  int output = mTfliteInterpreter->outputs()[output_index];
  auto* output_tensor = mTfliteInterpreter->tensor(output);
  auto& sink = result->inferenceOutputs[output_index];
  sink.insert(sink.end(), output_tensor->data.uint8,
              output_tensor->data.uint8 + output_tensor->bytes);
}

void BenchmarkModel::getOutputError(const uint8_t* expected_data, size_t length,
                                    InferenceResult* result, int output_index) {
  int output = mTfliteInterpreter->outputs()[output_index];
  auto* output_tensor = mTfliteInterpreter->tensor(output);
  if (output_tensor->bytes != length) {
    FATAL("Wrong size of output tensor, expected %zu, is %zu",
          output_tensor->bytes, length);
  }

  size_t elements_count = 0;
  float err_sum = 0.0;
  float max_error = 0.0;
  switch (output_tensor->type) {
    case kTfLiteUInt8: {
      uint8_t* output_raw = mTfliteInterpreter->typed_tensor<uint8_t>(output);
      elements_count = output_tensor->bytes;
      for (size_t i = 0; i < output_tensor->bytes; ++i) {
        float err = ((float)output_raw[i]) - ((float)expected_data[i]);
        if (err > max_error) max_error = err;
        err_sum += err * err;
      }
      break;
    }
    case kTfLiteFloat32: {
      const float* expected = reinterpret_cast<const float*>(expected_data);
      float* output_raw = mTfliteInterpreter->typed_tensor<float>(output);
      elements_count = output_tensor->bytes / sizeof(float);
      for (size_t i = 0; i < output_tensor->bytes / sizeof(float); ++i) {
        float err = output_raw[i] - expected[i];
        if (err > max_error) max_error = err;
        err_sum += err * err;
      }
      break;
    }
    default:
      FATAL("Output sensor type %d not supported", output_tensor->type);
  }
  result->meanSquareErrors[output_index] = err_sum / elements_count;
  result->maxSingleErrors[output_index] = max_error;
}

bool BenchmarkModel::resizeInputTensors(std::vector<int> shape) {
  // The benchmark only expects single input tensor, hardcoded as 0.
  int input = mTfliteInterpreter->inputs()[0];
  mTfliteInterpreter->ResizeInputTensor(input, shape);
  if (mTfliteInterpreter->AllocateTensors() != kTfLiteOk) {
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                        "Failed to allocate tensors!");
    return false;
  }
  return true;
}

bool BenchmarkModel::runInference() {
  auto status = mTfliteInterpreter->Invoke();
  if (status != kTfLiteOk) {
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Failed to invoke: %d!",
                        (int)status);
    return false;
  }
  return true;
}

bool BenchmarkModel::resetStates() {
  auto status = mTfliteInterpreter->ResetVariableTensors();
  if (status != kTfLiteOk) {
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                        "Failed to reset variable tensors: %d!", (int)status);
    return false;
  }
  return true;
}

bool BenchmarkModel::benchmark(
    const std::vector<InferenceInOutSequence>& inOutData,
    int seqInferencesMaxCount, float timeout, int flags,
    std::vector<InferenceResult>* results) {
  if (inOutData.empty()) {
    __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                        "Input/output vector is empty");
    return true;
  }

  float inferenceTotal = 0.0;
  for (int seqInferenceIndex = 0; seqInferenceIndex < seqInferencesMaxCount;
       ++seqInferenceIndex) {
    resetStates();

    const int inputOutputSequenceIndex = seqInferenceIndex % inOutData.size();
    const InferenceInOutSequence& seq = inOutData[inputOutputSequenceIndex];
    for (int i = 0; i < seq.size(); ++i) {
      const InferenceInOut& data = seq[i];

      // For NNAPI systrace usage documentation, see
      // frameworks/ml/nn/common/include/Tracing.h.
      kTraceFunc.ATrace_beginSection("[NN_LA_PE]BenchmarkModel::benchmark");
      kTraceFunc.ATrace_beginSection("[NN_LA_PIO]BenchmarkModel::input");
      if (data.input) {
        setInput(data.input, data.input_size);
      } else {
        int input = mTfliteInterpreter->inputs()[0];
        auto* input_tensor = mTfliteInterpreter->tensor(input);
        if (!data.createInput((uint8_t*)input_tensor->data.raw,
                              input_tensor->bytes)) {
          __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                              "Input creation %d failed", i);
          return false;
        }
      }
      kTraceFunc.ATrace_endSection();
      long long startTime = currentTimeInUsec();
      const bool success = runInference();
      kTraceFunc.ATrace_endSection();
      long long endTime = currentTimeInUsec();
      if (!success) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Inference %d failed",
                            i);
        return false;
      }

      float inferenceTime =
          static_cast<float>(endTime - startTime) / 1000000.0f;
      size_t outputsCount = mTfliteInterpreter->outputs().size();
      InferenceResult result{
          inferenceTime, {}, {}, {}, inputOutputSequenceIndex, i};
      result.meanSquareErrors.resize(outputsCount);
      result.maxSingleErrors.resize(outputsCount);
      result.inferenceOutputs.resize(outputsCount);

      if ((flags & FLAG_IGNORE_GOLDEN_OUTPUT) == 0) {
        if (outputsCount != data.outputs.size()) {
          __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                              "Golden/actual outputs (%zu/%zu) count mismatch",
                              data.outputs.size(), outputsCount);
          return false;
        }
        for (int j = 0; j < outputsCount; ++j) {
          getOutputError(data.outputs[j].ptr, data.outputs[j].size, &result, j);
        }
      }

      if ((flags & FLAG_DISCARD_INFERENCE_OUTPUT) == 0) {
        for (int j = 0; j < outputsCount; ++j) {
          saveInferenceOutput(&result, j);
        }
      }
      results->push_back(result);
      inferenceTotal += inferenceTime;
    }

    // Timeout?
    if (timeout > 0.001 && inferenceTotal > timeout) {
      return true;
    }
  }
  return true;
}

bool BenchmarkModel::dumpAllLayers(
    const char* path, const std::vector<InferenceInOutSequence>& inOutData) {
  if (inOutData.empty()) {
    FATAL("Input/output vector is empty");
  }

  for (int seqInferenceIndex = 0; seqInferenceIndex < inOutData.size();
       ++seqInferenceIndex) {
    resetStates();

    const InferenceInOutSequence& seq = inOutData[seqInferenceIndex];
    for (int i = 0; i < seq.size(); ++i) {
      const InferenceInOut& data = seq[i];
      setInput(data.input, data.input_size);
      const bool success = runInference();
      if (!success) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Inference %d failed",
                            i);
        return false;
      }

      // The order of the tensor is not sorted by the tensor index
      for (int tensor_order = 0; tensor_order < outputs.size(); ++tensor_order) {
        int tensor_index = outputs[tensor_order];
        auto* output_tensor = mTfliteInterpreter->tensor(tensor_index);
        if (output_tensor->data.raw == nullptr) {
          __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                      "output_tensor->data.raw == nullptr at index %d ", tensor_index);
          continue;
        }
        char fullpath[1024];
        snprintf(fullpath, 1024, "%s/dump_%.3d_seq_%.3d_order_%.3d_tensor_%.3d", path,
                 seqInferenceIndex, i, tensor_order, tensor_index);
        FILE* f = fopen(fullpath, "wb");
        fwrite(output_tensor->data.raw, output_tensor->bytes, 1, f);
        fclose(f);
      }
    }
  }
  return true;
}
