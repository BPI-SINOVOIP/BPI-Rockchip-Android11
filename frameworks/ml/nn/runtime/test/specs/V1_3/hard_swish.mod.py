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


def test(name, input0, output0, input0_data, output0_data):
  model = Model().Operation("HARD_SWISH", input0).To(output0)
  quant8 = DataTypeConverter().Identify({
      input0: ["TENSOR_QUANT8_ASYMM", 0.078125, 128],
      output0: ["TENSOR_QUANT8_ASYMM", 0.078125, 128],
  })
  quant8_signed = DataTypeConverter().Identify({
      input0: ["TENSOR_QUANT8_ASYMM_SIGNED", 0.078125, 0],
      output0: ["TENSOR_QUANT8_ASYMM_SIGNED", 0.078125, 0],
  })
  quant8_overflow = DataTypeConverter().Identify({
      input0: ["TENSOR_QUANT8_ASYMM", 0.078125, 128],
      output0: ["TENSOR_QUANT8_ASYMM", 0.03125, 0],
  })
  quant8_signed_overflow = DataTypeConverter().Identify({
      input0: ["TENSOR_QUANT8_ASYMM_SIGNED", 0.078125, 0],
      output0: ["TENSOR_QUANT8_ASYMM_SIGNED", 0.03125, -128],
  })
  example = Example({
      input0: input0_data,
      output0: output0_data,
  },
                    model=model,
                    name=name).AddVariations("float16", "relaxed", quant8,
                                             quant8_signed, quant8_overflow,
                                             quant8_signed_overflow)


test(
    name="simple",
    input0=Input("input0", "TENSOR_FLOAT32", "{40}"),
    output0=Output("output0", "TENSOR_FLOAT32", "{40}"),
    input0_data=[
        4.53125, 3.90625, 3.046875, -8.59375, -1.328125, 1.328125, 0.0,
        -8.515625, -8.984375, -0.234375, 0.859375, 9.84375, -0.15625, -8.515625,
        8.671875, 4.609375, 9.21875, -1.796875, 1.171875, 9.375, -8.75,
        2.421875, -8.125, -1.09375, -9.609375, -1.015625, -9.84375, 2.578125,
        4.921875, -5.078125, 5.0, -0.859375, 1.953125, -6.640625, -7.8125,
        4.453125, -4.453125, -6.875, 0.78125, 0.859375
    ],
    output0_data=[
        4.53125, 3.90625, 3.046875, 0.0, -0.3700765, 0.9580485, 0.0, 0.0, 0.0,
        -0.1080322, 0.5527751, 9.84375, -0.074056, 0.0, 8.671875, 4.609375,
        9.21875, -0.3603109, 0.8148193, 9.375, 0.0, 2.1885173, 0.0, -0.3474935,
        0.0, -0.3358968, 0.0, 2.3968506, 4.921875, 0.0, 5.0, -0.3065999,
        1.6123454, 0.0, 0.0, 4.453125, 0.0, 0.0, 0.4923503, 0.5527751
    ],
)

test(
    name="5d_input",
    input0=Input("input0", "TENSOR_FLOAT32", "{1, 2, 2, 2, 5}"),
    output0=Output("output0", "TENSOR_FLOAT32", "{1, 2, 2, 2, 5}"),
    input0_data=[
        4.53125, 3.90625, 3.046875, -8.59375, -1.328125, 1.328125, 0.0,
        -8.515625, -8.984375, -0.234375, 0.859375, 9.84375, -0.15625, -8.515625,
        8.671875, 4.609375, 9.21875, -1.796875, 1.171875, 9.375, -8.75,
        2.421875, -8.125, -1.09375, -9.609375, -1.015625, -9.84375, 2.578125,
        4.921875, -5.078125, 5.0, -0.859375, 1.953125, -6.640625, -7.8125,
        4.453125, -4.453125, -6.875, 0.78125, 0.859375
    ],
    output0_data=[
        4.53125, 3.90625, 3.046875, 0.0, -0.3700765, 0.9580485, 0.0, 0.0, 0.0,
        -0.1080322, 0.5527751, 9.84375, -0.074056, 0.0, 8.671875, 4.609375,
        9.21875, -0.3603109, 0.8148193, 9.375, 0.0, 2.1885173, 0.0, -0.3474935,
        0.0, -0.3358968, 0.0, 2.3968506, 4.921875, 0.0, 5.0, -0.3065999,
        1.6123454, 0.0, 0.0, 4.453125, 0.0, 0.0, 0.4923503, 0.5527751
    ],
)
