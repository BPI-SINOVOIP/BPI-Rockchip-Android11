#
# Copyright (C) 2018 The Android Open Source Project
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

# TEST 1: input and filter ranks are not 4
i1 = Input("op1", "TENSOR_FLOAT32", "{1, 3, 3}")
f1 = Parameter("op2", "TENSOR_FLOAT32", "{1, 2, 2}", [.25, .25, .25, .25])
b1 = Parameter("op3", "TENSOR_FLOAT32", "{1}", [0])
o1 = Output("op4", "TENSOR_FLOAT32", "{1, 2, 2}")
Model().Operation("CONV_2D", i1, f1, b1, 0, 0, 0, 0, 1, 1, 0, layout).To(o1)

# Additional data type
quant8 = DataTypeConverter().Identify({
    i1: ("TENSOR_QUANT8_ASYMM", 0.5, 0),
    f1: ("TENSOR_QUANT8_ASYMM", 0.125, 0),
    b1: ("TENSOR_INT32", 0.0625, 0),
    o1: ("TENSOR_QUANT8_ASYMM", 0.125, 0)
})
channelQuant8 = DataTypeConverter().Identify({
    i1: ("TENSOR_QUANT8_ASYMM", 0.5, 0),
    f1: ("TENSOR_QUANT8_SYMM_PER_CHANNEL", 0, 0,
         SymmPerChannelQuantParams(channelDim=0, scales=[0.125])),
    b1: ("TENSOR_INT32", 0.0, 0,
         SymmPerChannelQuantParams(channelDim=0, scales=[0.0625], hide=True)),
    o1: ("TENSOR_QUANT8_ASYMM", 0.125, 0)
})

# Instantiate an example
example = Example({
    i1: [1.0, 1.0, 1.0, 1.0, 0.5, 1.0, 1.0, 1.0, 1.0],
    o1: [.875, .875, .875, .875]
}).ExpectFailure()
