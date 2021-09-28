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
i1 = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 24, 1}, 1.0, -128")
squeezeDims = Parameter("squeezeDims", "TENSOR_INT32", "{1}", [2])
output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 24}, 1.0, -128")

model = model.Operation("SQUEEZE", i1, squeezeDims).To(output)

data = [
    -127, -126, -125, -124, -123, -122, -121, -120, -119, -118, -117, -116, -115, -114, -113, -112,
    -111, -110, -109, -108, -107, -106, -105, -104
]

# Instantiate an example
Example({i1: data, output: data})

# Test with omitted squeeze dims
squeezeDims = Parameter("squeezeDims", ["TENSOR_INT32", [0]], value=None)
output = Output("output", "TENSOR_QUANT8_ASYMM_SIGNED", "{24}, 1.0, -128")
Model("omitted").Operation("SQUEEZE", i1, squeezeDims).To(output)
Example({i1: data, output: data})
