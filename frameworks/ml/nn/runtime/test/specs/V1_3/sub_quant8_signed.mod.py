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

import itertools
import random

def dequantize(x, scale, offset):
  return (x - offset) * scale

def quantize(x, scale, offset):
  return max(-128, min(127, int(round(x / scale)) + offset))

def create_test(input0_scale, input0_offset,
                input1_scale, input1_offset,
                output_scale, output_offset):
  def sub_quantized(a, b):
    a_dequantized = dequantize(a, input0_scale, input0_offset)
    b_dequantized = dequantize(b, input1_scale, input1_offset)
    return quantize(a_dequantized - b_dequantized, output_scale, output_offset)

  values = [-128, -127, -126, -125, -124, -123, 122, 123, 124, 125, 126, 127]
  inputs = list(itertools.product(values, values))
  input0_values, input1_values = zip(*inputs)
  output_values = [sub_quantized(a, b) for a, b in inputs]
  size = len(output_values)
  input0 = Input("input0", "TENSOR_QUANT8_ASYMM_SIGNED",
                 "{%d}, %g, %d" % (size, input0_scale, input0_offset))
  input1 = Input("input1", "TENSOR_QUANT8_ASYMM_SIGNED",
                 "{%d}, %g, %d" % (size, input1_scale, input1_offset))
  activation = 0
  output0 = Output("output0", "TENSOR_QUANT8_ASYMM_SIGNED",
                   "{%d}, %g, %d" % (size, output_scale, output_offset))
  model = Model().Operation("SUB", input0, input1, activation).To(output0)
  Example({
      input0: input0_values,
      input1: input1_values,
      output0: output_values,
  })

scales_and_offsets = [(1.0, -128),
                      (1.0, -127),
                      (0.01, -8),
                      (10.0, -8)]
for params in itertools.product(scales_and_offsets,
                                scales_and_offsets,
                                scales_and_offsets):
  input0_params, input1_params, output_params = params
  create_test(*input0_params, *input1_params, *output_params)

#######################################################

input0 = Input("input0", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 2}, 1.0, -128")
input1 = Input("input1", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 2}, 1.0, -128")
activation = 0
output0 = Output("output0", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 2}, 1.0, -128")

model = Model("quant8").Operation("SUB", input0, input1, activation).To(output0)

input0_values = [-28, 72]
input1_values = [-127, -126,
                 -125, -124]
output_values = [-29, 70,
                 -31, 68]

Example({
    input0: input0_values,
    input1: input1_values,
    output0: output_values,
})

#######################################################

shape = "{2, 4, 16, 2}, 0.5, -128"
input0 = Input("input0", "TENSOR_QUANT8_ASYMM_SIGNED", shape)
input1 = Input("input1", "TENSOR_QUANT8_ASYMM_SIGNED", shape)
activation = 0
output0 = Output("output0", "TENSOR_QUANT8_ASYMM_SIGNED", shape)

model = Model("quant8").Operation("SUB", input0, input1, activation).To(output0)

input0_values = list(range(-128, 128))
input1_values = list(input0_values)
random.seed(0)
random.shuffle(input1_values)
output_values = [max(-128, (a - b) - 128) for a, b in zip(input0_values, input1_values)]

Example({
    input0: input0_values,
    input1: input1_values,
    output0: output_values,
})

#######################################################
# SUB, zero-sized input

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
i1 = Input("in", "TENSOR_FLOAT32", "{1, 1, 1, 2}")
zero_sized = Internal("featureMap", "TENSOR_FLOAT32", "{0, 2, 2, 2}")
model = model.Operation("ROI_ALIGN", i1, tmp1, tmp2, 2, 2, 2.0, 2.0, 4, 4, layout).To(zero_sized)

# SUB op with numBatches = 0.
i2 = Parameter("op", "TENSOR_FLOAT32", "{1, 2, 2, 1}", [1, 2, 3, 4]) # weights
o3 = Output("out", "TENSOR_FLOAT32", "{0, 2, 2, 2}") # out
model = model.Operation("SUB", zero_sized, i2, 0).To(o3)

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
