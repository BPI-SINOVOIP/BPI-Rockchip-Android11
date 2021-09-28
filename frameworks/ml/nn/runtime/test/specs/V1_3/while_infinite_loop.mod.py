#
# Copyright (C) 2020 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Model: given n <= 1.0, never returns.
# Expected result: execution aborted after a timeout.
#
# i = 1.0
# while i >= n:
#     i = i + 1.0

CounterType = ["TENSOR_FLOAT32", [1]]
BoolType = ["TENSOR_BOOL8", [1]]

def MakeConditionModel():
  i = Input("i", CounterType)
  n = Input("n", CounterType)
  out = Output("out", BoolType)
  model = Model()
  model.IdentifyInputs(i, n)
  model.IdentifyOutputs(out)
  model.Operation("GREATER_EQUAL", i, n).To(out)
  return model

def MakeBodyModel():
  i = Input("i", CounterType)
  n = Input("n", CounterType)
  i_out = Output("i_out", CounterType)
  model = Model()
  model.IdentifyInputs(i, n)
  model.IdentifyOutputs(i_out)
  model.Operation("ADD", i, [1.0], 0).To(i_out)
  return model

n = Input("n", CounterType)
i_out = Output("i_out", CounterType)
cond = MakeConditionModel()
body = MakeBodyModel()
i_init = [1.0]
model = Model().Operation("WHILE", cond, body, i_init, n).To(i_out)

quant8 = DataTypeConverter("quant8", scale=1.0, zeroPoint=127)
quant8_signed = DataTypeConverter("quant8_signed", scale=1.0, zeroPoint=0)

example = Example({n: [0.0], i_out: [0.0]}, model=model)
example.AddVariations("relaxed", "float16", quant8, quant8_signed)
example.DisableLifeTimeVariation()
example.DisableDynamicOutputShapeVariation()
example.ExpectFailure()
