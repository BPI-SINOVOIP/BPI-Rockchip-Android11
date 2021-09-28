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
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{4, 2, 2, 1}, 1.0, 0")
block = Parameter("block_size", "TENSOR_INT32", "{2}", [2, 2])
output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 4, 4, 1}, 1.0, 0")

model = model.Operation("BATCH_TO_SPACE_ND", i1, block).To(output)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]}

output0 = {output: # output 0
           [1, 5, 2, 6, 9, 13, 10, 14, 3, 7, 4, 8, 11, 15, 12, 16]}

# Instantiate an example
Example((input0, output0))#

layout = BoolScalar("layout", False) # NHWC

# TEST 1: BATCH_TO_SPACE_NCHW_1, block_size = [2, 2]
i1 = Input("op1", "TENSOR_FLOAT32", "{4, 1, 1, 2}")
o1 = Output("op4", "TENSOR_FLOAT32", "{1, 2, 2, 2}")
Model().Operation("BATCH_TO_SPACE_ND", i1, [2, 2], layout).To(o1)

# Additional data type
quant8_signed = DataTypeConverter().Identify({
    i1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0),
    o1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0)
})

# Instantiate an example
example = Example({
    i1: [1.4, 2.3, 3.2, 4.1, 5.4, 6.3, 7.2, 8.1],
    o1: [1.4, 2.3, 3.2, 4.1, 5.4, 6.3, 7.2, 8.1]
}).AddNchw(i1, o1, layout).AddVariations(quant8_signed, includeDefault=False)


# TEST 2: BATCH_TO_SPACE_NCHW_2, block_size = [2, 2]
i2 = Input("op1", "TENSOR_FLOAT32", "{4, 2, 2, 1}")
o2 = Output("op4", "TENSOR_FLOAT32", "{1, 4, 4, 1}")
Model().Operation("BATCH_TO_SPACE_ND", i2, [2, 2], layout).To(o2)

# Additional data type
quant8_signed = DataTypeConverter().Identify({
    i2: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.5, 0),
    o2: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.5, 0)
})

# Instantiate an example
example = Example({
    i2: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
    o2: [1, 5, 2, 6, 9, 13, 10, 14, 3, 7, 4, 8, 11, 15, 12, 16]
}).AddNchw(i2, o2, layout).AddVariations(quant8_signed, includeDefault=False)
