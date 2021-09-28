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
i1 = Input("op1", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 2, 2, 1}, 0.5f, -128") # input 0
o = Output("op2", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 2, 2, 1}, 0.5f, -128") # output 0
model = model.Operation("RELU1", i1).To(o)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [-128, -127, -126, -125]}
output0 = {o: # output 0
          [-128, -127, -126, -126]}

# Instantiate one example
Example((input0, output0))

#######################################################

# Example 2. Input in operand 0,
input1 = {i1: # input 0
          [-124, -118, -28, 127]}
output1 = {o: # output 0
          [-126, -126, -126, -126]}

# Instantiate another example
Example((input1, output1))

#######################################################

model = Model()

d0 = 2
d1 = 64
d2 = 64
d3 = 2

i0 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{%d, %d, %d, %d}, 1.f, 0" % (d0, d1, d2, d3))

output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{%d, %d, %d, %d}, 1.f, 0" % (d0, d1, d2, d3))

model = model.Operation("RELU1", i0).To(output)

# Example 1. Input in operand 0,
rng = d0 * d1 * d2 * d3
input_values = (lambda r = rng: [x % 256 for x in range(r)])()
output_values = [127 if x < 127 else 129 if x > 129 else x for x in input_values]

input0 = {i0: [value - 128 for value in input_values]}
output0 = {output: [value - 128 for value in output_values]}

# Instantiate an example
Example((input0, output0))

#######################################################
# Use BOX_WITH_NMS_LIMIT op to generate a zero-sized internal tensor for box cooridnates.

p1 = Parameter("scores", "TENSOR_FLOAT32", "{1, 2}", [0.90, 0.10]) # scores
p2 = Parameter("roi", "TENSOR_FLOAT32", "{1, 8}", [1, 1, 10, 10, 0, 0, 10, 10]) # roi
o1 = Output("scoresOut", "TENSOR_FLOAT32", "{0}") # scores out
o2 = Output("classesOut", "TENSOR_INT32", "{0}") # classes out
tmp1 = Internal("roiOut", "TENSOR_FLOAT32", "{0, 4}") # roi out
tmp2 = Internal("batchSplitOut", "TENSOR_INT32", "{0}") # batch split out
model = Model("zero_sized").Operation("BOX_WITH_NMS_LIMIT", p1, p2, [0], 0.3,  -1, 0, 0.4, 1.0, 0.3).To(o1, tmp1, o2, tmp2)

# Use ROI_ALIGN op to convert into zero-sized feature map.
layout = BoolScalar("layout", False) # NHWC
i1 = Input("in", "TENSOR_FLOAT32", "{1, 1, 1, 1}")
zero_sized = Internal("featureMap", "TENSOR_FLOAT32", "{0, 2, 2, 1}")
model = model.Operation("ROI_ALIGN", i1, tmp1, tmp2, 2, 2, 2.0, 2.0, 4, 4, layout).To(zero_sized)

# RELU1 op with numBatches = 0.
o3 = Output("out", "TENSOR_FLOAT32", "{0, 2, 2, 1}") # out
model = model.Operation("RELU1", zero_sized).To(o3)

quant8_signed = DataTypeConverter().Identify({
    p1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0),
    p2: ("TENSOR_QUANT16_ASYMM", 0.125, 0),
    o1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0),
    tmp1: ("TENSOR_QUANT16_ASYMM", 0.125, 0),
    i1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0),
    zero_sized: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0),
    o3: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0)
})

Example({
    i1: [1],
    o1: [],
    o2: [],
    o3: [],
}).AddVariations(quant8_signed, includeDefault=False)
