# Using the NN-API Test Generator

## Prerequisites

- Python3
- Numpy

## Writing a Test Specification

You should create new test specs in `nn/runtime/test/specs/<version>/` and name it with `.mod.py` suffix, so that other tools can automatically update the unit tests.

### Specifying Operands

#### Syntax

```
OperandType(name, (type, shape, <optional scale, zero point>), <optional initializer>)
```

For example,

```Python
# p1 is a 2-by-2 fp matrix parameter, with value [1, 2; 3, 4]
p1 = Parameter("param", ("TENSOR_FLOAT32", [2, 2]), [1, 2, 3, 4])

# i1 is a quantized input of shape (2, 256, 256, 3), with scale = 0.5, zero point = 128
i1 = Input("input", ("TENSOR_QUANT8_ASYMM", [2, 256, 256, 3], 0.5, 128))

# p2 is an Int32 scalar with value 1
p2 = Int32Scalar("act", 1)
```

#### OperandType

There are currently 10 operand types supported by the test generator.

- Input
- Output
    * IgnoredOutput, will not compare results in the test
- Parameter
    * Int32Scalar, shorthand for parameter with type INT32
    * Float32Scalar, shorthand for parameter with type FLOAT32
    * Int32Vector, shorthand for 1-D TENSOR_INT32 parameter
    * Float32Vector, shorthand for 1-D TENSOR_FLOAT32 parameter
    * SubgraphReference, shortcut for a SUBGRAPH parameter
- Internal, for model with multiple operations

### Specifying Models

#### Instantiate a model

```Python
# Instantiate a model
model = Model()

# Instantiate a model with a name
model2 = Model("model_name")
```

#### Add an operation

```
model.Operation(optype, i1, i2, ...).To(o1, o2, ...)
```

For example,

```Python
model.Operation("ADD", i1, i2, act).To(o1)
```

#### Use implicit operands

Simple scalar and 1-D vector parameters can now be directly passed to Operation constructor, and test generator will deduce the operand type from the value provided.

```Python
model.Operation("MEAN", i1, [1], 0) # axis = [1], keep_dims = 0
```

Note that, for fp values, the initializer should all be Python fp numbers, e.g. use `1.0` or `1.` instead of `1` for implicit fp operands.

### Specifying Inputs and Expected Outputs

The combination of inputs and expected outputs is called an example for a given model. An example is defined like

```Python
# Example 1, separate dictionary for inputs and outputs
input1 = {
    i1: [1, 2],
    i2: [3, 4]
}
output1 = {o1: [4, 6]}

# Example 2, combined dictionary
example2_values = {
    i1: [5, 6],
    i2: [7, 8],
    o1: [12, 14]
}

# Instantiate an example
Example((input1, output1), example2_values)
```

By default, examples will be attached to the most recent instantiated model. You can explicitly specify the target model, and optionally, the example name by

```Python
Example((input1, output1), example2_values, model=model, name="example_name")
```

### Specifying Variations

You can add variations to the example so that the test generator can automatically create multiple tests. The following variations are supported:

- DefaultVariation, i.e. no variation
- DataTypeConverter
- DataLayoutConverter
- AxisConverter
- RelaxedModeConverter
- ActivationConverter
- AllOutputsAsInternalCoverter

#### DataTypeConverter

Convert input/parameter/output to the specified type, e.g. float32 -> quant8. The target data type for each operand to transform has to be explicitly specified. It is the spec writer's responsibility to ensure such conversion is valid.

```Python
converter = DataTypeConverter(name="variation_name").Identify({
    op1: (target_type, target_scale, target_zero_point),
    op2: (target_type, target_scale, target_zero_point),
    ...
})
```

#### DataLayoutConverter

Convert input/parameter/output between NHWC and NCHW. The caller need to provide a list of target operands to transform, and also the data layout parameter to set.

```Python
converter = DataLayoutConverter(target_data_layout, name="variation_name").Identify(
    [op1, op2, ..., layout_parameter]
)
```

#### AxisConverter

Transpose a certain axis in input/output to target position, and optionally remove some axis. The caller need to provide a list of target operands to transform, and also the axis parameter to set.

```Python
converter = AxisConverter(originalAxis, targetAxis, dimension, drop=[], name="variation_name").Identify(
    [op1, op2, ..., axis_parameter]
)
```

