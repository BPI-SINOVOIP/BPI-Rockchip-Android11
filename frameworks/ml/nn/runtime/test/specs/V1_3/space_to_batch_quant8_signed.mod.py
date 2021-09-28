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
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 4, 4, 1}, 1.0, -128")
block = Parameter("block_size", "TENSOR_INT32", "{2}", [2, 2])
paddings = Parameter("paddings", "TENSOR_INT32", "{2, 2}", [0, 0, 0, 0])
output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{4, 2, 2, 1}, 1.0, -128")

model = model.Operation("SPACE_TO_BATCH_ND", i1, block, paddings).To(output)

# Example 1. Input in operand 0,
input0 = {
    i1:  # input 0
        [
            -127, -126, -125, -124, -123, -122, -121, -120, -119, -118, -117,
            -116, -115, -114, -113, -112
        ]
}

output0 = {
    output:  # output 0
        [
            -127, -125, -119, -117, -126, -124, -118, -116, -123, -121, -115,
            -113, -122, -120, -114, -112
        ]
}

# Instantiate an example
Example((input0, output0))

#######################################################

model = Model()
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 5, 2, 1}, 1.0, -128")
block = Parameter("block_size", "TENSOR_INT32", "{2}", [3, 2])
paddings = Parameter("paddings", "TENSOR_INT32", "{2, 2}", [1, 0, 2, 0])
output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{6, 2, 2, 1}, 1.0, -128")

model = model.Operation("SPACE_TO_BATCH_ND", i1, block, paddings).To(output)

# Example 1. Input in operand 0,
input0 = {
    i1:  # input 0
        [-127, -126, -125, -124, -123, -122, -121, -120, -119, -118]
}

output0 = {
    output:  # output 0
        [
            -128, -128, -128, -123, -128, -128, -128, -122, -128, -127, -128,
            -121, -128, -126, -128, -120, -128, -125, -128, -119, -128, -124,
            -128, -118
        ]
}

# Instantiate an example
Example((input0, output0))

#######################################################

model = Model()
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 4, 2, 1}, 1.0, -128")
block = Parameter("block_size", "TENSOR_INT32", "{2}", [3, 2])
paddings = Parameter("paddings", "TENSOR_INT32", "{2, 2}", [1, 1, 2, 4])
output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{6, 2, 4, 1}, 1.0, -128")

model = model.Operation("SPACE_TO_BATCH_ND", i1, block, paddings).To(output)

# Example 1. Input in operand 0,
input0 = {
    i1:  # input 0
        [-127, -126, -125, -124, -123, -122, -121, -120]
}

output0 = {
    output:  # output 0
        [
            -128, -128, -128, -128, -128, -123, -128, -128, -128, -128, -128,
            -128, -128, -122, -128, -128, -128, -127, -128, -128, -128, -121,
            -128, -128, -128, -126, -128, -128, -128, -120, -128, -128, -128,
            -125, -128, -128, -128, -128, -128, -128, -128, -124, -128, -128,
            -128, -128, -128, -128
        ]
}

# Instantiate an example
Example((input0, output0))

#######################################################
# Quantized SPACE_TO_BATCH_ND with non-zero zeroPoint is supported since 1.2.
# See http://b/132112227.

model = Model()
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 5, 2, 1}, 1.0, -119")
block = Parameter("block_size", "TENSOR_INT32", "{2}", [3, 2])
paddings = Parameter("paddings", "TENSOR_INT32", "{2, 2}", [1, 0, 2, 0])
output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{6, 2, 2, 1}, 1.0, -119")

model = model.Operation("SPACE_TO_BATCH_ND", i1, block, paddings).To(output)

# Example 1. Input in operand 0,
input0 = {
    i1:  # input 0
        [-127, -126, -125, -124, -123, -122, -121, -120, -119, -118]
}

output0 = {
    output:  # output 0
        [
            -119, -119, -119, -123, -119, -119, -119, -122, -119, -127, -119,
            -121, -119, -126, -119, -120, -119, -125, -119, -119, -119, -124,
            -119, -118
        ]
}

