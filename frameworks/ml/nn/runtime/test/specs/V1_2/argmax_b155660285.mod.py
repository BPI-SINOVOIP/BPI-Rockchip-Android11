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

# Model operands
op1 = Input("op1", ["TENSOR_FLOAT32", [3]])
op2 = Output("op2", ["TENSOR_INT32", [1]])

# Model operations
model = Model()
model.Operation("ARGMAX", op1, 0).To(op2)

# Example
Example({
    op1: [1.0, 2.0, 4.0],
    op2: [2],
}, model=model)