This model variation is for ops that apply calculation along certain axis, such as L2_NORMALIZATION, SOFTMAX, and CHANNEL_SHUFFLE. For example, consider L2_NORMALIZATION with input of shape [2, 3, 4, 5] along the last axis, i.e. axis = -1. The output shape would be the same as input. We can create a new model which will do the calculation along axis 0 by transposing input and output shape to [5, 2, 3, 4] and modify the axis parameter to 0. Such converter can be defined as

```Python
toAxis0 = AxisConverter(-1, 0, 4).Identify([input, output, axis])
```

The target axis can also be negative to test the negative indexing

```Python
toAxis0 = AxisConverter(-1, -4, 4).Identify([input, output, axis])
```

Consider the same L2_NORMALIZATION example, we can also create a new model with input/output of 2D shape [4, 5] by removing the first two dimension. This is essentially doing `new_input = input[0,0,:,:]` in numpy. Such converter can be defined as

```Python
toDim2 = AxisConverter(-1, -1, 4, drop=[0, 1]).Identify([input, output, axis])
```

If transposition and removal are specified at the same time, the converter will do transposition first and then remove the axis. For example, the following converter will result in shape [5, 4] and axis 0.

```Python
toDim2Axis0 = AxisConverter(-1, 2, 4, drop=[0, 1]).Identify([input, output, axis])
```

#### RelaxedModeConverter

Convert the model to enable/disable relaxed computation.

```Python
converter = RelaxedModeConverter(is_relaxed, name="variation_name")
```

#### ActivationConverter

Convert the output by certain activation, the original activation is assumed to be NONE. The caller need to provide a list of target operands to transform,  and also the activation parameter to set.

```Python
converter = ActivationConverter(name="variation_name").Identify(
    [op1, op2, ..., act_parameter]
)
```

#### AllOutputsAsInternalCoverter

Add a dummy ADD operation after each model output to make it as an internal operand. Will skip if the model does not have any output tensor that is compatible with the ADD operation or if the model has more than one operation.

#### Add variation to example

Each example can have multiple groups of variations, and if so, will take the cartesian product of the groups. For example, suppose we declare a model with two groups, and each group has two variations: `[[default, nchw], [default, relaxed, quant8]]`. This will result in 6 examples: `[default, default], [default, relaxed], [default, quant8], [nchw, default], [nchw, relaxed], [nchw, quant8]`.

Use `AddVariations` to add a group of variations to the example

```Python
# Add two groups of variations [default, nchw] and [default, relaxed, quant8]
example.AddVariations(nchw).AddVariations(relaxed, quant8)
```

By default, when you add a group of variation, a unnamed default variation will be automatically included in the list. You can name the default variation by

```Python
example.AddVariations(nchw, defaultName="nhwc").AddVariations(relaxed, quant8)
```

Also, you can choose not to include default by

```Python
# Add two groups of variations [nchw] and [default, relaxed, quant8]
example.AddVariations(nchw, includeDefault=False).AddVariations(relaxed, quant8)
```

The example above will result in 3 examples: `[nchw, default], [nchw, relaxed], [nchw, quant8]`.

#### Default variations

By default, the test generator will apply the following variations automatically.

- **AllTensorsAsInputsConverter:** Convert all constant tensors in the model to model inputs. Will skip if the model does not have any constant tensor, or if the model has more than one operations. If not explicitly disabled, this variation will be automatically applied to all tests.

- **AllInputsAsInternalCoverter:** Add a dummy ADD operation before each model input to make it as an internal operand. Will skip if the model does not have any input tensor that is compatible to the ADD operation, or if the model has more than one operations. If not explicitly disabled, this variation will be automatically applied to all tests.

- **DynamicOutputShapeConverter:** Convert the model to enable dynamic output shape test. If not explicitly disabled, this variation will be automatically applied to all tests introduce in HAL version 1.2 or later.

You can opt-out by invoking the corresponding methods on examples.

```Python
# Disable AllTensorsAsInputsConverter and AllInputsAsInternalCoverter.
example.DisableLifeTimeVariation()

# Disable DynamicOutputShapeConverter.
example.DisableDynamicOutputShapeVariation()
```

