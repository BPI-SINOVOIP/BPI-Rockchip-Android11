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

input0 = Input("input0", "TENSOR_QUANT8_ASYMM_SIGNED", "{6}, 1.0, -128")
axis = Int32Scalar("axis", 0)
num_splits = Int32Scalar("num_splits", 3)
output0 = Output("output0", "TENSOR_QUANT8_ASYMM_SIGNED", "{2}, 1.0, -128")
output1 = Output("output1", "TENSOR_QUANT8_ASYMM_SIGNED", "{2}, 1.0, -128")
output2 = Output("output2", "TENSOR_QUANT8_ASYMM_SIGNED", "{2}, 1.0, -128")

model = Model().Operation("SPLIT", input0, axis, num_splits).To(
    (output0, output1, output2))

# Example 1.
input_dict = {input0: [-127, -126, -125, -124, -123, -122]}
output_dict = {
    output0: [-127, -126],
    output1: [-125, -124],
    output2: [-123, -122],
}

# Instantiate an example
Example((input_dict, output_dict)).AddRelaxed()

#######################################################

input0 = Input("input0", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 3}, 2.0, -125")
axis = Int32Scalar("axis", 0)
num_splits = Int32Scalar("num_splits", 2)
output0 = Output("output0", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 3}, 2.0, -125")
output1 = Output("output1", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 3}, 2.0, -125")

model = Model().Operation("SPLIT", input0, axis, num_splits).To(
    (output0, output1))

# Example 1.
input_dict = {input0: [-127, -126, -125, -124, -123, -122]}
output_dict = {
    output0: [-127, -126, -125],
    output1: [-124, -123, -122],
}

# Instantiate an example
Example((input_dict, output_dict)).AddRelaxed()

#######################################################

input0 = Input("input0", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 3}, 2.0, -125")
axis = Int32Scalar("axis", 1)
num_splits = Int32Scalar("num_splits", 3)
output0 = Output("output0", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 1}, 2.0, -125")
output1 = Output("output1", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 1}, 2.0, -125")
output2 = Output("output2", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 1}, 2.0, -125")

model = Model().Operation("SPLIT", input0, axis, num_splits).To(
    (output0, output1, output2))

# Example 1.
input_dict = {input0: [-127, -126, -125, -124, -123, -122]}
output_dict = {
    output0: [-127, -124],
    output1: [-126, -123],
    output2: [-125, -122],
}

# Instantiate an example
Example((input_dict, output_dict))

#######################################################

input0 = Input("input0", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 2, 2}, 1.0, -128")
axis = Int32Scalar("axis", 1)
num_splits = Int32Scalar("num_splits", 2)
output0 = Output("output0", "TENSOR_QUANT8_ASYMM_SIGNED",
                 "{2, 1, 2}, 1.0, -128")
output1 = Output("output1", "TENSOR_QUANT8_ASYMM_SIGNED",
                 "{2, 1, 2}, 1.0, -128")

model = Model().Operation("SPLIT", input0, axis, num_splits).To(
    (output0, output1))

# Example 1.
input_dict = {input0: [-127, -126, -125, -124, -123, -122, -121, -120]}
output_dict = {
    output0: [-127, -126, -123, -122],
    output1: [-125, -124, -121, -120],
}

# Instantiate an example
Example((input_dict, output_dict))
