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

# Test for QUANTIZED_LSTM op.
import copy

model = Model()

batch_size = 2
input_size = 5
num_units = 4
output_size = 3

InputType = ("TENSOR_QUANT8_ASYMM_SIGNED", [batch_size, input_size], 0.0078125, 0)
input = Input("input", InputType)

InputWeightsType = ("TENSOR_QUANT8_SYMM", [num_units, input_size], 0.00784314, 0)
input_to_input_weights = Input("input_to_input_weights", InputWeightsType)
input_to_forget_weights = Input("input_to_forget_weights", InputWeightsType)
input_to_cell_weights = Input("input_to_cell_weights", InputWeightsType)
input_to_output_weights = Input("input_to_output_weights", InputWeightsType)

RecurrentWeightsType = ("TENSOR_QUANT8_SYMM", [num_units, output_size], 0.00784314, 0)
recurrent_to_input_weights = Input("recurrent_to_input_weights", RecurrentWeightsType)
recurrent_to_forget_weights = Input("recurrent_to_forget_weights", RecurrentWeightsType)
recurrent_to_cell_weights = Input("recurrent_to_cell_weights", RecurrentWeightsType)
recurrent_to_output_weights = Input("recurrent_to_output_weights", RecurrentWeightsType)

CellWeightsType = ("TENSOR_QUANT16_SYMM", [num_units], 1.0, 0)
cell_to_input_weights = Input("cell_to_input_weights", CellWeightsType)
cell_to_forget_weights = Input("cell_to_forget_weights", CellWeightsType)
cell_to_output_weights = Input("cell_to_output_weights", CellWeightsType)

# The bias scale value here is not used.
BiasType = ("TENSOR_INT32", [num_units], 0.0, 0)
input_gate_bias = Input("input_gate_bias", BiasType)
forget_gate_bias = Input("forget_gate_bias", BiasType)
cell_gate_bias = Input("cell_gate_bias", BiasType)
output_gate_bias = Input("output_gate_bias", BiasType)

projection_weights = Input("projection_weights",
                           ("TENSOR_QUANT8_SYMM", [output_size, num_units], 0.00392157, 0))
projection_bias = Input("projection_bias", ("TENSOR_INT32", [output_size]))

OutputStateType = ("TENSOR_QUANT8_ASYMM_SIGNED", [batch_size, output_size], 3.05176e-05, 0)
CellStateType = ("TENSOR_QUANT16_SYMM", [batch_size, num_units], 3.05176e-05, 0)
output_state_in = Input("output_state_in", OutputStateType)
cell_state_in = Input("cell_state_in", CellStateType)

LayerNormType = ("TENSOR_QUANT16_SYMM", [num_units], 3.05182e-05, 0)
input_layer_norm_weights = Input("input_layer_norm_weights", LayerNormType)
forget_layer_norm_weights = Input("forget_layer_norm_weights", LayerNormType)
cell_layer_norm_weights = Input("cell_layer_norm_weights", LayerNormType)
output_layer_norm_weights = Input("output_layer_norm_weights", LayerNormType)

cell_clip = Float32Scalar("cell_clip", 0.)
projection_clip = Float32Scalar("projection_clip", 0.)

input_intermediate_scale = Float32Scalar("input_intermediate_scale", 0.007059)
forget_intermediate_scale = Float32Scalar("forget_intermediate_scale", 0.007812)
cell_intermediate_scale = Float32Scalar("cell_intermediate_scale", 0.007059)
output_intermediate_scale = Float32Scalar("output_intermediate_scale", 0.007812)
hidden_state_zero_point = Int32Scalar("hidden_state_zero_point", 0)
hidden_state_scale = Float32Scalar("hidden_state_scale", 0.007)

output_state_out = Output("output_state_out", OutputStateType)
cell_state_out = Output("cell_state_out", CellStateType)
output = Output("output", OutputStateType)

model = model.Operation(
    "QUANTIZED_LSTM", input, input_to_input_weights, input_to_forget_weights,
    input_to_cell_weights, input_to_output_weights, recurrent_to_input_weights,
    recurrent_to_forget_weights, recurrent_to_cell_weights,
    recurrent_to_output_weights, cell_to_input_weights, cell_to_forget_weights,
    cell_to_output_weights, input_gate_bias, forget_gate_bias, cell_gate_bias,
    output_gate_bias, projection_weights, projection_bias, output_state_in,
    cell_state_in, input_layer_norm_weights, forget_layer_norm_weights,
    cell_layer_norm_weights, output_layer_norm_weights, cell_clip, projection_clip,
    input_intermediate_scale, forget_intermediate_scale, cell_intermediate_scale,
    output_intermediate_scale, hidden_state_zero_point, hidden_state_scale).To([output_state_out,
    cell_state_out, output])

