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

#include "utils/tflite-model-executor.h"

#include "utils/base/logging.h"
#include "tensorflow/lite/kernels/register.h"

// Forward declaration of custom TensorFlow Lite ops for registration.
namespace tflite {
namespace ops {
namespace builtin {
TfLiteRegistration* Register_ADD();
TfLiteRegistration* Register_CONCATENATION();
TfLiteRegistration* Register_CONV_2D();
TfLiteRegistration* Register_EQUAL();
TfLiteRegistration* Register_FULLY_CONNECTED();
TfLiteRegistration* Register_GREATER_EQUAL();
TfLiteRegistration* Register_L2_NORMALIZATION();
TfLiteRegistration* Register_MUL();
TfLiteRegistration* Register_RESHAPE();
TfLiteRegistration* Register_REDUCE_MAX();
TfLiteRegistration* Register_REDUCE_ANY();
TfLiteRegistration* Register_SOFTMAX();
TfLiteRegistration* Register_GATHER();
TfLiteRegistration* Register_TRANSPOSE();
TfLiteRegistration* Register_SUB();
TfLiteRegistration* Register_DIV();
TfLiteRegistration* Register_STRIDED_SLICE();
TfLiteRegistration* Register_EXP();
TfLiteRegistration* Register_TOPK_V2();
TfLiteRegistration* Register_SPLIT();
TfLiteRegistration* Register_CAST();
TfLiteRegistration* Register_MAXIMUM();
TfLiteRegistration* Register_MINIMUM();
TfLiteRegistration* Register_NEG();
TfLiteRegistration* Register_SLICE();
TfLiteRegistration* Register_LOG();
TfLiteRegistration* Register_SUM();
TfLiteRegistration* Register_PACK();
TfLiteRegistration* Register_DEQUANTIZE();
TfLiteRegistration* Register_MEAN();
TfLiteRegistration* Register_LESS();
TfLiteRegistration* Register_TILE();
TfLiteRegistration* Register_SQUARED_DIFFERENCE();
TfLiteRegistration* Register_RSQRT();
TfLiteRegistration* Register_LOG_SOFTMAX();
TfLiteRegistration* Register_WHERE();
}  // namespace builtin
}  // namespace ops
}  // namespace tflite

#ifdef TC3_WITH_ACTIONS_OPS
#include "utils/tflite/dist_diversification.h"
#include "utils/tflite/text_encoder.h"
#include "utils/tflite/token_encoder.h"

