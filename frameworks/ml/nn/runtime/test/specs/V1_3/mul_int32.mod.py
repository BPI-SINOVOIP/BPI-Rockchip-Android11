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
input0 = Input("input0", "TENSOR_INT32", "{1, 2, 2}")
input1 = Input("input1", "TENSOR_INT32", "{1, 2, 2}")
output = Output("output", "TENSOR_INT32", "{1, 2, 2}")
model = model.Operation("MUL", input0, input1, 0).To(output)
Example({input0: [2, -4, 8, -16],
         input1: [32, -16, -8, 4],
         output: [64, 64, -64, -64]})
