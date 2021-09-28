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
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 3}, 1.0, -128")
begins = Parameter("begins", "TENSOR_INT32", "{2}", [1, 0])
ends = Parameter("ends", "TENSOR_INT32", "{2}", [2, 2])
strides = Parameter("strides", "TENSOR_INT32", "{2}", [1, 1])
beginMask = Int32Scalar("beginMask", 0)
endMask = Int32Scalar("endMask", 2)
shrinkAxisMask = Int32Scalar("shrinkAxisMask", 0)

output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 3}, 1.0, -128")

model = model.Operation("STRIDED_SLICE", i1, begins, ends, strides, beginMask, endMask, shrinkAxisMask).To(output)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [-127, -126, -125, -124, -123, -122]}

output0 = {output: # output 0
           [-124, -123, -122]}

# Instantiate an example
Example((input0, output0))

#######################################################

model = Model()
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 3}, 1.0, -128")
begins = Parameter("begins", "TENSOR_INT32", "{2}", [0, 0])
ends = Parameter("ends", "TENSOR_INT32", "{2}", [1, 3])
strides = Parameter("strides", "TENSOR_INT32", "{2}", [1, 1])
beginMask = Int32Scalar("beginMask", 0)
endMask = Int32Scalar("endMask", 0)
shrinkAxisMask = Int32Scalar("shrinkAxisMask", 1)

output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{3}, 1.0, -128")

model = model.Operation("STRIDED_SLICE", i1, begins, ends, strides, beginMask, endMask, shrinkAxisMask).To(output)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [-127, -126, -125, -124, -123, -122]}

output0 = {output: # output 0
           [-127, -126, -125]}

# Instantiate an example
Example((input0, output0))

#######################################################

model = Model()
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{4}, 1.0, -128")
begins = Parameter("begins", "TENSOR_INT32", "{1}", [1])
ends = Parameter("ends", "TENSOR_INT32", "{1}", [3])
strides = Parameter("strides", "TENSOR_INT32", "{1}", [1])
beginMask = Int32Scalar("beginMask", 0)
endMask = Int32Scalar("endMask", 0)
shrinkAxisMask = Int32Scalar("shrinkAxisMask", 0)

output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{2}, 1.0, -128")

model = model.Operation("STRIDED_SLICE", i1, begins, ends, strides, beginMask, endMask, shrinkAxisMask).To(output)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [-127, -126, -125, -124]}

output0 = {output: # output 0
           [-126, -125]}

# Instantiate an example
Example((input0, output0))

#######################################################

model = Model()
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{4}, 1.0, -128")
begins = Parameter("begins", "TENSOR_INT32", "{1}", [-3])
ends = Parameter("ends", "TENSOR_INT32", "{1}", [3])
strides = Parameter("strides", "TENSOR_INT32", "{1}", [1])
beginMask = Int32Scalar("beginMask", 0)
endMask = Int32Scalar("endMask", 0)
shrinkAxisMask = Int32Scalar("shrinkAxisMask", 0)

output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{2}, 1.0, -128")

model = model.Operation("STRIDED_SLICE", i1, begins, ends, strides, beginMask, endMask, shrinkAxisMask).To(output)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [-127, -126, -125, -124]}

output0 = {output: # output 0
           [-126, -125]}

# Instantiate an example
Example((input0, output0))

#######################################################

model = Model()
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{4}, 1.0, -128")
begins = Parameter("begins", "TENSOR_INT32", "{1}", [-5])
ends = Parameter("ends", "TENSOR_INT32", "{1}", [3])
strides = Parameter("strides", "TENSOR_INT32", "{1}", [1])
beginMask = Int32Scalar("beginMask", 0)
endMask = Int32Scalar("endMask", 0)
shrinkAxisMask = Int32Scalar("shrinkAxisMask", 0)

output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{3}, 1.0, -128")

model = model.Operation("STRIDED_SLICE", i1, begins, ends, strides, beginMask, endMask, shrinkAxisMask).To(output)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [-127, -126, -125, -124]}

output0 = {output: # output 0
           [-127, -126, -125]}

# Instantiate an example
Example((input0, output0))

#######################################################

model = Model()
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{4}, 1.0, -128")
begins = Parameter("begins", "TENSOR_INT32", "{1}", [1])
ends = Parameter("ends", "TENSOR_INT32", "{1}", [-2])
strides = Parameter("strides", "TENSOR_INT32", "{1}", [1])
beginMask = Int32Scalar("beginMask", 0)
endMask = Int32Scalar("endMask", 0)
shrinkAxisMask = Int32Scalar("shrinkAxisMask", 0)