# Instantiate an example
Example((input0, output0))

#######################################################

layout = BoolScalar("layout", False) # NHWC

# SPACE_TO_BATCH_NCHW_1, block_size = [2, 2]
i1 = Input("op1", "TENSOR_FLOAT32", "{1, 2, 2, 2}")
pad1 = Parameter("paddings", "TENSOR_INT32", "{2, 2}", [0, 0, 0, 0])
o1 = Output("op4", "TENSOR_FLOAT32", "{4, 1, 1, 2}")
Model().Operation("SPACE_TO_BATCH_ND", i1, [2, 2], pad1, layout).To(o1)

# Additional data type
quant8_signed = DataTypeConverter().Identify({
    i1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, -128),
    o1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, -128)
})

# Instantiate an example
example = Example({
    i1: [1.4, 2.3, 3.2, 4.1, 5.4, 6.3, 7.2, 8.1],
    o1: [1.4, 2.3, 3.2, 4.1, 5.4, 6.3, 7.2, 8.1]
}).AddNchw(i1, o1, layout).AddVariations(quant8_signed, includeDefault=False)

#######################################################
# SPACE_TO_BATCH_NCHW_2, block_size = [2, 2]
i2 = Input("op1", "TENSOR_FLOAT32", "{1, 4, 4, 1}")
o2 = Output("op4", "TENSOR_FLOAT32", "{4, 2, 2, 1}")
Model().Operation("SPACE_TO_BATCH_ND", i2, [2, 2], pad1, layout).To(o2)

# Additional data type
quant8_signed = DataTypeConverter().Identify({
    i2: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.5, -128),
    o2: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.5, -128)
})

# Instantiate an example
example = Example({
    i2: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
    o2: [1, 3, 9, 11, 2, 4, 10, 12, 5, 7, 13, 15, 6, 8, 14, 16]
}).AddNchw(i2, o2, layout).AddVariations(quant8_signed, includeDefault=False)

#######################################################
# SPACE_TO_BATCH_NCHW_3, block_size = [3, 2]
i3 = Input("op1", "TENSOR_FLOAT32", "{1, 5, 2, 1}")
pad3 = Parameter("paddings", "TENSOR_INT32", "{2, 2}", [1, 0, 2, 0])
o3 = Output("op4", "TENSOR_FLOAT32", "{6, 2, 2, 1}")
Model().Operation("SPACE_TO_BATCH_ND", i3, [3, 2], pad3, layout).To(o3)

# Additional data type
quant8_signed = DataTypeConverter().Identify({
    i3: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.5, 0),
    o3: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.5, 0)
})

# Instantiate an example
example = Example({
    i3: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
    o3: [0, 0, 0, 5, 0, 0, 0, 6, 0, 1, 0, 7,
         0, 2, 0, 8, 0, 3, 0, 9, 0, 4, 0, 10]
}).AddNchw(i3, o3, layout).AddVariations(quant8_signed, includeDefault=False)

#######################################################
# SPACE_TO_BATCH_NCHW_4, block_size = [3, 2]
i4 = Input("op1", "TENSOR_FLOAT32", "{1, 4, 2, 1}")
pad4 = Parameter("paddings", "TENSOR_INT32", "{2, 2}", [1, 1, 2, 4])
o4 = Output("op4", "TENSOR_FLOAT32", "{6, 2, 4, 1}")
Model().Operation("SPACE_TO_BATCH_ND", i4, [3, 2], pad4, layout).To(o4)

# Additional data type
quant8_signed = DataTypeConverter().Identify({
    i4: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.25, 0),
    o4: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.25, 0)
})

# Instantiate an example
example = Example({
    i4: [1, 2, 3, 4, 5, 6, 7, 8],
    o4: [0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 6, 0, 0,
         0, 1, 0, 0, 0, 7, 0, 0, 0, 2, 0, 0, 0, 8, 0, 0,
         0, 3, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0]
}).AddNchw(i4, o4, layout).AddVariations(quant8_signed, includeDefault=False)