void RegisterSelectedOps(tflite::MutableOpResolver* resolver) {
  resolver->AddBuiltin(tflite::BuiltinOperator_ADD,
                       tflite::ops::builtin::Register_ADD(),
                       /*min_version=*/1,
                       /*max_version=*/2);
  resolver->AddBuiltin(tflite::BuiltinOperator_CONCATENATION,
                       tflite::ops::builtin::Register_CONCATENATION(),
                       /*min_version=*/1,
                       /*max_version=*/2);
  resolver->AddBuiltin(tflite::BuiltinOperator_CONV_2D,
                       tflite::ops::builtin::Register_CONV_2D(),
                       /*min_version=*/1,
                       /*max_version=*/3);
  resolver->AddBuiltin(::tflite::BuiltinOperator_EQUAL,
                       ::tflite::ops::builtin::Register_EQUAL());

  resolver->AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
                       tflite::ops::builtin::Register_FULLY_CONNECTED(),
                       /*min_version=*/1,
                       /*max_version=*/4);
  resolver->AddBuiltin(::tflite::BuiltinOperator_GREATER_EQUAL,
                       ::tflite::ops::builtin::Register_GREATER_EQUAL());
  resolver->AddBuiltin(tflite::BuiltinOperator_L2_NORMALIZATION,
                       tflite::ops::builtin::Register_L2_NORMALIZATION(),
                       /*min_version=*/1,
                       /*max_version=*/2);
  resolver->AddBuiltin(tflite::BuiltinOperator_MUL,
                       tflite::ops::builtin::Register_MUL());
  resolver->AddBuiltin(tflite::BuiltinOperator_RESHAPE,
                       tflite::ops::builtin::Register_RESHAPE());
  resolver->AddBuiltin(::tflite::BuiltinOperator_REDUCE_MAX,
                       ::tflite::ops::builtin::Register_REDUCE_MAX());
  resolver->AddBuiltin(::tflite::BuiltinOperator_REDUCE_ANY,
                       ::tflite::ops::builtin::Register_REDUCE_ANY());
  resolver->AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                       tflite::ops::builtin::Register_SOFTMAX(),
                       /*min_version=*/1,
                       /*max_version=*/2);
  resolver->AddBuiltin(tflite::BuiltinOperator_GATHER,
                       tflite::ops::builtin::Register_GATHER(),
                       /*min_version=*/1,
                       /*max_version=*/2);
  resolver->AddBuiltin(tflite::BuiltinOperator_TRANSPOSE,
                       tflite::ops::builtin::Register_TRANSPOSE(),
                       /*min_version=*/1,
                       /*max_version=*/2);
  resolver->AddBuiltin(tflite::BuiltinOperator_SUB,
                       tflite::ops::builtin::Register_SUB(),
                       /*min_version=*/1,
                       /*max_version=*/2);
  resolver->AddBuiltin(tflite::BuiltinOperator_DIV,
                       tflite::ops::builtin::Register_DIV());
  resolver->AddBuiltin(tflite::BuiltinOperator_STRIDED_SLICE,
                       tflite::ops::builtin::Register_STRIDED_SLICE(),
                       /*min_version=*/1,
                       /*max_version=*/2);
  resolver->AddBuiltin(tflite::BuiltinOperator_EXP,
                       tflite::ops::builtin::Register_EXP());
  resolver->AddBuiltin(tflite::BuiltinOperator_TOPK_V2,
                       tflite::ops::builtin::Register_TOPK_V2(),
                       /*min_version=*/1,
                       /*max_version=*/2);
  resolver->AddBuiltin(tflite::BuiltinOperator_SPLIT,
                       tflite::ops::builtin::Register_SPLIT(),
                       /*min_version=*/1,
                       /*max_version=*/3);
  resolver->AddBuiltin(tflite::BuiltinOperator_CAST,
                       tflite::ops::builtin::Register_CAST());
  resolver->AddBuiltin(tflite::BuiltinOperator_MAXIMUM,
                       tflite::ops::builtin::Register_MAXIMUM(),
                       /*min_version=*/1,
                       /*max_version=*/2);
  resolver->AddBuiltin(tflite::BuiltinOperator_MINIMUM,
                       tflite::ops::builtin::Register_MINIMUM(),
                       /*min_version=*/1,
                       /*max_version=*/2);
  resolver->AddBuiltin(tflite::BuiltinOperator_NEG,
                       tflite::ops::builtin::Register_NEG());
  resolver->AddBuiltin(tflite::BuiltinOperator_SLICE,
                       tflite::ops::builtin::Register_SLICE(),
                       /*min_version=*/1,
                       /*max_version=*/2);
  resolver->AddBuiltin(tflite::BuiltinOperator_LOG,
                       tflite::ops::builtin::Register_LOG());
  resolver->AddBuiltin(tflite::BuiltinOperator_SUM,
                       tflite::ops::builtin::Register_SUM());
  resolver->AddBuiltin(tflite::BuiltinOperator_PACK,
                       tflite::ops::builtin::Register_PACK(),
                       /*min_version=*/1,
                       /*max_version=*/2);
  resolver->AddBuiltin(tflite::BuiltinOperator_DEQUANTIZE,
                       tflite::ops::builtin::Register_DEQUANTIZE(),
                       /*min_version=*/1,
                       /*max_version=*/2);
  resolver->AddBuiltin(tflite::BuiltinOperator_MEAN,
                       tflite::ops::builtin::Register_MEAN());
  resolver->AddBuiltin(tflite::BuiltinOperator_LESS,
                       tflite::ops::builtin::Register_LESS());
  resolver->AddBuiltin(tflite::BuiltinOperator_TILE,
                       tflite::ops::builtin::Register_TILE());
  resolver->AddBuiltin(tflite::BuiltinOperator_SQUARED_DIFFERENCE,
                       tflite::ops::builtin::Register_SQUARED_DIFFERENCE());
  resolver->AddBuiltin(tflite::BuiltinOperator_RSQRT,
                       tflite::ops::builtin::Register_RSQRT());
  resolver->AddBuiltin(tflite::BuiltinOperator_LOG_SOFTMAX,
                       tflite::ops::builtin::Register_LOG_SOFTMAX());
  resolver->AddBuiltin(::tflite::BuiltinOperator_WHERE,
                       ::tflite::ops::builtin::Register_WHERE());
}
#else
void RegisterSelectedOps(tflite::MutableOpResolver* resolver) {
  resolver->AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
                       tflite::ops::builtin::Register_FULLY_CONNECTED());
}
#endif  // TC3_WITH_ACTIONS_OPS

