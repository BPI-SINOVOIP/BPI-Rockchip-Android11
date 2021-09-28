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

i1 = Input("input", "TENSOR_FLOAT32", "{4, 1, 1, 2}")
squeezeDims = Parameter("squeezeDims", ["TENSOR_INT32", [0]], value=None)
o1 = Output("output", "TENSOR_FLOAT32", "{4, 2}")
Model().Operation("SQUEEZE", i1, squeezeDims).To(o1)

quant8 = DataTypeConverter().Identify({
    i1: ("TENSOR_QUANT8_ASYMM", 0.5, 0),
    o1: ("TENSOR_QUANT8_ASYMM", 0.5, 0)
})

# Instantiate an example
Example({
    i1: [1.4, 2.3, 3.2, 4.1, 5.4, 6.3, 7.2, 8.1],
    o1: [1.4, 2.3, 3.2, 4.1, 5.4, 6.3, 7.2, 8.1]
}).AddVariations("relaxed", quant8)
