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

def test(name, input0, alpha, output0, input0_data, output0_data):
  model = Model().Operation("ELU", input0, alpha).To(output0)
  example = Example({
      input0: input0_data,
      output0: output0_data,
  }, model=model, name=name).AddVariations("float16", "relaxed")

test(
    name="alpha_one",
    input0=Input("input0", "TENSOR_FLOAT32", "{8}"),
    alpha=Float32Scalar("alpha", 1.0),
    output0=Output("output0", "TENSOR_FLOAT32", "{8}"),
    input0_data=[0, -6, 2, -4, 3, -2, 10, -0.1],
    output0_data=[0.0, -0.997521, 2.0, -0.981684, 3.0, -0.864665, 10.0, -0.0951626],
)

test(
    name="alpha01",
    input0=Input("input0", "TENSOR_FLOAT32", "{2, 2}"),
    alpha=Float32Scalar("alpha", 0.1),
    output0=Output("output0", "TENSOR_FLOAT32", "{2, 2}"),
    input0_data=[-0.2, -0.1, 0.0, 0.1],
    output0_data=[-0.018127, -0.009516, 0, 0.1],
)

test(
    name="alpha10",
    input0=Input("input0", "TENSOR_FLOAT32", "{2, 1, 1, 1, 2}"),
    alpha=Float32Scalar("alpha", 10),
    output0=Output("output0", "TENSOR_FLOAT32", "{2, 1, 1, 1, 2}"),
    input0_data=[-10, -5, 0, 5],
    output0_data=[-9.999546, -9.932620, 0, 5],
)