namespace libtextclassifier3 {

inline std::unique_ptr<tflite::OpResolver> BuildOpResolver() {
#ifdef TC3_USE_SELECTIVE_REGISTRATION
  std::unique_ptr<tflite::MutableOpResolver> resolver(
      new tflite::MutableOpResolver);
  RegisterSelectedOps(resolver.get());
#else
  std::unique_ptr<tflite::ops::builtin::BuiltinOpResolver> resolver(
      new tflite::ops::builtin::BuiltinOpResolver);
#endif
#ifdef TC3_WITH_ACTIONS_OPS
  resolver->AddCustom("DistanceDiversification",
                      tflite::ops::custom::Register_DISTANCE_DIVERSIFICATION());
  resolver->AddCustom("TextEncoder",
                      tflite::ops::custom::Register_TEXT_ENCODER());
  resolver->AddCustom("TokenEncoder",
                      tflite::ops::custom::Register_TOKEN_ENCODER());
#endif  // TC3_WITH_ACTIONS_OPS
  return std::unique_ptr<tflite::OpResolver>(std::move(resolver));
}

std::unique_ptr<const tflite::FlatBufferModel> TfLiteModelFromModelSpec(
    const tflite::Model* model_spec) {
  std::unique_ptr<const tflite::FlatBufferModel> model(
      tflite::FlatBufferModel::BuildFromModel(model_spec));
  if (!model || !model->initialized()) {
    TC3_LOG(ERROR) << "Could not build TFLite model from a model spec.";
    return nullptr;
  }
  return model;
}

std::unique_ptr<const tflite::FlatBufferModel> TfLiteModelFromBuffer(
    const flatbuffers::Vector<uint8_t>* model_spec_buffer) {
  const tflite::Model* model =
      flatbuffers::GetRoot<tflite::Model>(model_spec_buffer->data());
  flatbuffers::Verifier verifier(model_spec_buffer->data(),
                                 model_spec_buffer->size());
  if (!model->Verify(verifier)) {
    return nullptr;
  }
  return TfLiteModelFromModelSpec(model);
}

TfLiteModelExecutor::TfLiteModelExecutor(
    std::unique_ptr<const tflite::FlatBufferModel> model)
    : model_(std::move(model)), resolver_(BuildOpResolver()) {}
TfLiteModelExecutor::TfLiteModelExecutor(
    std::unique_ptr<const tflite::FlatBufferModel> model,
    std::unique_ptr<tflite::OpResolver> resolver)
    : model_(std::move(model)), resolver_(std::move(resolver)) {}

std::unique_ptr<tflite::Interpreter> TfLiteModelExecutor::CreateInterpreter()
    const {
  std::unique_ptr<tflite::Interpreter> interpreter;
  tflite::InterpreterBuilder(*model_, *resolver_)(&interpreter);
  return interpreter;
}

template <>
void TfLiteModelExecutor::SetInput(const int input_index,
                                   const std::vector<std::string>& input_data,
                                   tflite::Interpreter* interpreter) const {
  tflite::DynamicBuffer buf;
  for (const std::string& s : input_data) {
    buf.AddString(s.data(), s.length());
  }
  buf.WriteToTensorAsVector(
      interpreter->tensor(interpreter->inputs()[input_index]));
}

template <>
std::vector<tflite::StringRef> TfLiteModelExecutor::Output(
    const int output_index, const tflite::Interpreter* interpreter) const {
  const TfLiteTensor* output_tensor =
      interpreter->tensor(interpreter->outputs()[output_index]);
  const int num_strings = tflite::GetStringCount(output_tensor);
  std::vector<tflite::StringRef> output(num_strings);
  for (int i = 0; i < num_strings; i++) {
    output[i] = tflite::GetString(output_tensor, i);
  }
  return output;
}

template <>
std::vector<std::string> TfLiteModelExecutor::Output(
    const int output_index, const tflite::Interpreter* interpreter) const {
  std::vector<std::string> output;
  for (const tflite::StringRef& s :
       Output<tflite::StringRef>(output_index, interpreter)) {
    output.push_back(std::string(s.str, s.len));
  }
  return output;
}

}  // namespace libtextclassifier3
