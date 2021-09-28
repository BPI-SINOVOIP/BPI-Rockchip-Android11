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

# Model: z = if (value) then (x + y) else (x - y)
#        where value is a constant

x_data = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]
y_data = [8, 7, 6, 5, 4, 3, 2, 1, 0, -1, -2, -3]
output_data = {
    True: [x + y for (x, y) in zip(x_data, y_data)],
    False: [x - y for (x, y) in zip(x_data, y_data)],
}

ValueType = ["TENSOR_FLOAT32", [3, 4]]
BoolType = ["TENSOR_BOOL8", [1]]

def MakeBranchModel(operation_name):
  x = Input("x", ValueType)
  y = Input("y", ValueType)
  z = Output("z", ValueType)
  return Model().Operation(operation_name, x, y, 0).To(z)

def Test(value, name):
  x = Input("x", ValueType)
  y = Input("y", ValueType)
  z = Output("z", ValueType)
  cond = Parameter("cond", BoolType, [value])
  then_model = MakeBranchModel("ADD")
  else_model = MakeBranchModel("SUB")
  model = Model().Operation("IF", cond, then_model, else_model, x, y).To(z)

  quant8 = DataTypeConverter("quant8", scale=1.0, zeroPoint=100)
  quant8_signed = DataTypeConverter("quant8_signed", scale=1.0, zeroPoint=100)

  example = Example({
      x: x_data,
      y: y_data,
      z: output_data[value],
  }, model=model, name=name)
  example.AddVariations("relaxed", "float16", "int32", quant8, quant8_signed)
  example.DisableLifeTimeVariation()

# CONSTANT_COPY
Configuration.use_shm_for_weights = False
Test(value=True, name="copy_true")
Test(value=False, name="copy_false")

# CONSTANT_REFERENCE
Configuration.use_shm_for_weights = True
Test(value=True, name="reference_true")
Test(value=False, name="reference_false")
