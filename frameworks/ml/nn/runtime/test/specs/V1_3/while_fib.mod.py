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

# Model: given n, produces [fib(n), fib(n + 1)].
#
# fib = [1, 1]
# i = 1
# while i < n:
#     fib = matmul(fib, [0 1;
#                        1 1])
#     i = i + 1

FibType = ["TENSOR_FLOAT32", [1, 2]]
FibTypeQuant8 = ["TENSOR_QUANT8_ASYMM", 1.0, 0]
FibTypeQuant8Signed = ["TENSOR_QUANT8_ASYMM_SIGNED", 1.0, 0]
CounterType = ["TENSOR_INT32", [1]]
BoolType = ["TENSOR_BOOL8", [1]]

def MakeConditionModel():
  fib = Input("fib", FibType)
  i = Input("i", CounterType)
  n = Input("n", CounterType)
  out = Output("out", BoolType)
  model = Model()
  model.IdentifyInputs(fib, i, n)
  model.IdentifyOutputs(out)
  model.Operation("LESS", i, n).To(out)

  quant8 = DataTypeConverter().Identify({
      fib: FibTypeQuant8,
  })
  quant8_signed = DataTypeConverter().Identify({
      fib: FibTypeQuant8Signed,
  })

  return SubgraphReference("cond", model), quant8, quant8_signed

def MakeBodyModel():
  fib = Input("fib", FibType)
  i = Input("i", CounterType)
  n = Input("n", CounterType)
  fib_out = Output("fib_out", FibType)
  i_out = Output("i_out", CounterType)
  matrix = Parameter("matrix", ["TENSOR_FLOAT32", [2, 2]], [0, 1, 1, 1])
  zero_bias = Parameter("zero_bias", ["TENSOR_FLOAT32", [2]], [0, 0])
  model = Model()
  model.IdentifyInputs(fib, i, n)
  model.IdentifyOutputs(fib_out, i_out)
  model.Operation("ADD", i, [1], 0).To(i_out)
  model.Operation("FULLY_CONNECTED", fib, matrix, zero_bias, 0).To(fib_out)

  quant8 = DataTypeConverter().Identify({
      fib: FibTypeQuant8,
      matrix: FibTypeQuant8,
      zero_bias: ["TENSOR_INT32", 1.0, 0],
      fib_out: FibTypeQuant8,
  })
  quant8_signed = DataTypeConverter().Identify({
      fib: FibTypeQuant8Signed,
      matrix: FibTypeQuant8Signed,
      zero_bias: ["TENSOR_INT32", 1.0, 0],
      fib_out: FibTypeQuant8Signed,
  })

  return SubgraphReference("body", model), quant8, quant8_signed

def Test(n_data, fib_data, add_unused_output=False):
  n = Input("n", CounterType)
  fib_out = Output("fib_out", FibType)
  cond, cond_quant8, cond_quant8_signed = MakeConditionModel()
  body, body_quant8, body_quant8_signed = MakeBodyModel()
  fib_init = Parameter("fib_init", FibType, [1, 1])
  i_init = [1]
  outputs = [fib_out]
  if add_unused_output:
    i_out = Internal("i_out", CounterType)  # Unused.
    outputs.append(i_out)
  model = Model().Operation("WHILE", cond, body, fib_init, i_init, n).To(outputs)

  quant8 = DataTypeConverter().Identify({
      fib_init: FibTypeQuant8,
      fib_out: FibTypeQuant8,
      cond: cond_quant8,
      body: body_quant8,
  })
  quant8_signed = DataTypeConverter().Identify({
      fib_init: FibTypeQuant8Signed,
      fib_out: FibTypeQuant8Signed,
      cond: cond_quant8_signed,
      body: body_quant8_signed,
  })

  name = "n_{}".format(n_data)
  if add_unused_output:
    name += "_unused_output"
  example = Example({n: [n_data], fib_out: fib_data}, name=name)
  example.AddVariations("relaxed", "float16", quant8, quant8_signed)
  example.AddVariations(AllOutputsAsInternalCoverter())

for use_shm_for_weights in [False, True]:
  Configuration.use_shm_for_weights = use_shm_for_weights
  # Fibonacci numbers: 1 1 2 3 5 8
  Test(n_data=1, fib_data=[1, 1], add_unused_output=True)
  Test(n_data=2, fib_data=[1, 2], add_unused_output=True)
  Test(n_data=3, fib_data=[2, 3], add_unused_output=True)
  Test(n_data=4, fib_data=[3, 5])
  Test(n_data=5, fib_data=[5, 8])
