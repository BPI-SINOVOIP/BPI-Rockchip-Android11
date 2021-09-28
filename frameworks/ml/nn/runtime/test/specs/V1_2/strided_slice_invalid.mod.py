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

# Based on strided_slice_float_11.mod.py.

# This test makes sure that executing STRIDED_SLICE results in a failure when
# the deduced output dimension is not one and shrinkAxisMask is set.
# See http://b/79856511#comment2.
i1 = Input("input", "TENSOR_FLOAT32", "{2, 3}")
output = Output("output", "TENSOR_FLOAT32", "{3}")
Model("output_dims").Operation("STRIDED_SLICE", i1, [0, 0], [2, 3], [1, 1], 0, 0, 1).To(output)
Example({
    i1: [1, 2, 3, 4, 5, 6],
    output: [1, 2, 3],
}).ExpectFailure()


# This test makes sure that executing STRIDED_SLICE results in a failure when
# the stride is negative and shrinkAxisMask is set.
# See http://b/154639297#comment11.
i1 = Input("input", "TENSOR_FLOAT32", "{2, 3}")
output = Output("output", "TENSOR_FLOAT32", "{3}")
Model("neg_stride").Operation("STRIDED_SLICE", i1, [1, 0], [0, 3], [-1, 1], 0, 0, 1).To(output)
Example({
    i1: [1, 2, 3, 4, 5, 6],
    output: [4, 5, 6],
}).ExpectFailure()