# Example 1. Layer Norm, Projection.
input0 = {
    input_to_input_weights: [
        64, 77, 89, -102, -115, 13, 25, 38, -51, 64, -102, 89, -77, 64, -51, -64, -51, -38, -25, -13
    ],
    input_to_forget_weights: [
        -77, -13, 38, 25, 115, -64, -25, -51, 38, -102, -51, 38, -64, -51, -77, 38, -51, -77, -64, -64
    ],
    input_to_cell_weights: [
        -51, -38, -25, -13, -64, 64, -25, -38, -25, -77, 77, -13, -51, -38, -89, 89, -115, -64, 102, 77
    ],
    input_to_output_weights: [
        -102, -51, -25, -115, -13, -89, 38, -38, -102, -25, 77, -25, 51, -89, -38, -64, 13, 64, -77, -51
    ],
    input_gate_bias: [644245, 3221226, 4724464, 8160438],
    forget_gate_bias: [2147484, -6442451, -4294968, 2147484],
    cell_gate_bias: [-1073742, 15461883, 5368709, 1717987],
    output_gate_bias: [1073742, -214748, 4294968, 2147484],
    recurrent_to_input_weights: [
        -25, -38, 51, 13, -64, 115, -25, -38, -89, 6, -25, -77
    ],
    recurrent_to_forget_weights: [
        -64, -38, -64, -25, 77, 51, 115, 38, -13, 25, 64, 25
    ],
    recurrent_to_cell_weights: [
        -38, 25, 13, -38, 102, -10, -25, 38, 102, -77, -13, 25
    ],
    recurrent_to_output_weights: [
        38, -13, 13, -25, -64, -89, -25, -77, -13, -51, -89, -25
    ],
    projection_weights: [
        -25, 51, 3, -51, 25, 127, 77, 20, 18, 51, -102, 51
    ],
    projection_bias: [ 0 for _ in range(output_size) ],
    input_layer_norm_weights: [3277, 6553, 9830, 16384],
    forget_layer_norm_weights: [6553, 6553, 13107, 9830],
    cell_layer_norm_weights: [22937, 6553, 9830, 26214],
    output_layer_norm_weights: [19660, 6553, 6553, 16384],
    output_state_in: [ 0 for _ in range(batch_size * output_size) ],
    cell_state_in: [ 0 for _ in range(batch_size * num_units) ],
    cell_to_input_weights: [],
    cell_to_forget_weights: [],
    cell_to_output_weights: [],
}

test_input = [90, 102, 13, 26, 38, 102, 13, 26, 51, 64]

golden_output = [
    127, 127, -108, -67, 127, 127
]

output0 = {
    output_state_out: golden_output,
    cell_state_out: [-14650, 8939, 5771, 6715, -11843, 7847, 1508, 12939],
    output: golden_output,
}

input0[input] = test_input

Example((input0, output0))

# Example 2. CIFG, Layer Norm, Projection.
input0 = {
    input_to_input_weights: [],
    input_to_forget_weights: [
        -77, -13, 38, 25, 115, -64, -25, -51, 38, -102, -51, 38, -64, -51, -77, 38, -51, -77, -64, -64
    ],
    input_to_cell_weights: [
        -51, -38, -25, -13, -64, 64, -25, -38, -25, -77, 77, -13, -51, -38, -89, 89, -115, -64, 102, 77
    ],
    input_to_output_weights: [
        -102, -51, -25, -115, -13, -89, 38, -38, -102, -25, 77, -25, 51, -89, -38, -64, 13, 64, -77, -51
    ],
    input_gate_bias: [],
    forget_gate_bias: [2147484, -6442451, -4294968, 2147484],
    cell_gate_bias: [-1073742, 15461883, 5368709, 1717987],
    output_gate_bias: [1073742, -214748, 4294968, 2147484],
    recurrent_to_input_weights: [],
    recurrent_to_forget_weights: [
        -64, -38, -64, -25, 77, 51, 115, 38, -13, 25, 64, 25
    ],
    recurrent_to_cell_weights: [
        -38, 25, 13, -38, 102, -10, -25, 38, 102, -77, -13, 25
    ],
    recurrent_to_output_weights: [
        38, -13, 13, -25, -64, -89, -25, -77, -13, -51, -89, -25
    ],
    projection_weights: [
        -25, 51, 3, -51, 25, 127, 77, 20, 18, 51, -102, 51
    ],
    projection_bias: [ 0 for _ in range(output_size) ],
    input_layer_norm_weights: [],
    forget_layer_norm_weights: [6553, 6553, 13107, 9830],
    cell_layer_norm_weights: [22937, 6553, 9830, 26214],
    output_layer_norm_weights: [19660, 6553, 6553, 16384],
    output_state_in: [ 0 for _ in range(batch_size * output_size) ],
    cell_state_in: [ 0 for _ in range(batch_size * num_units) ],
    cell_to_input_weights: [],
    cell_to_forget_weights: [],
    cell_to_output_weights: [],
}

test_input = [90, 102, 13, 26, 38, 102, 13, 26, 51, 64]

golden_output = [
    127, 127, 127, -128, 127, 127
]

output0 = {
    output_state_out: golden_output,
    cell_state_out: [-11692, 9960, 5491, 8861, -9422, 7726, 2056, 13149],
    output: golden_output,
}

input0[input] = test_input

Example((input0, output0))