You may also specify a certain operand to be input/const-only that `AllInputsAsInternalCoverter` will skip converting this operand.

```Python
# "hash" will be converted to a model input when applying AllTensorsAsInputsConverter,
# but will be skipped when further applying AllInputsAsInternalCoverter.
hash = Parameter("hash", "TENSOR_FLOAT32", "{1, 1}", [0.123]).ShouldNeverBeInternal()
```

#### Some helper functions

The test generator provides several helper functions or shorthands to add commonly used group of variations.

```Python
# Each following group of statements are equivalent

# DataTypeConverter
example.AddVariations(DataTypeConverter().Identify({op1: "TENSOR_FLOAT16", ...}))
example.AddVariations("float16")    # will apply to every TENSOR_FLOAT32 operands

example.AddVariations(DataTypeConverter().Identify({op1: "TENSOR_INT32", ...}))
example.AddVariations("int32")      # will apply to every TENSOR_FLOAT32 operands

# DataLayoutConverter
example.AddVariations(DataLayoutConverter("nchw").Identify(op_list))
example.AddVariations(("nchw", op_list))
example.AddNchw(*op_list)

# AxisConverter
# original axis and dim are deduced from the op_list
example.AddVariations(*[AxisConverter(origin, t, dim).Identify(op_list) for t in targets])
example.AddAxis(targets, *op_list)

example.AddVariations(*[
        AxisConverter(origin, t, dim).Identify(op_list) for t in range(dim)
    ], includeDefault=False)
example.AddAllPositiveAxis(*op_list)

example.AddVariations(*[
        AxisConverter(origin, t, dim).Identify(op_list) for t in range(-dim, dim)
    ], includeDefault=False)
example.AddAllAxis(*op_list)

drop = list(range(dim))
drop.pop(origin)
example.AddVariations(*[
    AxisConverter(origin, origin, dim, drop[0:(dim-i)]).Identify(op_list) for i in dims])
example.AddDims(dims, *op_list)

example.AddVariations(*[
    AxisConverter(origin, origin, dim, drop[0:i]).Identify(op_list) for i in range(dim)])
example.AddAllDims(dims, *op_list)

example.AddVariations(*[
        AxisConverter(origin, j, dim, range(i)).Identify(op_list) \
                for i in range(dim) for j in range(i, dim)
    ], includeDefault=False)
example.AddAllDimsAndPositiveAxis(dims, *op_list)

example.AddVariations(*[
        AxisConverter(origin, k, dim, range(i)).Identify(op_list) \
                for i in range(dim) for j in range(i, dim) for k in [j, j - dim]
    ], includeDefault=False)
example.AddAllDimsAndAxis(dims, *op_list)

# RelaxedModeConverter
example.Addvariations(RelaxedModeConverter(True))
example.AddVariations("relaxed")
example.AddRelaxed()

# ActivationConverter
example.AddVariations(ActivationConverter("relu").Identify(op_list))
example.AddVariations(("relu", op_list))
example.AddRelu(*op_list)

example.AddVariations(
    ActivationConverter("relu").Identify(op_list),
    ActivationConverter("relu1").Identify(op_list),
    ActivationConverter("relu6").Identify(op_list))
example.AddVariations(
    ("relu", op_list),
    ("relu1", op_list),
    ("relu6", op_list))
example.AddAllActivations(*op_list)
```

#### Specifying SUBGRAPH conversions

Converters that support nested control flow models accept the following syntax:

```
converter = DataTypeConverter().Identify({
    ...
    subgraphOperand: DataTypeConverter().Identify({
        ...
    }),
    ...
})
```

### Specifying the Model Version

If not explicitly specified, the minimal required HAL version will be inferred from the path, e.g. the models defined in `nn/runtime/test/specs/V1_0/add.mod.py` will all have version `V1_0`. However there are several exceptions that a certain operation is under-tested in previous version and more tests are added in a later version. In this case, two methods are provided to set the version manually.

#### Set the version when creating the model

Use `IntroducedIn` to set the version of a model. All variations of the model will have the same version.

```Python
model_V1_0 = Model().IntroducedIn("V1_0")
...
# All variations of model_V1_0 will have the same version V1_0.
Example(example, model=model_V1_0).AddVariations(var0, var1, ...)
```

