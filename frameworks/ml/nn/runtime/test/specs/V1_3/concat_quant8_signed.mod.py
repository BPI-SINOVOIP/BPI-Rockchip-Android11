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

# Adapted from tensorflow/lite/kernels/concatenation_test.cc

input0 = Input("input0", "TENSOR_FLOAT32", "{2, 1, 2}")
input1 = Input("input1", "TENSOR_FLOAT32", "{2, 1, 2}")
input2 = Input("input2", "TENSOR_FLOAT32", "{2, 1, 2}")
input3 = Input("input3", "TENSOR_FLOAT32", "{2, 1, 2}")
axis = 2
output0 = Output("output0", "TENSOR_FLOAT32", "{2, 1, 8}")

model = Model().Operation("CONCATENATION", input0, input1, input2, input3, axis).To(output0)

# FourInputsQuantizedMixedRange
Example({
    input0: [1.0, -3.0, -4.0, -7.0],
    input1: [1.1, 3.1, 4.1, 7.1],
    input2: [1.2, -3.2, -4.2, 7.2],
    input3: [1.3, 3.3, 4.3, 7.3],
    output0: [1.0, -3.0, 1.1, 3.1, 1.2, -3.2, 1.3, 3.3, -4.0, -7.0, 4.1, 7.1, -4.2, 7.2, 4.3, 7.3],
}).AddVariations(DataTypeConverter().Identify({
    input0: ["TENSOR_QUANT8_ASYMM_SIGNED", 0.084, -1],
    input1: ["TENSOR_QUANT8_ASYMM_SIGNED", 0.05, -128],
    input2: ["TENSOR_QUANT8_ASYMM_SIGNED", 0.089, -5],
    input3: ["TENSOR_QUANT8_ASYMM_SIGNED", 0.029, -128],
    output0: ["TENSOR_QUANT8_ASYMM_SIGNED", 0.1, -1],
}), includeDefault=False)

# FourInputsQuantizedMixedRangeClampingLogic
Example({
    input0: [1.0, -3.0, -4.0, -7.0],
    input1: [1.1, 3.1, 4.1, 7.1],
    input2: [1.2, -3.2, -4.2, 7.2],
    input3: [1.3, 3.3, 4.3, 7.3],
    output0: [1.0, -1.0, 1.0, 1.0, 1.0, -1.0, 1.0, 1.0, -1.0, -1.0, 1.0, 1.0, -1.0, 1.0, 1.0, 1.0]
}).AddVariations(DataTypeConverter().Identify({
    input0: ["TENSOR_QUANT8_ASYMM_SIGNED", 0.084, -1],
    input1: ["TENSOR_QUANT8_ASYMM_SIGNED", 0.05, -128],
    input2: ["TENSOR_QUANT8_ASYMM_SIGNED", 0.089, -5],
    input3: ["TENSOR_QUANT8_ASYMM_SIGNED", 0.029, -128],
    output0: ["TENSOR_QUANT8_ASYMM_SIGNED", 0.0078125, -1],
}), includeDefault=False)

#######################################################

model = Model()
i1 = Input("op1", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 3}, 0.5f, -128") # input 0
i2 = Input("op2", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 3}, 0.5f, -128") # input 1
axis1 = Int32Scalar("axis1", 1)
r = Output("result", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 6}, 0.5f, -128") # output
model = model.Operation("CONCATENATION", i1, i2, axis1).To(r)

# Example 1.
input0 = {i1: [1, 2, 3, 4, 5, 6],
          i2: [7, 8, 9, 10, 11, 12]}
output0 = {r: [1, 2, 3, 7, 8, 9, 4, 5, 6, 10, 11, 12]}

# Instantiate an example
Example((input0, output0))

#######################################################

model = Model()

row1 = 52
row2 = 40
col = 300
output_row = row1 + row2

input1 = Input("input1", "TENSOR_QUANT8_ASYMM_SIGNED", "{%d, %d}, 0.5f, -128" % (row1, col))
input2 = Input("input2", "TENSOR_QUANT8_ASYMM_SIGNED", "{%d, %d}, 0.5f, -128" % (row2, col))
axis0 = Int32Scalar("axis0", 0)
output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{%d, %d}, 0.5f, -128" % (output_row, col))
model = model.Operation("CONCATENATION", input1, input2, axis0).To(output)

# Example 1.
input1_values = [x % 256 for x in range(row1 * col)]
input2_values = (lambda s1 = row1 * col, s2 = row2 * col:
                 [(x + s1) % 256 for x in range(s2)])()
input0 = {input1: [x - 128 for x in input1_values],
          input2: [x - 128 for x in input2_values]}
output_values = [x % 256 - 128 for x in range(output_row * col)]
output0 = {output: output_values}

# Instantiate an example
Example((input0, output0))

#######################################################

model = Model()

row = 400
col1 = 60
col2 = 30
output_col = col1 + col2

