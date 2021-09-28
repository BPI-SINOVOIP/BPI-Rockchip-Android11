#
# Copyright (C) 2019 The Android Open Source Project
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

import collections

Operand = collections.namedtuple(
    "Operand", [
        "name", "as_input", "as_output", "data",
        "supports_relaxation", "introduced_in"
    ],
    defaults=[False, None])
operands = [
    Operand(
        name="float16",
        as_input=Input("input0", "TENSOR_FLOAT16", "{2, 3}"),
        as_output=Output("output0", "TENSOR_FLOAT16", "{2, 3}"),
        data=[1, 2, 3, 4, 5, 6],
        supports_relaxation=False,
        introduced_in="V1_2"),
    Operand(
        name="float32",
        as_input=Input("input0", "TENSOR_FLOAT32", "{2, 3}"),
        as_output=Output("output0", "TENSOR_FLOAT32", "{2, 3}"),
        data=[1, 2, 3, 4, 5, 6],
        supports_relaxation=True,
        introduced_in="V1_2"),
    Operand(
        name="int32",
        as_input=Input("input0", "TENSOR_INT32", "{2, 3}"),
        as_output=Output("output0", "TENSOR_INT32", "{2, 3}"),
        data=[1, 2, 3, 4, 5, 6],
        supports_relaxation=False,
        introduced_in="V1_2"),
    Operand(
        name="bool8",
        as_input=Input("input0", "TENSOR_BOOL8", "{2, 3}"),
        as_output=Output("output0", "TENSOR_BOOL8", "{2, 3}"),
        data=[1, 0, 1, 0, 0, 1]),
    Operand(
        name="quant8_asymm",
        as_input=Input("input0", "TENSOR_QUANT8_ASYMM", "{2, 3}, 4.0, 100"),
        as_output=Output("output0", "TENSOR_QUANT8_ASYMM", "{2, 3}, 4.0, 100"),
        data=[1, 2, 3, 4, 5, 6],
        supports_relaxation=False,
        introduced_in="V1_2"),
    Operand(
        name="quant8_symm",
        as_input=Input("input0", "TENSOR_QUANT8_SYMM", "{2, 3}, 4.0, 0"),
        as_output=Output("output0", "TENSOR_QUANT8_SYMM", "{2, 3}, 4.0, 0"),
        data=[1, 2, 3, 4, 5, 6]),
    Operand(
        name="quant16_asymm",
        as_input=Input("input0", "TENSOR_QUANT16_ASYMM", "{2, 3}, 4.0, 100"),
        as_output=Output("output0", "TENSOR_QUANT16_ASYMM", "{2, 3}, 4.0, 100"),
        data=[1, 2, 3, 4, 5, 6]),
    Operand(
        name="quant16_symm",
        as_input=Input("input0", "TENSOR_QUANT16_SYMM", "{2, 3}, 4.0, 0"),
        as_output=Output("output0", "TENSOR_QUANT16_SYMM", "{2, 3}, 4.0, 0"),
        data=[1, 2, 3, 4, 5, 6]),
]

for operand in operands:
  input0 = operand.as_input
  output0 = operand.as_output

  model = Model().Operation("CAST", input0).To(output0)

  if operand.introduced_in:
    model.IntroducedIn(operand.introduced_in)

  example = Example({
      input0: operand.data,
      output0: operand.data,
  }, model=model, name=operand.name)

  if operand.supports_relaxation:
    example.AddRelaxed()
