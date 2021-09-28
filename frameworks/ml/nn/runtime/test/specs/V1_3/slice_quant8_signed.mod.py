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

inp = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{3, 2, 3, 1}, 2.0, 0")
inp_data = [
    -127, -127, -127, -126, -126, -126, -125, -125, -125, -124, -124, -124,
    -123, -123, -123, -122, -122, -122
]
begin = Input("begin", "TENSOR_INT32", "{4}")
begin_data = [1, 0, 0, 0]
size = Input("size", "TENSOR_INT32", "{4}")
size_data = [2, 1, 3, 1]
output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 1, 3, 1}, 2.0, 0")
output_data = [-125, -125, -125, -123, -123, -123]

model = Model().Operation("SLICE", inp, begin, size).To(output)
Example(
    {
        inp: inp_data,
        begin: begin_data,
        size: size_data,
        output: output_data,
    },
    model=model)

# zero-sized input

# Use BOX_WITH_NMS_LIMIT op to generate a zero-sized internal tensor for box cooridnates.
p1 = Parameter("scores", "TENSOR_FLOAT32", "{1, 2}", [0.90, 0.10])  # scores
p2 = Parameter("roi", "TENSOR_FLOAT32", "{1, 8}",
               [1, 1, 10, 10, 0, 0, 10, 10])  # roi
o1 = Output("scoresOut", "TENSOR_FLOAT32", "{0}")  # scores out
o2 = Output("classesOut", "TENSOR_INT32", "{0}")  # classes out
tmp1 = Internal("roiOut", "TENSOR_FLOAT32", "{0, 4}")  # roi out
tmp2 = Internal("batchSplitOut", "TENSOR_INT32", "{0}")  # batch split out
model = Model("zero_sized").Operation("BOX_WITH_NMS_LIMIT", p1, p2, [0], 0.3,
                                      -1, 0, 0.4, 1.0,
                                      0.3).To(o1, tmp1, o2, tmp2)

# Use ROI_ALIGN op to convert into zero-sized feature map.
layout = BoolScalar("layout", False)  # NHWC
i1 = Input("in", "TENSOR_FLOAT32", "{1, 1, 1, 1}")
zero_sized = Internal("featureMap", "TENSOR_FLOAT32", "{0, 2, 2, 1}")
model = model.Operation("ROI_ALIGN", i1, tmp1, tmp2, 2, 2, 2.0, 2.0, 4, 4,
                        layout).To(zero_sized)

# SLICE op with numBatches = 0.
o3 = Output("out", "TENSOR_FLOAT32", "{0, 1, 1, 1}")  # out
model = model.Operation("SLICE", zero_sized, [0, 1, 1, 0],
                        [-1, 1, -1, 1]).To(o3)

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
