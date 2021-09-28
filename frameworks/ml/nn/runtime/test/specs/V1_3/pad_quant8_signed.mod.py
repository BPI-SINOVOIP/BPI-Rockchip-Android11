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

import numpy as np

input0 = Input("input0", "TENSOR_FLOAT32", "{1, 1, 2, 3}")
paddings = Parameter("paddings", "TENSOR_INT32", "{4, 2}", [1, 2,
                                                            3, 4,
                                                            3, 3,
                                                            2, 1])
output0 = Output("output0", "TENSOR_FLOAT32", "{4, 8, 8, 6}")

model = Model().Operation("PAD", input0, paddings).To(output0)

quant8_signed = DataTypeConverter().Identify({
    input0: ("TENSOR_QUANT8_ASYMM_SIGNED", 2.3, -128),
    output0: ("TENSOR_QUANT8_ASYMM_SIGNED", 2.3, -128),
})

Example({
    input0: [1.0, 2.0, 3.0,
             4.0, 5.0, 6.0],
    output0: np.pad([[[[1.0, 2.0, 3.0],
                       [4.0, 5.0, 6.0]]]],
                    [[1, 2],
                     [3, 4],
                     [3, 3],
                     [2, 1]],
                    "constant").flatten().tolist(),
}).AddVariations(quant8_signed, includeDefault=False)

#######################################################

input0 = Input("input0", "TENSOR_QUANT8_ASYMM_SIGNED", "{3}, 2.3, -128")
paddings = Parameter("paddings", "TENSOR_INT32", "{1, 2}", [3, 1])
output0 = Output("output0", "TENSOR_QUANT8_ASYMM_SIGNED", "{7}, 2.3, -128")

model = Model().Operation("PAD", input0, paddings).To(output0)

Example({
    input0: [-127, -126, -125],
    output0: [-128, -128, -128, -127, -126, -125, -128],
})

#######################################################

input0 = Input("input0", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 2, 3, 1}, 2.3, -128")
paddings = Parameter("paddings", "TENSOR_INT32", "{4, 2}", [0, 0,
                                                            0, 2,
                                                            1, 3,
                                                            0, 0])
output0 = Output("output0", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 4, 7, 1}, 2.3, -128")

model = Model().Operation("PAD", input0, paddings).To(output0)

Example({
    input0: [-127, -126, -125,
             -124, -123, -122],
    output0: [-128, -127, -126, -125, -128, -128, -128,
              -128, -124, -123, -122, -128, -128, -128,
              -128, -128, -128, -128, -128, -128, -128,
              -128, -128, -128, -128, -128, -128, -128],
})

#######################################################

# Quantized PAD with non-zero zeroPoint is supported since 1.2.
# See http://b/132112227.

input0 = Input("input0", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 2, 3, 1}, 2.3, -119")
paddings = Parameter("paddings", "TENSOR_INT32", "{4, 2}", [0, 0,
                                                            0, 2,
                                                            1, 3,
                                                            0, 0])
output0 = Output("output0", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 4, 7, 1}, 2.3, -119")

model = Model().Operation("PAD", input0, paddings).To(output0)

Example({
    input0: [-127, -126, -125,
             -124, -123, -122],
    output0: [-119, -127, -126, -125, -119, -119, -119,
              -119, -124, -123, -122, -119, -119, -119,
              -119, -119, -119, -119, -119, -119, -119,
              -119, -119, -119, -119, -119, -119, -119],
})

#######################################################

input0 = Input("input0", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 2, 3, 1}, 2.3, -124")
paddings = Parameter("paddings", "TENSOR_INT32", "{4, 2}", [0, 0,
                                                            0, 2,
                                                            1, 3,
                                                            0, 0])
pad_value = Int32Scalar("pad_value", 9)
output0 = Output("output0", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 4, 7, 1}, 2.3, -124")

model = Model().Operation("PAD_V2", input0, paddings, pad_value).To(output0)

Example(({
    input0: [-127, -126, -125,
             -124, -123, -122],
}, {
    output0: [9, -127, -126, -125, 9, 9, 9,
              9, -124, -123, -122, 9, 9, 9,
              9, 9, 9, 9, 9, 9, 9,
              9, 9, 9, 9, 9, 9, 9],
}))

#######################################################

input0 = Input("input0", "TENSOR_QUANT8_ASYMM_SIGNED", "{1, 1, 2, 3}, 2.3, -124")
paddings = Parameter("paddings", "TENSOR_INT32", "{4, 2}", [1, 2,
                                                            3, 4,
                                                            3, 3,
                                                            2, 1])
pad_value = Int32Scalar("pad_value", -125)
output0 = Output("output0", "TENSOR_QUANT8_ASYMM_SIGNED", "{4, 8, 8, 6}, 2.3, -124")

model = Model().Operation("PAD_V2", input0, paddings, pad_value).To(output0)

Example({
    input0: [-127, -126, -125,
             -124, -123, -122],
    output0: np.pad([[[[-127, -126, -125],
                       [-124, -123, -122]]]],
                    [[1, 2],
                     [3, 4],
                     [3, 3],
                     [2, 1]],
                    "constant",
                    constant_values=-125).flatten().tolist(),
})

#######################################################

input0 = Input("input0", "TENSOR_QUANT8_ASYMM_SIGNED", "{3}, 2.3, -124")
paddings = Parameter("paddings", "TENSOR_INT32", "{1, 2}", [3, 1])
pad_value = Int32Scalar("pad_value", 9)
output0 = Output("output0", "TENSOR_QUANT8_ASYMM_SIGNED", "{7}, 2.3, -124")

model = Model().Operation("PAD_V2", input0, paddings, pad_value).To(output0)

Example({
    input0: [-127, -126, -125],
    output0: [9, 9, 9, -127, -126, -125, 9],
})
