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

i = Input("op1", "TENSOR_FLOAT32", "{2, 2, 2, 6}") # input
o = Output("op2", "TENSOR_FLOAT32", "{2, 2, 2, 6}") # output
axis = Int32Scalar("axis", -1) # last axis
input0 = {i: [-1.1, .6, .7, 1.2, -.7, .1] * 8}

# TEST 2: radius = 20, bias = 9.0, alpha = 4.0, beta = 0.5
Model("axis").Operation("LOCAL_RESPONSE_NORMALIZATION", i, 20, 9.0, 4.0, 0.5, axis).To(o)
Example((input0, {
        o: [-.22, .12, .14, .24, -.14, .02] * 8
    })).AddAllDimsAndAxis(i, o, axis).AddVariations("relaxed", "float16")

# TEST 4: radius = 2, bias = 9.0, alpha = 4.0, beta = 0.5
Model("axis").Operation("LOCAL_RESPONSE_NORMALIZATION", i, 2, 9.0, 4.0, 0.5, axis).To(o)
Example((input0, {
        o: [-.26492569, .12510864, .14011213, .26726127, -.16178755, .0244266] * 8
    })).AddAllDimsAndAxis(i, o, axis).AddVariations("relaxed", "float16")

# TEST5: All dimensions other than 4, without axis parameter
Model().Operation("LOCAL_RESPONSE_NORMALIZATION", i, 2, 9.0, 4.0, 0.5).To(o)
Example((input0, {
        o: [-.26492569, .12510864, .14011213, .26726127, -.16178755, .0244266] * 8
    })).AddDims([1, 2, 3], i, o, includeDefault=False).AddVariations("relaxed", "float16")

# The tests below can comply with a lower version because the runtime removes
# optional arguments set to default values.
Example.SetVersion("V1_0",
                   "local_response_normalization_v1_2_axis_dim4_axis3_neg",
                   "local_response_normalization_v1_2_axis_dim4_axis3_neg_2",
                   "local_response_normalization_v1_2_axis_dim4_axis3_neg_all_inputs_as_internal",
                   "local_response_normalization_v1_2_axis_dim4_axis3_neg_all_inputs_as_internal_2")
