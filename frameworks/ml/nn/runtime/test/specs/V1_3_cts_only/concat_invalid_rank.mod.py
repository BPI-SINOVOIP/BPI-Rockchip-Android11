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

input0 = Input("input0", "TENSOR_FLOAT32", "{1, 1, 1, 1, 2}")
input1 = Input("input1", "TENSOR_FLOAT32", "{1, 1, 1, 1, 2}")
axis = 4
output0 = Output("output0", "TENSOR_FLOAT32", "{1, 1, 1, 1, 4}")

model = Model().Operation("CONCATENATION", input0, input1,
                          axis).To(output0).IntroducedIn("V1_0")

Example({
    input0: [1, 2],
    input1: [3, 4],
    output0: [1, 2, 3, 4],
}).ExpectFailure()
