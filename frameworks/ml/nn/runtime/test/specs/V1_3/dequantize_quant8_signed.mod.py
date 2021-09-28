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

model = Model()
i1 = Input("op1",  "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 2, 2, 1}, 1.f, -128")
i2 = Output("op2", "TENSOR_FLOAT32", "{1, 2, 2, 1}")
model = model.Operation("DEQUANTIZE", i1).To(i2)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [-128, -96, 0, 127]}

output0 = {i2: # output 0
           [0.0, 32.0, 128.0, 255.0]}

# Instantiate an example
Example((input0, output0))

#######################################################

def test(name, input0, output0, input0_data, output0_data):
  model = Model().Operation("DEQUANTIZE", input0).To(output0)
  example = Example({
      input0: input0_data,
      output0: output0_data,
  },
                    model=model,
                    name=name).AddVariations("relaxed", "float16")


test(
    name="1d_quant8_asymm",
    input0=Input("input0", "TENSOR_QUANT8_ASYMM_SIGNED", "{10}, 0.5, -1"),
    output0=Output("output0", "TENSOR_FLOAT32", "{10}"),
    input0_data=[-128, -127, -126, -125, -124, 123, 124, 125, 126, 127],
    output0_data=[-63.5, -63, -62.5, -62, -61.5, 62, 62.5, 63, 63.5, 64],
)

test(
    name="2d_quant8_asymm",
    input0=Input("input0", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 5}, 0.5, -1"),
    output0=Output("output0", "TENSOR_FLOAT32", "{2, 5}"),
    input0_data=[-128, -127, -126, -125, -124, 123, 124, 125, 126, 127],
    output0_data=[-63.5, -63, -62.5, -62, -61.5, 62, 62.5, 63, 63.5, 64],
)

# FLOAT16
model = Model()
i1 = Input("op1",  "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 2, 2, 1}, 1.f, -128")
i2 = Output("op2", "TENSOR_FLOAT16", "{1, 2, 2, 1}")
model = model.Operation("DEQUANTIZE", i1).To(i2)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [-128, -96, 0, 127]}

output0 = {i2: # output 0
           [0.0, 32.0, 128.0, 255.0]}

# Instantiate an example
Example((input0, output0))

#######################################################

# Zero-sized input

# Use BOX_WITH_NMS_LIMIT op to generate a zero-sized internal tensor for box cooridnates.
p1 = Parameter("scores", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 2}, 0.1f, 0", [9, 1]) # scores
p2 = Parameter("roi", "TENSOR_QUANT16_ASYMM", "{1, 8}, 0.125f, 0", [8, 8, 80, 80, 0, 0, 80, 80]) # roi
o1 = Output("scoresOut", "TENSOR_QUANT8_ASYMM_SIGNED", "{0}, 0.1f, 0") # scores out
o2 = Output("classesOut", "TENSOR_INT32", "{0}") # classes out
tmp1 = Internal("roiOut", "TENSOR_QUANT16_ASYMM", "{0, 4}, 0.125f, 0") # roi out
tmp2 = Internal("batchSplitOut", "TENSOR_INT32", "{0}") # batch split out
model = Model("zero_sized").Operation("BOX_WITH_NMS_LIMIT", p1, p2, [0], 0.3, -1, 0, 0.4, 1.0, 0.3).To(o1, tmp1, o2, tmp2)

# Use ROI_ALIGN op to convert into zero-sized feature map.
layout = BoolScalar("layout", False) # NHWC
i1 = Input("in", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 1, 1, 1}, 0.1f, 0")
zero_sized = Internal("featureMap", "TENSOR_QUANT8_ASYMM_SIGNED", "{0, 2, 2, 1}, 0.1f, 0")
model = model.Operation("ROI_ALIGN", i1, tmp1, tmp2, 2, 2, 2.0, 2.0, 4, 4, layout).To(zero_sized)

# DEQUANTIZE op with numBatches = 0.
o3 = Output("out", "TENSOR_FLOAT32", "{0, 2, 2, 1}") # out
model = model.Operation("DEQUANTIZE", zero_sized).To(o3)

float16 = DataTypeConverter().Identify({o3: ("TENSOR_FLOAT16",)})

Example({
    i1: [-127],
    o1: [],
    o2: [],
    o3: [],
}).AddVariations("relaxed", float16)