output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{1}, 1.0, -128")

model = model.Operation("STRIDED_SLICE", i1, begins, ends, strides, beginMask, endMask, shrinkAxisMask).To(output)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [-127, -126, -125, -124]}

output0 = {output: # output 0
           [-126]}

# Instantiate an example
Example((input0, output0))

#######################################################

model = Model()
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{4}, 1.0, -128")
begins = Parameter("begins", "TENSOR_INT32", "{1}", [1])
ends = Parameter("ends", "TENSOR_INT32", "{1}", [3])
strides = Parameter("strides", "TENSOR_INT32", "{1}", [1])
beginMask = Int32Scalar("beginMask", 1)
endMask = Int32Scalar("endMask", 0)
shrinkAxisMask = Int32Scalar("shrinkAxisMask", 0)

output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{3}, 1.0, -128")

model = model.Operation("STRIDED_SLICE", i1, begins, ends, strides, beginMask, endMask, shrinkAxisMask).To(output)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [-127, -126, -125, -124]}

output0 = {output: # output 0
           [-127, -126, -125]}

# Instantiate an example
Example((input0, output0))

#######################################################

model = Model()
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{4}, 1.0, -128")
begins = Parameter("begins", "TENSOR_INT32", "{1}", [1])
ends = Parameter("ends", "TENSOR_INT32", "{1}", [3])
strides = Parameter("strides", "TENSOR_INT32", "{1}", [1])
beginMask = Int32Scalar("beginMask", 0)
endMask = Int32Scalar("endMask", 1)
shrinkAxisMask = Int32Scalar("shrinkAxisMask", 0)

output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{3}, 1.0, -128")

model = model.Operation("STRIDED_SLICE", i1, begins, ends, strides, beginMask, endMask, shrinkAxisMask).To(output)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [-127, -126, -125, -124]}

output0 = {output: # output 0
           [-126, -125, -124]}

# Instantiate an example
Example((input0, output0))

#######################################################

model = Model()
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{3}, 1.0, -128")
begins = Parameter("begins", "TENSOR_INT32", "{1}", [-1])
ends = Parameter("ends", "TENSOR_INT32", "{1}", [-4])
strides = Parameter("strides", "TENSOR_INT32", "{1}", [-1])
beginMask = Int32Scalar("beginMask", 0)
endMask = Int32Scalar("endMask", 0)
shrinkAxisMask = Int32Scalar("shrinkAxisMask", 0)

output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{3}, 1.0, -128")

model = model.Operation("STRIDED_SLICE", i1, begins, ends, strides, beginMask, endMask, shrinkAxisMask).To(output)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [-127, -126, -125]}

output0 = {output: # output 0
           [-125, -126, -127]}

# Instantiate an example
Example((input0, output0))

#######################################################

model = Model()
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 3}, 1.0, -128")
begins = Parameter("begins", "TENSOR_INT32", "{2}", [1, -1])
ends = Parameter("ends", "TENSOR_INT32", "{2}", [2, -4])
strides = Parameter("strides", "TENSOR_INT32", "{2}", [2, -1])
beginMask = Int32Scalar("beginMask", 0)
endMask = Int32Scalar("endMask", 0)
shrinkAxisMask = Int32Scalar("shrinkAxisMask", 0)

output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 3}, 1.0, -128")

model = model.Operation("STRIDED_SLICE", i1, begins, ends, strides, beginMask, endMask, shrinkAxisMask).To(output)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [-127, -126, -125, -124, -123, -122]}

output0 = {output: # output 0
           [-122, -123, -124]}

# Instantiate an example
Example((input0, output0))

#######################################################

model = Model()
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 3}, 1.0, -128")
begins = Parameter("begins", "TENSOR_INT32", "{2}", [1, 0])
ends = Parameter("ends", "TENSOR_INT32", "{2}", [2, 2])
strides = Parameter("strides", "TENSOR_INT32", "{2}", [1, 1])
beginMask = Int32Scalar("beginMask", 1)
endMask = Int32Scalar("endMask", 0)
shrinkAxisMask = Int32Scalar("shrinkAxisMask", 0)

output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 2}, 1.0, -128")

model = model.Operation("STRIDED_SLICE", i1, begins, ends, strides, beginMask, endMask, shrinkAxisMask).To(output)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [-127, -126, -125, -124, -123, -122]}

output0 = {output: # output 0
           [-127, -126, -124, -123]}

# Instantiate an example
Example((input0, output0))
