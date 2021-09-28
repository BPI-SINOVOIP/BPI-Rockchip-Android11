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
model = Model()
input0 = Input("input0", "TENSOR_INT32", "{2, 2, 4, 6}")
input1 = Input("input1", "TENSOR_INT32", "{2, 2, 4, 6}")
output = Output("output", "TENSOR_INT32", "{2, 2, 4, 6}")
model = model.Operation("DIV", input0, input1, 0).To(output)
Example({
    input0: [
        -6, -6, -6, -6, -6, -6,
        -5, -5, -5, -5, -5, -5,
        -4, -4, -4, -4, -4, -4,
        -3, -3, -3, -3, -3, -3,
        -2, -2, -2, -2, -2, -2,
        -1, -1, -1, -1, -1, -1,
        0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1,
        2, 2, 2, 2, 2, 2,
        3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5,
        6, 6, 6, 6, 6, 6,
        7, 7, 7, 7, 7, 7,
        8, 8, 8, 8, 8, 8,
        9, 9, 9, 9, 9, 9,
    ],
    input1: [
        -3, -2, -1, 1, 2, 3,
        -3, -2, -1, 1, 2, 3,
        -3, -2, -1, 1, 2, 3,
        -3, -2, -1, 1, 2, 3,
        -3, -2, -1, 1, 2, 3,
        -3, -2, -1, 1, 2, 3,
        -3, -2, -1, 1, 2, 3,
        -3, -2, -1, 1, 2, 3,
        -3, -2, -1, 1, 2, 3,
        -3, -2, -1, 1, 2, 3,
        -3, -2, -1, 1, 2, 3,
        -3, -2, -1, 1, 2, 3,
        -3, -2, -1, 1, 2, 3,
        -3, -2, -1, 1, 2, 3,
        -3, -2, -1, 1, 2, 3,
        -3, -2, -1, 1, 2, 3,
    ],
    output: [
        2, 3, 6, -6, -3, -2,
        1, 2, 5, -5, -3, -2,
        1, 2, 4, -4, -2, -2,
        1, 1, 3, -3, -2, -1,
        0, 1, 2, -2, -1, -1,
        0, 0, 1, -1, -1, -1,
        0, 0, 0, 0, 0, 0,
        -1, -1, -1, 1, 0, 0,
        -1, -1, -2, 2, 1, 0,
        -1, -2, -3, 3, 1, 1,
        -2, -2, -4, 4, 2, 1,
        -2, -3, -5, 5, 2, 1,
        -2, -3, -6, 6, 3, 2,
        -3, -4, -7, 7, 3, 2,
        -3, -4, -8, 8, 4, 2,
        -3, -5, -9, 9, 4, 3,
    ],
})


# Test DIV by zero.
# It is undefined behavior. The output is ignored and we only require the drivers to not crash.
input0 = Input("input0", "TENSOR_INT32", "{1}")
input1 = Input("input1", "TENSOR_INT32", "{1}")
output = IgnoredOutput("output", "TENSOR_INT32", "{1}")
model = Model("by_zero").Operation("DIV", input0, input1, 0).To(output)
Example({
    input0: [1],
    input1: [0],
    output: [0],
})
