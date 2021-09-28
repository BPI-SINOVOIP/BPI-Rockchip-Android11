#
# Copyright (C) 2020 The Android Open Source Project
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

# Test L2_NORMALIZATION with inputs of all zeros.
i1 = Input("op1", "TENSOR_FLOAT32", "{2}") # input 0
o1 = Output("op2", "TENSOR_FLOAT32", "{2}") # output 0
axis = Int32Scalar("axis", -1) # last axis

quant8 = DataTypeConverter().Identify({
    i1: ("TENSOR_QUANT8_ASYMM", 0.1, 32),
    o1: ("TENSOR_QUANT8_ASYMM", 1.0 / 128, 128)
})

quant8_signed = DataTypeConverter().Identify({
    i1: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, -96),
    o1: ("TENSOR_QUANT8_ASYMM_SIGNED", 1.0 / 128, 0)
})

Model().IntroducedIn("V1_2").Operation("L2_NORMALIZATION", i1, axis).To(o1)
Example({
    i1: [0, 0],
    o1: [0, 0]
}).AddVariations("relaxed", "float16", quant8, quant8_signed)

# TENSOR_QUANT8_ASYMM_SIGNED is a new data type in V1_3.
Example.SetVersion("V1_3",
                   "l2_normalization_zeros_quant8_signed",
                   "l2_normalization_zeros_quant8_signed_all_inputs_as_internal")
