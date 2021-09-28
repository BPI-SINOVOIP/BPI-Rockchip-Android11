/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "model.h"

#include "sine_model_data.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

//  The following registration code is generated. Check the following commit for
//  details.
//  https://github.com/tensorflow/tensorflow/commit/098556c3a96e1d51f79606c0834547cb2aa20908

namespace {
void RegisterSelectedOps(::tflite::MicroMutableOpResolver *resolver) {
  resolver->AddBuiltin(
      ::tflite::BuiltinOperator_FULLY_CONNECTED,
      // For now the op version is not supported in the generated code, so this
      // version still needs to added manually.
      ::tflite::ops::micro::Register_FULLY_CONNECTED(), 1, 4);
}
}  // namespace

namespace demo {
float run(float x_val) {
  tflite::MicroErrorReporter micro_error_reporter;
  const tflite::Model *model = tflite::GetModel(g_sine_model_data);
  // TODO(wangtz): Check for schema version.

  tflite::MicroMutableOpResolver resolver;
  RegisterSelectedOps(&resolver);
  constexpr int kTensorAreanaSize = 2 * 1024;
  uint8_t tensor_arena[kTensorAreanaSize];

  tflite::MicroInterpreter interpreter(
      model, resolver, tensor_arena, kTensorAreanaSize, &micro_error_reporter);
  interpreter.AllocateTensors();

  TfLiteTensor *input = interpreter.input(0);
  TfLiteTensor *output = interpreter.output(0);
  input->data.f[0] = x_val;
  TfLiteStatus invoke_status = interpreter.Invoke();
  if (invoke_status != kTfLiteOk) {
    micro_error_reporter.ReportError(nullptr, "Internal error: invoke failed.");
    return 0.0;
  }
  float y_val = output->data.f[0];
  return y_val;
}

}  // namespace demo