input1 = Input("input1", "TENSOR_QUANT8_ASYMM_SIGNED", "{%d, %d}, 0.5f, -128" % (row, col1))
input2 = Input("input2", "TENSOR_QUANT8_ASYMM_SIGNED", "{%d, %d}, 0.5f, -128" % (row, col2))
axis1 = Int32Scalar("axis1", 1)
output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{%d, %d}, 0.5f, -128" % (row, output_col))
model = model.Operation("CONCATENATION", input1, input2, axis1).To(output)

# Example 1.
input1_values = [(x % 128) for x in range(row * col1)]
input2_values = [x % 128 - 128 for x in range(row * col2)]
input0 = {input1: input1_values,
          input2: input2_values}

output_values = [x for x in range(row * output_col)]
for r in range(row):
  for c1 in range(col1):
    output_values[r * output_col + c1] = input1_values[r * col1 + c1]
  for c2 in range(col2):
    output_values[r * output_col + col1 + c2] = input2_values[r * col2 + c2]

output0 = {output: output_values}

# Instantiate an example
Example((input0, output0))

#######################################################

# Zero-sized input: zero dimension is not "axis"

# Use BOX_WITH_NMS_LIMIT op to generate a zero-sized internal tensor for box cooridnates.
p1 = Parameter("scores", "TENSOR_FLOAT32", "{1, 2}", [0.90, 0.10]) # scores
p2 = Parameter("roi", "TENSOR_FLOAT32", "{1, 8}", [1, 1, 10, 10, 0, 0, 10, 10]) # roi
o1 = Output("scoresOut", "TENSOR_FLOAT32", "{0}") # scores out
o2 = Output("classesOut", "TENSOR_INT32", "{0}") # classes out
tmp1 = Internal("roiOut", "TENSOR_FLOAT32", "{0, 4}") # roi out
tmp2 = Internal("batchSplitOut", "TENSOR_INT32", "{0}") # batch split out
model = Model().Operation("BOX_WITH_NMS_LIMIT", p1, p2, [0], 0.3, -1, 0, 0.4, 1.0, 0.3).To(o1, tmp1, o2, tmp2)

# Use ROI_ALIGN op to convert into zero-sized feature map.
layout = BoolScalar("layout", False) # NHWC
i1 = Input("in", "TENSOR_FLOAT32", "{1, 1, 1, 1}")
zero_sized = Internal("featureMap", "TENSOR_FLOAT32", "{0, 2, 2, 1}")
model = model.Operation("ROI_ALIGN", i1, tmp1, tmp2, 2, 2, 2.0, 2.0, 4, 4, layout).To(zero_sized)

# CONCATENATION op with numBatches = 0.
o3 = Output("out", "TENSOR_FLOAT32", "{0, 2, 2, 2}") # out
model = model.Operation("CONCATENATION", zero_sized, zero_sized, 3).To(o3)

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

#######################################################

# Zero-sized input: zero dimension is "axis"

# Use BOX_WITH_NMS_LIMIT op to generate a zero-sized internal tensor for box cooridnates.
p1 = Parameter("scores", "TENSOR_FLOAT32", "{1, 2}", [0.90, 0.10]) # scores
p2 = Parameter("roi", "TENSOR_FLOAT32", "{1, 8}", [1, 1, 10, 10, 0, 0, 10, 10]) # roi
o1 = Output("scoresOut", "TENSOR_FLOAT32", "{0}") # scores out
o2 = Output("classesOut", "TENSOR_INT32", "{0}") # classes out
tmp1 = Internal("roiOut", "TENSOR_FLOAT32", "{0, 4}") # roi out
tmp2 = Internal("batchSplitOut", "TENSOR_INT32", "{0}") # batch split out
model = Model().Operation("BOX_WITH_NMS_LIMIT", p1, p2, [0], 0.3, -1, 0, 0.4, 1.0, 0.3).To(o1, tmp1, o2, tmp2)

# Use ROI_ALIGN op to convert into zero-sized feature map.
layout = BoolScalar("layout", False) # NHWC
i1 = Input("in", "TENSOR_FLOAT32", "{1, 1, 1, 1}")
zero_sized = Internal("featureMap", "TENSOR_FLOAT32", "{0, 2, 2, 1}")
model = model.Operation("ROI_ALIGN", i1, tmp1, tmp2, 2, 2, 2.0, 2.0, 4, 4, layout).To(zero_sized)

# CONCATENATION op with numBatches = 0.
i2 = Input("in", "TENSOR_FLOAT32", "{1, 2, 2, 1}")
o3 = Output("out", "TENSOR_FLOAT32", "{1, 2, 2, 1}") # out
model = model.Operation("CONCATENATION", zero_sized, i2, 0).To(o3)

quant8_signed = DataTypeConverter().Identify({
    p1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0),
    p2: ("TENSOR_QUANT16_ASYMM", 0.125, 0),
    o1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0),
    tmp1: ("TENSOR_QUANT16_ASYMM", 0.125, 0),
    i1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0),
    zero_sized: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0),
    i2: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.2, 0),
    o3: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0)
})

Example({
    i1: [1],
    i2: [1, 2, 3, 4],
    o1: [],
    o2: [],
    o3: [1, 2, 3, 4],
}).AddVariations(quant8_signed, includeDefault=False)