#### Set the version overrides

Use `Example.SetVersion` to override the model version for specific tests. The target tests are specified by names. This method can also override the version specified by `IntroducedIn`.

```Python
Example.SetVersion(<version>, testName0, testName1, ...)
```

This is useful when only a subset of variations has a different version.

### Specifying model inputs and outputs

Use `Model.IdentifyInputs` and `Model.IdentifyOutputs` to explicitly specify
model inputs and outputs. This is particularly useful for models referenced by
IF and WHILE operations.

```Python
DataType = ["TENSOR_INT32", [1]]
BoolType = ["TENSOR_BOOL8", [1]]

def MakeConditionModel():
  a = Input("a", DataType)
  b = Input("b", DataType)
  out = Output("out", BoolType)
  model = Model()
  model.IdentifyInputs(a, b)  # "a" is unused by the model.
  model.IdentifyOutputs(out)
  model.Operation("LESS", b, [10]).To(out)
  return model

def MakeBodyModel():
  a = Input("a", DataType)
  b = Input("b", DataType)
  a_out = Output("a_out", DataType)
  b_out = Output("b_out", DataType)
  model = Model()
  model.IdentifyInputs(a, b)  # The order is the same as in the WHILE operation.
  model.IdentifyOutputs(a_out, b_out)
  model.Operation("SUB", b, a, 0).To(a_out)
  model.Operation("ADD", b, [1], 0).To(b_out)
  return model

a = Input("a", DataType)
a_out = Output("a_out", DataType)
cond = MakeConditionModel()
body = MakeBodyModel()
b_init = [1]
Model().Operation("WHILE", cond, body, a, b_init).To(a_out)
```

### Creating negative tests

Negative test, also known as validation test, is a testing method that supplies invalid model or request, and expects the target framework or driver to fail gracefully. You can use `ExpectFailure` to tag a example as invalid.

```Python
Example.ExpectFailure()
```

### A Complete Example

```Python
# Declare input, output, and parameters
i1 = Input("op1", ("TENSOR_FLOAT32", [1, 3, 4, 1]))
f1 = Parameter("op2", ("TENSOR_FLOAT32", [1, 3, 3, 1]), [1, 4, 7, 2, 5, 8, 3, 6, 9])
b1 = Parameter("op3", ("TENSOR_FLOAT32", [1]), [-200])
act = Int32Scalar("act", 0)
o1 = Output("op4", ("TENSOR_FLOAT32", [1, 3, 4, 1]))

# Instantiate a model and add CONV_2D operation
# Use implicit parameter for implicit padding and strides
Model().Operation("CONV_2D", i1, f1, b1, 1, 1, 1, act, layout).To(o1)

# Additional data type
quant8 = DataTypeConverter().Identify({
    i1: ("TENSOR_QUANT8_ASYMM", 0.5, 127),
    f1: ("TENSOR_QUANT8_ASYMM", 0.5, 127),
    b1: ("TENSOR_INT32", 0.25, 0),
    o1: ("TENSOR_QUANT8_ASYMM", 1.0, 50)
})

# Instantiate an example
example = Example({
    i1: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12],
    o1: [0, 0, 0, 0, 35, 112, 157, 0, 0, 34, 61, 0]
})

# Only use NCHW data layout
example.AddNchw(i1, f1, o1, layout, includeDefault=False)

# Add two more groups of variations
example.AddInput(f1, b1).AddVariations("relaxed", quant8).AddAllActivations(o1, act)

# The following variations are added implicitly.
# example.AddVariations(AllTensorsAsInputsConverter())
# example.AddVariations(AllInputsAsInternalCoverter())

# The following variation is added implicitly if this test is introduced in v1.2 or later.
# example.AddVariations(DynamicOutputShapeConverter())
```

The spec above will result in 96 tests if introduced in v1.0 or v1.1, and 192 tests if introduced in v1.2 or later.

## Generate Tests

Once you have your model ready, run

```
$ANDROID_BUILD_TOP/frameworks/ml/nn/runtime/test/specs/generate_all_tests.sh
```

It will update all CTS and VTS tests based on spec files in `nn/runtime/test/specs/V1_*/*`.

Rebuild with mma afterwards.
