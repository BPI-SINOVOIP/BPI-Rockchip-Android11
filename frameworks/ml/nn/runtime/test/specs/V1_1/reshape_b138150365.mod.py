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

# Issue b/138150365, truncation of last values of reshape output used in mobilenet
model = Model()
i1 = Input("op1", "TENSOR_FLOAT32", "{1, 1, 1, 1001}")
p2 =  Parameter("op2", "TENSOR_INT32", "{2}", [1, 1001])
o1 = Output("op3", "TENSOR_FLOAT32", "{1, 1001}")
model = model.Operation("RESHAPE", i1, p2).To(o1)
model = model.RelaxedExecution(True)

input0 = {i1: [-1.0] * 1001}
output0 = {o1:[-1.0] * 1001}
Example((input0, output0))
