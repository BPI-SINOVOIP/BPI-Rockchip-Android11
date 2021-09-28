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

# Model: given n >= 2, produces [fib(1), ..., fib(n)].
#
# i = 2
# fib = [1, 1]                         # shape=[1, i + 1]
# while i < n:
#     last2 = fib[0:1, (i - 2):i]      # shape=[1, 2]
#     new_fib = matmul(last2, [1; 1])  # shape=[1, 1]
#     fib = append(fib, new_fib)       # shape=[1, i + 2]
#     i = i + 1

FibType = ["TENSOR_FLOAT32", [1, 0]]  # Unknown dimension.
CounterType = ["TENSOR_INT32", [1]]
BoolType = ["TENSOR_BOOL8", [1]]

Quant8Params = ["TENSOR_QUANT8_ASYMM", 1.0, 0]
Quant8SignedParams = ["TENSOR_QUANT8_ASYMM_SIGNED", 1.0, 0]

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
      fib: Quant8Params,
  })
  quant8_signed = DataTypeConverter().Identify({
      fib: Quant8SignedParams,
  })

  return SubgraphReference("cond", model), quant8, quant8_signed

def MakeBodyModel():
  fib = Input("fib", FibType)
  i = Input("i", CounterType)
  n = Input("n", CounterType)
  fib_out = Output("fib_out", FibType)
  i_out = Output("i_out", CounterType)
  matrix = Parameter("matrix", ["TENSOR_FLOAT32", [1, 2]], [1, 1])
  zero_bias = Parameter("zero_bias", ["TENSOR_FLOAT32", [1]], [0])
  i_minus_2 = Internal("i_minus_2", ["TENSOR_INT32", [1]])
  slice_start = Internal("slice_start", ["TENSOR_INT32", [2]])
  slice_size = Parameter("slice_size", ["TENSOR_INT32", [2]], [1, 2])
  last2 = Internal("last2", ["TENSOR_FLOAT32", [1, 2]])
  new_fib = Internal("new_fib", ["TENSOR_FLOAT32", [1, 1]])
  model = Model()
  model.IdentifyInputs(fib, i, n)
  model.IdentifyOutputs(fib_out, i_out)
  model.Operation("ADD", i, [-2], 0).To(i_minus_2)
  model.Operation("MUL", i_minus_2, [0, 1], 0).To(slice_start)
  model.Operation("SLICE", fib, slice_start, slice_size).To(last2)
  model.Operation("FULLY_CONNECTED", last2, matrix, zero_bias, 0).To(new_fib)
  model.Operation("CONCATENATION", fib, new_fib, 1).To(fib_out)
  model.Operation("ADD", i, [1], 0).To(i_out)

  quant8 = DataTypeConverter().Identify({
      fib_out: Quant8Params,
      fib: Quant8Params,
      last2: Quant8Params,
      matrix: Quant8Params,
      new_fib: Quant8Params,
      zero_bias: ["TENSOR_INT32", 1.0, 0],
  })
  quant8_signed = DataTypeConverter().Identify({
      fib_out: Quant8SignedParams,
      fib: Quant8SignedParams,
      last2: Quant8SignedParams,
      matrix: Quant8SignedParams,
      new_fib: Quant8SignedParams,
      zero_bias: ["TENSOR_INT32", 1.0, 0],
  })

  return SubgraphReference("body", model), quant8, quant8_signed

def Test(n_data, fib_data, add_unused_output=False):
  n = Input("n", CounterType)
  fib_out = Output("fib_out", ["TENSOR_FLOAT32", [1, len(fib_data)]])  # Known dimensions.
  cond, cond_quant8, cond_quant8_signed = MakeConditionModel()
  body, body_quant8, body_quant8_signed = MakeBodyModel()
  fib_init = Parameter("fib_init", ["TENSOR_FLOAT32", [1, 2]], [1, 1])
  i_init = [2]
  outputs = [fib_out]
  if add_unused_output:
    i_out = Internal("i_out", CounterType)  # Unused.
    outputs.append(i_out)
  model = Model().Operation("WHILE", cond, body, fib_init, i_init, n).To(outputs)

  quant8 = DataTypeConverter().Identify({
      fib_init: Quant8Params,
      fib_out: Quant8Params,
      cond: cond_quant8,
      body: body_quant8,
  })
  quant8_signed = DataTypeConverter().Identify({
      fib_init: Quant8SignedParams,
      fib_out: Quant8SignedParams,
      cond: cond_quant8_signed,
      body: body_quant8_signed,
  })

  name = "n_{}".format(n_data)
  if add_unused_output:
    name += "_unused_output"
  example = Example({n: [n_data], fib_out: fib_data}, name=name)
  example.AddVariations("relaxed", "float16", quant8, quant8_signed)
  example.AddVariations(AllOutputsAsInternalCoverter())

Test(n_data=2, fib_data=[1, 1], add_unused_output=True)
Test(n_data=3, fib_data=[1, 1, 2], add_unused_output=True)
Test(n_data=4, fib_data=[1, 1, 2, 3], add_unused_output=True)
Test(n_data=5, fib_data=[1, 1, 2, 3, 5])
Test(n_data=6, fib_data=[1, 1, 2, 3, 5, 8])
