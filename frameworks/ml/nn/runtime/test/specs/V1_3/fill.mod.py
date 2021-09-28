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
def test(name, input_dims, value, output, input_dims_data, output_data):
  model = Model().Operation("FILL", input_dims, value).To(output)
  example = Example({
      input_dims: input_dims_data,
      output: output_data,
  },
                    model=model,
                    name=name).AddVariations("float16", "int32")


test(
    name="1d",
    input_dims=Input("input0", "TENSOR_INT32", "{1}"),
    value=Float32Scalar("value", 3.0),
    output=Output("output", "TENSOR_FLOAT32", "{5}"),
    input_dims_data=[5],
    output_data=[3.0] * 5,
)

test(
    name="3d",
    input_dims=Input("input0", "TENSOR_INT32", "{3}"),
    value=Float32Scalar("value", 3.0),
    output=Output("output", "TENSOR_FLOAT32", "{2, 3, 4}"),
    input_dims_data=[2, 3, 4],
    output_data=[3.0] * (2 * 3 * 4),
)

test(
    name="5d",
    input_dims=Input("input0", "TENSOR_INT32", "{5}"),
    value=Float32Scalar("value", 3.0),
    output=Output("output", "TENSOR_FLOAT32", "{1, 2, 3, 4, 5}"),
    input_dims_data=[1, 2, 3, 4, 5],
    output_data=[3.0] * (1 * 2 * 3 * 4 * 5),
)
