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

layout = BoolScalar("layout", False)  # NHWC

# This test makes sure that executing DEPTHWISE_CONV_2D results in a failure when
# the first dimension of the filter tensor is not 1.

i1 = Input("op1", "TENSOR_FLOAT32", "{1, 3, 3, 2}")
f1 = Parameter("op2", "TENSOR_FLOAT32", "{2, 2, 2, 4}", [0.] * (2 * 2 * 2 * 4))
b1 = Parameter("op3", "TENSOR_FLOAT32", "{4}", [0.] * 4)
o1 = Output("op4", "TENSOR_FLOAT32", "{1, 2, 2, 4}")
Model().Operation("DEPTHWISE_CONV_2D", i1, f1, b1, 0, 0, 0, 0, 1, 1, 2, 0, layout).To(o1)

# Additional data type
quant8 = DataTypeConverter().Identify({
    i1: ("TENSOR_QUANT8_ASYMM", 0.5, 0),
    f1: ("TENSOR_QUANT8_ASYMM", 0.01, 0),
    b1: ("TENSOR_INT32", 0.005, 0),
    o1: ("TENSOR_QUANT8_ASYMM", 0.1, 0)
})
channelQuant8 = DataTypeConverter().Identify({
    i1: ("TENSOR_QUANT8_ASYMM", 0.5, 0),
    f1: ("TENSOR_QUANT8_SYMM_PER_CHANNEL", 0, 0,
         SymmPerChannelQuantParams(channelDim=3, scales=[0.01, 0.005, 0.01, 0.005])),
    b1: ("TENSOR_INT32", 0.0, 0,
         SymmPerChannelQuantParams(channelDim=0, scales=[0.005, 0.0025, 0.005, 0.0025], hide=True)),
    o1: ("TENSOR_QUANT8_ASYMM", 0.1, 0)
})

# Instantiate an example
example = Example({
    i1: [0.] * (1 * 3 * 3 * 2),
    o1: [0.] * (1 * 2 * 2 * 4)
}).AddNchw(i1, o1, layout).AddVariations("relaxed", "float16", channelQuant8,
                                         quant8).ExpectFailure()
