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
  model = Model().Operation("MAXIMUM", input0, input1).To(output0)

  quant8_signed = DataTypeConverter().Identify({
      input0: ["TENSOR_QUANT8_ASYMM_SIGNED", 0.5, -1],
      input1: ["TENSOR_QUANT8_ASYMM_SIGNED", 1.0, -28],
      output0: ["TENSOR_QUANT8_ASYMM_SIGNED", 2.0, -48],
  })

  Example({
      input0: input0_data,
      input1: input1_data,
      output0: output_data,
  }, model=model, name=name).AddVariations(quant8_signed, includeDefault=False)


test(
    name="simple",
    input0=Input("input0", "TENSOR_FLOAT32", "{3, 1, 2}"),
    input1=Input("input1", "TENSOR_FLOAT32", "{3, 1, 2}"),
    output0=Output("output0", "TENSOR_FLOAT32", "{3, 1, 2}"),
    input0_data=[1.0, 0.0, -1.0, 11.0, -2.0, -1.44],
    input1_data=[-1.0, 0.0, 1.0, 12.0, -3.0, -1.43],
    output_data=[1.0, 0.0, 1.0, 12.0, -2.0, -1.43],
)

test(
    name="broadcast",
    input0=Input("input0", "TENSOR_FLOAT32", "{3, 1, 2}"),
    input1=Input("input1", "TENSOR_FLOAT32", "{2}"),
    output0=Output("output0", "TENSOR_FLOAT32", "{3, 1, 2}"),
    input0_data=[1.0, 0.0, -1.0, -2.0, -1.44, 11.0],
    input1_data=[0.5, 2.0],
    output_data=[1.0, 2.0, 0.5, 2.0, 0.5, 11.0],
)


# Test overflow and underflow.
input0 = Input("input0", "TENSOR_QUANT8_ASYMM_SIGNED", "{2}, 1.0f, 0")
input1 = Input("input1", "TENSOR_QUANT8_ASYMM_SIGNED", "{2}, 1.0f, 0")
output0 = Output("output0", "TENSOR_QUANT8_ASYMM_SIGNED", "{2}, 0.5f, 0")
model = Model().Operation("MAXIMUM", input0, input1).To(output0)

Example({
    input0: [-68, 0],
    input1: [0, 72],
    output0: [0, 127],
}, model=model, name="overflow")
