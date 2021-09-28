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
def test(name, input0, input1, output0, input0_data, input1_data, output_data):
  model = Model().Operation("GREATER", input0, input1).To(output0)
  example = Example({
      input0: input0_data,
      input1: input1_data,
      output0: output_data,
  }, model=model, name=name)

test(
    name="quantized_different_scale",
    input0=Input("input0", ("TENSOR_QUANT8_ASYMM_SIGNED", [3], 1.0, 0)),
    input1=Input("input1", ("TENSOR_QUANT8_ASYMM_SIGNED", [1], 2.0, 0)),
    output0=Output("output0", "TENSOR_BOOL8", "{3}"),
    input0_data=[1, 2, 3], # effectively 1, 2, 3
    input1_data=[1],       # effectively 2
    output_data=[False, False, True],
)

test(
    name="quantized_different_zero_point",
    input0=Input("input0", ("TENSOR_QUANT8_ASYMM_SIGNED", [3], 1.0, 0)),
    input1=Input("input1", ("TENSOR_QUANT8_ASYMM_SIGNED", [1], 1.0, 1)),
    output0=Output("output0", "TENSOR_BOOL8", "{3}"),
    input0_data=[1, 2, 3], # effectively 1, 2, 3
    input1_data=[3],       # effectively 2
    output_data=[False, False, True],
)

test(
    name="quantized_overflow_second_input_if_requantized",
    input0=Input("input0", ("TENSOR_QUANT8_ASYMM_SIGNED", [1], 1.64771, -97)),
    input1=Input("input1", ("TENSOR_QUANT8_ASYMM_SIGNED", [1], 1.49725, 112)),
    output0=Output("output0", "TENSOR_BOOL8", "{1}"),
    input0_data=[-128],
    input1_data=[72],
    output_data=[True],
)

test(
    name="quantized_overflow_first_input_if_requantized",
    input0=Input("input0", ("TENSOR_QUANT8_ASYMM_SIGNED", [1], 1.49725, 112)),
    input1=Input("input1", ("TENSOR_QUANT8_ASYMM_SIGNED", [1], 1.64771, -97)),
    output0=Output("output0", "TENSOR_BOOL8", "{1}"),
    input0_data=[72],
    input1_data=[-128],
    output_data=[False],
)
