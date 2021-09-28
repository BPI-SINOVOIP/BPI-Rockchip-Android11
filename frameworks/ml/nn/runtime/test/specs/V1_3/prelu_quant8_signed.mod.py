#
# Copyright (C) 2018 The Android Open Source Project
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

i1 = Input("input", "TENSOR_FLOAT32", "{1, 2, 2, 3}")
a1 = Parameter("alpha", "TENSOR_FLOAT32", "{1, 1, 3}", [0, 1, 2])
o1 = Output("output", "TENSOR_FLOAT32", "{1, 2, 2, 3}")
Model().Operation("PRELU", i1, a1).To(o1)

# output.scale > input.scale && output.scale > input.scale * alpha.scale
quant8_signed_gt = DataTypeConverter().Identify({
    i1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.25, 0),
    a1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.25, -78),
    o1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.5, -8)
})

# output.scale == input.scale
quant8_signed_eq1 = DataTypeConverter().Identify({
    i1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.25, 0),
    a1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.25, -78),
    o1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.25, -8)
})

# output.scale == input.scale * alpha.scale
quant8_signed_eq2 = DataTypeConverter().Identify({
    i1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.25, 0),
    a1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.5, -78),
    o1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.125, -8)
})

# output.scale < input.scale && output.scale < input.scale * alpha.scale
quant8_signed_lt = DataTypeConverter().Identify({
    i1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.25, 0),
    a1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.5, -78),
    o1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, -8)
})

# Instantiate an example
Example({
    i1: [ 0,  0,  0,
          1,  1,  1,
         -1, -1, -1,
         -2, -2, -2],
    o1: [ 0,  0,  0,
          1,  1,  1,
          0, -1, -2,
          0, -2, -4]
}).AddVariations(quant8_signed_gt, quant8_signed_eq1, quant8_signed_eq2, quant8_signed_lt, includeDefault=False)
