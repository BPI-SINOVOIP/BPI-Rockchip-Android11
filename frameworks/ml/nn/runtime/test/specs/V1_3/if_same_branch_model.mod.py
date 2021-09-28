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

# Test with multiple references to the same referenced model.
# Model: z = if (x) then (y + 10) else (y + 10)

input_data = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
output_add = [y + 10 for y in input_data]

ValueType = ["TENSOR_FLOAT32", [3, 4]]
BoolType = ["TENSOR_BOOL8", [1]]

def MakeBranchModel(operation_name):
  y = Input("y", ValueType)
  z = Output("z", ValueType)
  return Model().Operation(operation_name, y, [10.0], 0).To(z)

def Test(x, y, z, name):
  x_data, y_data, z_data = x, y, z
  x = Input("x", BoolType)
  y = Input("y", ValueType)
  z = Output("z", ValueType)
  then_model = MakeBranchModel("ADD")
  else_model = then_model
  model = Model().Operation("IF", x, then_model, else_model, y).To(z)

  quant8 = DataTypeConverter("quant8", scale=1.0, zeroPoint=100)
  quant8_signed = DataTypeConverter("quant8_signed", scale=1.0, zeroPoint=100)

  example = Example({x: [x_data], y: y_data, z: z_data}, name=name)
  example.AddVariations("relaxed", "float16", "int32", quant8, quant8_signed)
  example.AddVariations(AllOutputsAsInternalCoverter())

Test(x=True, y=input_data, z=output_add, name="true")
Test(x=False, y=input_data, z=output_add, name="false")
