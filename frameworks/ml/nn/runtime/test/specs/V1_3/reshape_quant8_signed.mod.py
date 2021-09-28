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
# a line of 3 pixels, 3 components/pixel
i1 = Input("op1", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 1, 3, 3}, 1.f, -128")
i2 = Parameter("op2", "TENSOR_INT32", "{1}", [-1])
i3 = Output("op3", "TENSOR_QUANT8_ASYMM_SIGNED", "{9}, 1.f, -128")
model = model.Operation("RESHAPE", i1, i2).To(i3)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
             [-127, -126, -125,
              -124, -123, -122,
              -121, -120, -119]}

output0 = {i3: # output 0
           [-127, -126, -125, -124, -123, -122, -121, -120, -119]}

# Instantiate an example
Example((input0, output0))
