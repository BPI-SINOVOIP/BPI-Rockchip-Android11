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
base = Input("base", "TENSOR_FLOAT32", "{3, 2, 1}")
exponent = Input("exponent", "TENSOR_FLOAT32", "{3, 2, 1}")
output = Output("output", "TENSOR_FLOAT32", "{3, 2, 1}")

base_data = [1, 2, 3, 4, 5, 6]
exponent_data = [4, 3, 2, 1, 0.5, 0]
output_data = [base_data[i]**exponent_data[i] for i in range(len(base_data))]

model = Model().Operation("POW", base, exponent).To(output).IntroducedIn("V1_2")
Example({
    base: base_data,
    exponent: exponent_data,
    output: output_data
}, model=model).AddVariations("relaxed", "float16")
