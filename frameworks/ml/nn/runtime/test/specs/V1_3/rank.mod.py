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
def test(name, input0, output0, input0_data, output0_data):
  model = Model().Operation("RANK", input0).To(output0)
  quant8 = DataTypeConverter().Identify({
      input0: ("TENSOR_QUANT8_ASYMM", 0.1, 128),
  })
  quant8_signed = DataTypeConverter().Identify({
      input0: ("TENSOR_QUANT8_ASYMM_SIGNED", 0.1, 0),
  })
  example = Example({
      input0: input0_data,
      output0: output0_data,
  }, model=model, name=name).AddVariations("int32", "float16", quant8, quant8_signed)

test(
    name="1d",
    input0=Input("input0", "TENSOR_FLOAT32", "{3}"),
    output0=Output("output0", "INT32", "{}"),
    input0_data=[5, 7, 10],
    output0_data=[1],
)

test(
    name="1d",
    input0=Input("input0", "TENSOR_FLOAT32", "{2, 3}"),
    output0=Output("output0", "INT32", "{}"),
    input0_data=[1, 2, 3, 4, 5, 6],
    output0_data=[2],
)

# b/150728111 regression test.
# Rank is a first operation that produces a scalar output.
# This test verifies that RANK works with a scalar output
# that's internal graph variable (not input or output of a graph).
def test_internal_output(name, rank_input, fill_dims, fill_output, rank_input_data,
                  fill_dims_data, fill_output_data):
  internal_result = Internal("rank_internal_result", "INT32", "{}")
  model = Model()
  model = model.Operation("RANK", rank_input).To(internal_result)
  model = model.Operation("FILL", fill_dims, internal_result).To(fill_output)

  example = Example({
      rank_input: rank_input_data,
      fill_dims: fill_dims_data,
      fill_output: fill_output_data,
  }, model=model, name=name)

test_internal_output(
    name="internal_output",
    rank_input=Input("input0", "TENSOR_FLOAT32", "{2, 3}"),
    fill_dims=Input("input0", "TENSOR_INT32", "{3}"),
    fill_output=Output("output", "TENSOR_INT32", "{2, 3, 4}"),
    rank_input_data=[1, 2, 3, 4, 5, 6],
    fill_dims_data=[2, 3, 4],
    fill_output_data=[2] * (2 * 3 * 4),
)
