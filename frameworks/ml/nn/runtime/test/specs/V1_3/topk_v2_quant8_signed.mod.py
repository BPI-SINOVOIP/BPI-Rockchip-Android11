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

inp = Input("input", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 3}, 2.0, 0")
inp_data = [-127, -126, -125, 123, 122, 121]
k = Int32Scalar("k", 2)
out_values = Output("out_values", "TENSOR_QUANT8_ASYMM_SIGNED", "{2, 2}, 2.0, 0")
out_values_data = [-125, -126, 123, 122]
out_indices = Output("out_indices", "TENSOR_INT32", "{2, 2}")
out_indices_data = [2, 1, 0, 1]

model = Model().Operation("TOPK_V2", inp, k).To(out_values, out_indices)
Example(
    {
        inp: inp_data,
        out_values: out_values_data,
        out_indices: out_indices_data
    },
    model=model)
