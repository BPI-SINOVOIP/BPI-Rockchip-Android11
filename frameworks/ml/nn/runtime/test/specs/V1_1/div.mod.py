# model
model = Model()
i1 = Input("op1", "TENSOR_FLOAT32", "{1, 2, 2, 1}")
i2 = Input("op2", "TENSOR_FLOAT32", "{1, 2, 2, 1}")
act = Int32Scalar("act", 0) # an int32_t scalar fuse_activation
i3 = Output("op3", "TENSOR_FLOAT32", "{1, 2, 2, 1}")
model = model.Operation("DIV", i1, i2, act).To(i3)

# Example 1. Input in operand 0,
input0 = {i1: # input 0
          [2.0, -4.0, 8.0, -16.0],
          i2: # input 1
          [2.0, -2.0, -4.0, 4.0]}

output0 = {i3: # output 0
           [1.0, 2.0, -2.0, -4.0]}

# Instantiate an example
Example((input0, output0))


# Test DIV by zero.
# It is undefined behavior. The output is ignored and we only require the drivers to not crash.
input0 = Input("input0", "TENSOR_FLOAT32", "{1}")
input1 = Input("input1", "TENSOR_FLOAT32", "{1}")
output = IgnoredOutput("output", "TENSOR_FLOAT32", "{1}")
model = Model("by_zero").Operation("DIV", input0, input1, 0).To(output)
Example({
    input0: [1],
    input1: [0],
    output: [0],
}).AddVariations("relaxed")
