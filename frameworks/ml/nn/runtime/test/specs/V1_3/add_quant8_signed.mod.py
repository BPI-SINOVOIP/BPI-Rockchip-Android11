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
i1 = Input("op1", "TENSOR_QUANT8_ASYMM_SIGNED", "{2}, 2.0, 0")
i2 = Input("op2", "TENSOR_QUANT8_ASYMM_SIGNED", "{2}, 1.0, 0")
act = Int32Scalar("act", 0)
i3 = Output("op3", "TENSOR_QUANT8_ASYMM_SIGNED", "{2}, 1.0, 0")
model = model.Operation("ADD", i1, i2, act).To(i3)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [1, 2],
          i2: # input 1
          [3, 4]}

output0 = {i3: # output 0
           [5, 8]}

# Instantiate an example
Example((input0, output0))

#######################################################

model = Model()
i1 = Input("op1", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 2}, 2.0, 0")
i2 = Input("op2", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 2}, 1.0, 0")
act = Int32Scalar("act", 0)
i3 = Output("op3", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 2}, 1.0, 0")
model = model.Operation("ADD", i1, i2, act).To(i3)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [1, 2],
          i2: # input 1
          [1, 2, 3, 4]}

output0 = {i3: # output 0
           [3, 6, 5, 8]}

# Instantiate an example
Example((input0, output0))

#######################################################

# Zero-sized input test
# Use BOX_WITH_NMS_LIMIT op to generate a zero-sized internal tensor for box cooridnates.
p1 = Parameter("scores", "TENSOR_FLOAT32", "{1, 2}", [0.90, 0.10]) # scores
p2 = Parameter("roi", "TENSOR_FLOAT32", "{1, 8}", [1, 1, 10, 10, 0, 0, 10, 10]) # roi
o1 = Output("scoresOut", "TENSOR_FLOAT32", "{0}") # scores out
o2 = Output("classesOut", "TENSOR_INT32", "{0}") # classes out
tmp1 = Internal("roiOut", "TENSOR_FLOAT32", "{0, 4}") # roi out
tmp2 = Internal("batchSplitOut", "TENSOR_INT32", "{0}") # batch split out
model = Model("zero_sized").Operation("BOX_WITH_NMS_LIMIT", p1, p2, [0], 0.3, -1, 0, 0.4, 1.0, 0.3).To(o1, tmp1, o2, tmp2)

# Use ROI_ALIGN op to convert into zero-sized feature map.
layout = BoolScalar("layout", False) # NHWC
i1 = Input("in", "TENSOR_FLOAT32", "{1, 1, 1, 2}")
zero_sized = Internal("featureMap", "TENSOR_FLOAT32", "{0, 2, 2, 2}")
model = model.Operation("ROI_ALIGN", i1, tmp1, tmp2, 2, 2, 2.0, 2.0, 4, 4, layout).To(zero_sized)

# ADD op with numBatches = 0.
i2 = Parameter("op", "TENSOR_FLOAT32", "{1, 2, 2, 1}", [1, 2, 3, 4]) # weights
o3 = Output("out", "TENSOR_FLOAT32", "{0, 2, 2, 2}") # out
model = model.Operation("ADD", zero_sized, i2, 0).To(o3)

quant8_signed = DataTypeConverter().Identify({
    p1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0),
    p2: ("TENSOR_QUANT16_ASYMM", 0.125, 0),
    o1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0),
    tmp1: ("TENSOR_QUANT16_ASYMM", 0.125, 0),
    i1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0),
    zero_sized: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0),
    i2: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0),
    o3: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0)
})

Example({
    i1: [1, 2],
    o1: [],
    o2: [],
    o3: [],
}).AddVariations(quant8_signed, includeDefault=False)
