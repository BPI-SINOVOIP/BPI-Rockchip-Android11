#!/usr/bin/python3

# Copyright 2017, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""NN model compiler

Contain classes definition and utilify functions for compiling models and
examples into NDK-based CTS and VTS unit tests.

Used by example_generator.py and spec_visualizer.py
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
import copy
from functools import reduce
import argparse
import io
import itertools
import os
import re
import sys
import traceback
import numpy as np

def GetJointStr(l, sep=", ", method=str):
    return sep.join([method(i) for i in l])

# Print in C float literal format
def PrettyPrintAsFloat(x):
    s = str(float(x))
    if s.find(".") >= 0 or s.find("e") >= 0:
        return s + "f"
    else:
        return s + ".0f"

# Transform from original type to float32
def Dequantize(v, ty):
    v -= ty.zeroPoint
    if ty.scale != 0:
        v *= ty.scale
    if isinstance(ty.extraParams, SymmPerChannelQuantParams):
        v *= ty.extraParams.GetScalesBroadcastArray(ty.dimensions)
    return v

# Transform float32 to target data type
def Quantize(v, ty):
    if ty.scale != 0:
        v /= ty.scale
    if isinstance(ty.extraParams, SymmPerChannelQuantParams):
        v = v / ty.extraParams.GetScalesBroadcastArray(ty.dimensions)
    v += ty.zeroPoint
    if not ty.IsFloat():
        v = np.round(v)
        v = v.astype(int)

    if ty.type == "TENSOR_QUANT8_ASYMM":
        v = np.minimum(np.maximum(v, 0), 255)
    elif ty.type == "TENSOR_QUANT16_ASYMM":
        v = np.minimum(np.maximum(v, 0), 65535)
    elif ty.type == "TENSOR_QUANT8_SYMM_PER_CHANNEL":
        v = np.minimum(np.maximum(v, -127), 127)
    elif ty.type == "UINT32":
        v = np.maximum(v, 0)
    elif ty.type == "TENSOR_QUANT8_ASYMM_SIGNED":
        v = np.minimum(np.maximum(v, -128), 127)
    return v

# Tracking objects inside a model with a unique name
class NamedObject:
    existingNames = set()

    def __init__(self, *args, sep="_", showZero=False, startsFrom=0, skipRenaming=False):
        name = GetJointStr([i for i in args if i is not None and i != ""], sep=sep)
        if skipRenaming:
            self.name = name
            return
        # make the name unique by renaming with a suffix number
        uniqueName = name if showZero is False else name + sep + str(startsFrom)
        while uniqueName in self.__class__.existingNames:
            startsFrom += 1
            uniqueName = name + sep + str(startsFrom)
        self.__class__.existingNames.add(uniqueName)
        self.name = uniqueName

    def __str__(self):
        return self.name
    __repr__ = __str__

    # Since names are unique, objects with the same name are considered equal
    def __eq__(self, other):
        return isinstance(other, NamedObject) and self.name == other.name

    def __ne__(self, other):
        return not self.__eq__(other)

    def __hash__(self):
        return hash(self.name)

    def __lt__(self, other):
        return self.name < other.name

# Types, operands should all have a unique name since they share the same namespace
class NamedVariable(NamedObject):
    existingNames = set()
    def __init__(self, *args, sep="_", showZero=False, startsFrom=0, skipRenaming=False):
        NamedObject.__init__(self, *args, sep=sep, showZero=showZero,
            startsFrom=startsFrom, skipRenaming=skipRenaming)

# Global variables in the spec namespace such as CreateModel, is_ignored, and examples
class GlobalVariable(NamedVariable):
    def __init__(self, *args, skipRenaming=False):
        NamedObject.__init__(self, *args, startsFrom=1, skipRenaming=skipRenaming)

# Each test should have a unique name, but will not conflict with variables
class NamedTest(NamedObject):
    existingNames = set()
    def __init__(self, *args, startsFrom=0, skipRenaming=False):
        NamedObject.__init__(self, *args, startsFrom=1, skipRenaming=skipRenaming)

class Type(NamedVariable):
    typesMap = dict()
    typeLookup = {
        "INT32": "int32_t",
        "UINT32": "uint32_t",
        "FLOAT32": "float",
        "FLOAT16": "_Float16",
        "TENSOR_INT32": "int32_t",
        "TENSOR_FLOAT16": "_Float16",
        "TENSOR_FLOAT32": "float",
        "TENSOR_QUANT8_ASYMM": "uint8_t",
        "TENSOR_QUANT8_SYMM": "int8_t",
        "BOOL": "bool8",
        "TENSOR_QUANT16_ASYMM": "uint16_t",
        "TENSOR_QUANT16_SYMM": "int16_t",
        "TENSOR_BOOL8": "bool8",
        "TENSOR_QUANT8_SYMM_PER_CHANNEL": "int8_t",
        "TENSOR_QUANT8_ASYMM_SIGNED": "int8_t",
    #     "OEM_SCALAR": this is service-defined.
        "TENSOR_OEM_BYTE": "uint8_t",
        "SUBGRAPH": "uint32_t",  # Index into TestModel::referenced.
    }

    # types are named as "type0", "type1", ...
    def __init__(self, vt, dimensions, scale, zeroPoint, name="type", skipRenaming=False,
                 extraParams=None):
        NamedVariable.__init__(self, name, sep="", showZero=True, skipRenaming=skipRenaming)
        self.type = vt
        self.dimensions = dimensions
        self.scale = float(scale)
        self.zeroPoint = int(zeroPoint)
        self.extraParams = extraParams

    # Factory for Type object, only create a new Type if requested type does
    # not have a match with all existing types
    @staticmethod
    def GetType(vt, dimensions, scale=0, zeroPoint=0, extraParams=None):
        assert isinstance(dimensions, (list, tuple)), \
            'dimensions must be a list or tuple, got {}'.format(type(dimensions))
        key = ",".join([vt, str(dimensions), str(scale), str(zeroPoint), str(extraParams)])
        if key not in Type.typesMap:
            Type.typesMap[key] = Type(vt, dimensions, scale, zeroPoint, extraParams=extraParams)
        return Type.typesMap[key]

    @staticmethod
    def GetAllTypes():
        # sort to ensure a stable order when dumping the code
        return sorted(Type.typesMap.values())

    # For backward-compatibility
    @staticmethod
    def GetTypeFromString(vt, shape, extraParams=None):
        dimensions, scale, zeroPoint = Type.GetParsedShape(shape)
        scale = float(scale)
        zeroPoint = int(zeroPoint)
        return Type.GetType(vt, dimensions, scale, zeroPoint, extraParams)

    # For backward-compatibility
    @staticmethod
    def GetParsedShape(shape):
        # Parse shape
        if (shape != "" and shape != "{}"):
            left, sep, right = shape.partition('{')
            real_shape, sep, right = right.partition('}')
            shape = [int(x) for x in real_shape.split(",")]
            # left now looks like "0.0f, 127.5f, "
            scale, sep, zero_point = right.rpartition(',')
            if scale == "":
                if zero_point == "":
                    return shape, "0", "0"
                return shape, zero_point, "0"
            left, sep, scale = scale.partition(',')
            return shape, scale.replace("f", ""), zero_point
        else:
            return [], "0", "0"

    def GetNumberOfElements(self):
        return reduce(lambda x,y: x*y, self.dimensions, 1)

    def GetCppTypeString(self):
        return Type.typeLookup[self.type]

    def IsFloat(self):
        return self.GetCppTypeString() in ["float", "_Float16"]

    def IsBool(self):
        return self.GetCppTypeString() == "bool8"

    def IsScalar(self):
        return not self.type.startswith("TENSOR_")

    def GetElementByteSize(self):
        cppTypeString = self.GetCppTypeString()
        if cppTypeString in ["uint8_t", "int8_t", "bool8"]:
            return 1
        elif cppTypeString in ["int16_t", "uint16_t", "_Float16"]:
            return 2
        else:
            return 4

    def GetByteSize(self):
        return self.GetElementByteSize() * self.GetNumberOfElements()

    def GetDimensionsString(self):
        return "{" + GetJointStr(self.dimensions) + "}"

    def GetSignatureTuple(self):
        return (self.type, self.dimensions, self.scale, self.zeroPoint)

    def ToUnspecifiedDim(self):
        return Type.GetType(self.type, [0] * len(self.dimensions), self.scale, self.zeroPoint)

# To track implicitly convertible parameter types
class ImplicitParameter():
    @staticmethod
    def ImplicitConvertion(value):
        if isinstance(value, Operand):
            return value
        for implicitType in ImplicitParameter.__subclasses__():
            if implicitType.IsCompatible(value):
                return implicitType("param", value)
        assert False, "%s not supported for implicit parameter"%value


# ExtraParams with per-channel quantization.
class SymmPerChannelQuantParams():
    def __init__(self, channelDim, scales, hide = False):
        self.channelDim = channelDim
        self.scales = scales
        self.hide = hide

    def GetScalesBroadcastArray(self, dimensions):
        bshape = [1] * len(dimensions)
        bshape[self.channelDim] = len(self.scales)
        return np.array(self.scales).reshape(bshape)

    def GetConstructor(self):
        return "SymmPerChannelQuantParams({%s},%d)" % (
            ", ".join(str(x) + "f" for x in self.scales), self.channelDim)

    def GetVtsSetter(self):
        return "channelQuant"

    def GetVtsConstructor(self):
        return "SymmPerChannelQuantParams{.scales={%s}, .channelDim=%d}" % (
            ", ".join(str(x) + "f" for x in self.scales), self.channelDim)


# An operand that can be fed into operations. Also, an operand is always
# declared before operations.
class Operand(NamedVariable):

    def __init__(self, name, opType, value, backward=None, skipRenaming=False, extraParams=None):
        NamedVariable.__init__(self, name, sep="", skipRenaming=skipRenaming)
        if type(opType) is str:
            self.type = Type.GetTypeFromString(opType, value, extraParams)
            value = backward
        else:
            self.type = Type.GetType(*opType, extraParams=extraParams)
        self.SetValue(value)
        self.lifetime = "TEMPORARY_VARIABLE"
        self.model_index = None
        self.ins = []
        self.outs = []
        self.mayBeInternal = True

    def SetValue(self, value):
        self.value = value if type(value) is list or type(value) is tuple or value is None \
                     else [value]
        return self

    def SetValueFromNumpy(self, value):
        self.value = value.flatten().tolist()
        return self

    def GetValueAsNumpy(self):
        return np.array(self.value).reshape(self.type.dimensions)

    # Print value as cpp-style list initialization
    def GetListInitialization(self):
        if self.value is None:
            return "{}"
        elif self.type.IsFloat():
            return "{%s}"%(GetJointStr(self.value, method=PrettyPrintAsFloat))
        elif self.type.IsBool():
            return "{%s}"%(GetJointStr(self.value, method=lambda v: "true" if v else "false"))
        else:
            return "{%s}"%(GetJointStr(self.value, method=lambda x: str(int(x))))

    def ToUnspecifiedDim(self):
        self.dimensions = self.type.dimensions
        self.type = self.type.ToUnspecifiedDim()

    def ConvertTo(self, DerivedClass, name=None):
        assert issubclass(DerivedClass, Operand)
        name = self.name if name is None else name
        newop = DerivedClass(name, self.type.GetSignatureTuple(), skipRenaming=True,
                             extraParams=self.type.extraParams)
        if not issubclass(DerivedClass, Internal):
            newop.SetValue(self.value)
        if not self.mayBeInternal:
            assert not issubclass(DerivedClass, Internal)
            newop.ShouldNeverBeInternal()
        return newop

    def ShouldNeverBeInternal(self):
        self.mayBeInternal = False
        return self

# Base class of user-defined input/output operand
class InOut(Operand):

    def __init__(self, name, opType, backward=None, skipRenaming=False, extraParams=None):
        Operand.__init__(self, name, opType, backward, None, skipRenaming=skipRenaming, extraParams=extraParams)
        self.lifetime = "SUBGRAPH_INPUT"
        self.index = 0

    def Feed(self, value):
        self.SetValue(value[self] if type(value) is dict else value)
        return self

# A user-declared input operand
class Input(InOut):
    def __init__(self, name, opType, backward=None, skipRenaming=False, extraParams=None):
        InOut.__init__(self, name, opType, backward, skipRenaming=skipRenaming, extraParams=extraParams)
        self.lifetime = "SUBGRAPH_INPUT"

# A user-declared output operand
class Output(InOut):
    def __init__(self, name, opType, backward=None, skipRenaming=False, extraParams=None):
        InOut.__init__(self, name, opType, backward, skipRenaming=skipRenaming, extraParams=extraParams)
        self.lifetime = "SUBGRAPH_OUTPUT"

# An output that we don't want to compare the results
class IgnoredOutput(Output):
    def __init__(self, name, opType, backward=None, skipRenaming=False, extraParams=None):
        Output.__init__(self, name, opType, backward, skipRenaming=skipRenaming, extraParams=extraParams)
        self.lifetime = "SUBGRAPH_OUTPUT"
    def Feed(self, value):
        numElements = reduce(lambda x,y: x*y, self.type.dimensions, 1)
        self.value = [0 for x in range(numElements)]
        return self

# An explicitly declared parameter
class Parameter(Operand):
    def __init__(self, name, opType, value, backward=None, skipRenaming=False, extraParams=None):
        Operand.__init__(self, name, opType, value, backward, skipRenaming=skipRenaming,
                         extraParams=extraParams)
        self.initializer = NamedVariable(str(self) + "_init")
        if value is None:
            self.lifetime = "NO_VALUE"
        elif Configuration.useSHM():
            self.lifetime = "CONSTANT_REFERENCE"
        else:
            self.lifetime = "CONSTANT_COPY"

# A shortcut for parameters of INT32
class Int32Scalar(Parameter, ImplicitParameter):
    def __init__(self, name, value):
        Parameter.__init__(self, name, ("INT32", []), int(value))
    @staticmethod
    def IsCompatible(value):
        return type(value) is int

# A shortcut for parameters of FLOAT16
class Float16Scalar(Parameter, ImplicitParameter):
    def __init__(self, name, value):
        Parameter.__init__(self, name, ("FLOAT16", []), float(value))
    @staticmethod
    def IsCompatible(value):
        return False

# A shortcut for parameters of FLOAT32
class Float32Scalar(Parameter, ImplicitParameter):
    def __init__(self, name, value):
        Parameter.__init__(self, name, ("FLOAT32", []), float(value))
    @staticmethod
    def IsCompatible(value):
        return type(value) is float

# A shortcut for parameters of BOOL
class BoolScalar(Parameter, ImplicitParameter):
    def __init__(self, name, value):
        Parameter.__init__(self, name, ("BOOL", []), bool(value))
    @staticmethod
    def IsCompatible(value):
        return type(value) is bool

# A shortcut for parameter of 1-D TENSOR_INT32
class Int32Vector(Parameter, ImplicitParameter):
    def __init__(self, name, value):
        Parameter.__init__(self, name, ("TENSOR_INT32", [len(value)]), [int(v) for v in value])
    @staticmethod
    def IsCompatible(value):
        if type(value) is not list and type(value) is not tuple:
            return False
        return all(type(i) is int for i in value)

# A shortcut for parameter of 1-D TENSOR_FLOAT32
class Float32Vector(Parameter, ImplicitParameter):
    def __init__(self, name, value):
        Parameter.__init__(self, name, ("TENSOR_FLOAT32", [len(value)]), [float(v) for v in value])
    @staticmethod
    def IsCompatible(value):
        if type(value) is not list and type(value) is not tuple:
            return False
        return all(type(i) is float for i in value)

# A shortcut for a SUBGRAPH parameter
class SubgraphReference(Parameter, ImplicitParameter):
    def __init__(self, name, model):
        Parameter.__init__(self, name, ("SUBGRAPH", []), model)
        self.lifetime = "SUBGRAPH"
        if model.name is None:
            model.name = name
    @staticmethod
    def IsCompatible(value):
        return type(value) is Model

# An explicitly declared intermediate result
class Internal(Operand):
    def __init__(self, name, opType, backward=None, skipRenaming=False, extraParams=None):
        Operand.__init__(self, name, opType, backward, None, skipRenaming=skipRenaming,
                         extraParams=extraParams)
        self.lifetime = "TEMPORARY_VARIABLE"

# An operation in a model, does not need a name
class Operation:

    def __init__(self, optype, ins, outs):
        self.optype = optype
        self.SetInputs(ins)
        self.SetOutputs(outs)

    # for the ease of debugging
    def __str__(self):
        insString = GetJointStr(self.ins)
        outsString = GetJointStr(self.outs)
        return "Operation %s: [%s] -> [%s]"%(self.optype, insString, outsString)
    __repr__ = __str__

    def SetInputs(self, ins):
        self.ins = [ImplicitParameter.ImplicitConvertion(i) for i in ins]
        return self

    def SetOutputs(self, outs):
        self.outs = list(outs)
        return self

# Main interface
class Model:
    models = list()

    def __init__(self, name=None):
        self.name = name
        self.operations = []
        self.operands = []
        self.isRelaxed = False
        self.compiled = False
        self.dumped = False
        self.version = FileNames.version
        self.referenced_models = None
        Model.models.append(self)

    def WithSuffix(self, *args):
        self.createFunctionName = GlobalVariable("CreateModel", self.name, *args)
        self.createTestFunctionName = GlobalVariable("createTestModel", self.name, *args)
        self.isIgnoredFunctionName = GlobalVariable("is_ignored", self.name, *args)
        return self

    def AddOperand(self, operand):
        if operand not in self.operands:
            self.operands.append(operand)
        return self

    # Makes sure the model contains all (and only) the given inputs in the
    # specified order.
    def IdentifyInputs(self, *args):
        for arg in args:
            self.AddOperand(arg)
        inputs = tuple(self.GetInputs())
        assert inputs == args, '{} vs {}'.format(inputs, args)
        return self

    # Makes sure the model contains all (and only) the given outputs in the
    # specified order.
    def IdentifyOutputs(self, *args):
        for arg in args:
            self.AddOperand(arg)
        outputs = tuple(self.GetOutputs())
        assert outputs == args, '{} vs {}'.format(outputs, args)
        return self

    def AddOperation(self, operation):
        self.operations.append(operation)
        for i in operation.ins:
            self.AddOperand(i)
        for o in operation.outs:
            self.AddOperand(o)
        return self

    def Operation(self, op_name, *args):
        return self.AddOperation(Operation(op_name, args, []))

    def To(self, *args):
        assert len(self.operations) > 0
        if type(args[0]) is tuple or type(args[0]) is list:
            outs = args[0]
        else:
            outs = args
        self.operations[-1].SetOutputs(outs)
        for o in outs:
            self.AddOperand(o)
        return self

    def RelaxedExecution(self, isRelaxed):
        self.isRelaxed = isRelaxed
        return self

    # Sets the version of the model in compliance tests. Set to None to disable the test.
    def IntroducedIn(self, ver):
        self.version = ver
        return self

    def GetTypes(self):
        return sorted(list(set(op.type for op in self.operands)))

    def GetInputs(self):
        return [i for i in self.operands if isinstance(i, Input)]

    def GetOutputs(self):
        return [o for o in self.operands if isinstance(o, Output)]

    def GetInputsIndex(self):
        return [i for i,op in enumerate(self.operands) if isinstance(op, Input)]

    def GetOutputsIndex(self):
        return [o for o,op in enumerate(self.operands) if isinstance(op, Output)]

    def GetIndexOfOperands(self, operands):
        return [self.operands.index(i) for i in operands]

    def GetIgnoredOutputs(self):
        return [o for o in self.operands if isinstance(o, IgnoredOutput)]

    def GetParameters(self):
        return [p for p in self.operands if isinstance(p, Parameter)]

    def GetReferencedModels(self):
        assert self.compiled
        return self.referenced_models

    def GetEquivalentOperands(self, targets):
        return [self.operands[self.operands.index(t)] for t in targets]

    def UpdateEquivalentOperands(self, targets):
        for t in targets:
            self.operands[self.operands.index(t)] = t
        return self

    def SetOperandIndex(self):
        for ind, i in enumerate(self.GetInputs()):
            i.index = ind
        for ind, o in enumerate(self.GetOutputs()):
            o.index = ind
        for ind, op in enumerate(self.operands):
            op.model_index = ind
        return self

    def SetOperandInsAndOuts(self):
        for op in self.operands:
            op.ins = list()
            op.outs = list()
        for op in self.operations:
            op.ins = self.GetEquivalentOperands(op.ins)
            op.outs = self.GetEquivalentOperands(op.outs)
            for i in op.ins:
                i.outs.append(op)
            for o in op.outs:
                o.ins.append(op)
        return self

    def TopologicalSortHelper(self, op, deps, visited):
        if op in visited:
            assert op not in deps, "Cycle detected in the graph"
        else:
            visited.add(op)
            for i in deps[op]:
                self.TopologicalSortHelper(i, deps, visited)
            self.operations.append(op)
            deps.pop(op)

    # Topological sort of the operations, and detect if there is a cycle is the graph
    def TopologicalSort(self):
        deps = {op: list() for op in self.operations}
        [deps[o].append(i) for op in self.operands for o in op.outs for i in op.ins]
        operations = self.operations.copy()
        self.operations = []
        visited = set()
        for op in operations:
            self.TopologicalSortHelper(op, deps, visited)

    def CompileReferencedModels(self, referenced_models, referenced_model_to_index):
        for operand in self.operands:
            if operand.lifetime != "SUBGRAPH":
                continue
            model = operand.value[0]
            key = id(model)
            if key not in referenced_model_to_index:
                referenced_model_to_index[key] = len(referenced_model_to_index)
                referenced_models.append(model)
                model.Compile(referenced_models, referenced_model_to_index)
            operand.value = [referenced_model_to_index[key]]

    def Compile(self, referenced_models=None, referenced_model_to_index=None):
        if self.compiled:
            return self
        if referenced_models is None:
            # This is the main model.
            referenced_models = []
            referenced_model_to_index = {}
            self.referenced_models = referenced_models
        self.SetOperandIndex()
        self.SetOperandInsAndOuts()
        self.TopologicalSort()
        self.CompileReferencedModels(referenced_models, referenced_model_to_index)
        # Do not check compliance for relaxed mode tests.
        if self.isRelaxed:
            self.IntroducedIn(None)
        self.compiled = True
        return self

    def Feed(self, feedDict):
        for i in self.GetInputs():
            i.Feed(feedDict[0])
        for o in self.GetOutputs():
            o.Feed(feedDict[1])
        return self

# To track implicitly convertible variation types
class ImplicitVariation:
    @staticmethod
    def ImplicitConvertion(value):
        if isinstance(value, ModelVariation):
            return value
        for implicitType in ImplicitVariation.__subclasses__():
            value = value if type(value) is tuple or type(value) is list else [value]
            if implicitType.IsCompatible(value[0]):
                var = implicitType(value[0])
                if len(value) > 1:
                    var.Identify(*value[1:])
                return var
        assert False, "%s not supported for implicit variation"%value[0]

# An exception indicating that the current variation list should be skipped.
class SkipVariation(Exception):
    pass

# The base class for model variations
class ModelVariation:
    supportsSubgraphs = False

    def __init__(self, name=None):
        self.targetOperands = {}
        self.name = name

    # Apply the model variation.
    def ApplyTo(self, model):
        assert not model.compiled
        assert not model.dumped

        if not self.supportsSubgraphs:
            containsSubgraphs = any(operand.lifetime == "SUBGRAPH" for operand in model.operands)
            assert not containsSubgraphs, "Variation {} does not support subgraphs".format(
                self.__class__.__name__)

        if not self.targetOperands:
            self.AutoIdentify(model)

        # Transform operands and model.
        targets = model.GetEquivalentOperands(sorted(self.targetOperands.keys()))
        model.UpdateEquivalentOperands(
            [self.TransformOperand(op, self.targetOperands[op]) for op in targets])
        model = self.TransformModel(model)
        return model

    def IdentifyOperands(self, args=None):
        if args is None:
            return self
        self.targetOperands = args if type(args) is dict else {i: None for i in args}
        return self

    def Identify(self, operandArgs=None, paramArgs=None):
        self.IdentifyOperands(operandArgs)
        return self

    # Set variation to its default name
    def SetToDefaultName(self):
        self.name = ""
        return self

    # Automatically select the target operand list
    def AutoIdentify(self, model):
        return self

    # Transform operands that are marked by IdentifyOperands()
    def TransformOperand(self, op, arg=None):
        return op

    # Transform the model
    def TransformModel(self, model):
        return model

# Default variation that does nothing
class DefaultVariation(ModelVariation):
    supportsSubgraphs = True

    def __init__(self, name=None):
        ModelVariation.__init__(self, name=name)

# Convert operand data type
class DataTypeConverter(ModelVariation, ImplicitVariation):
    supportsSubgraphs = True

    def __init__(self, targetType=None, name=None, scale=None, zeroPoint=None):
        ModelVariation.__init__(self, name=name)
        if targetType is not None:
            assert DataTypeConverter.IsCompatible(targetType)
        self.targetType = targetType
        self.scale = scale
        self.zeroPoint = zeroPoint

    @staticmethod
    def IsCompatible(value):
        return value.lower() in ["float16", "int32", "quant8", "quant8_signed"]

    def SetToDefaultName(self):
        if self.targetType is not None:
            self.name = self.targetType.lower()
            return self
        targetTypes = list(zip(*(arg for arg in self.targetOperands.values()
                                 if type(arg) is not DataTypeConverter)))[0]
        if "TENSOR_QUANT8_SYMM_PER_CHANNEL" in targetTypes:
            self.name = "channelQuant8"
        elif "TENSOR_QUANT8_ASYMM" in targetTypes:
            self.name = "quant8"
        elif "TENSOR_QUANT8_ASYMM_SIGNED" in targetTypes:
            self.name = "quant8_signed"
        elif "TENSOR_INT32" in targetTypes:
            self.name = "int32"
        elif "TENSOR_FLOAT16" in targetTypes:
            self.name = "float16"
        else:
            self.name = "float32"
        return self

    def AutoIdentify(self, model):
        if self.targetType is not None:
            if self.targetType == "quant8" or self.targetType == "quant8_signed":
                if self.targetType == "quant8":
                    tensorType = "TENSOR_QUANT8_ASYMM"
                else:
                    tensorType = "TENSOR_QUANT8_ASYMM_SIGNED"
                assert self.scale is not None
                assert self.zeroPoint is not None
                tensorType = [tensorType, self.scale, self.zeroPoint]
                scalarType = None  # Not supported.
            else:
                tensorType = ["TENSOR_" + self.targetType.upper()]
                scalarType = [self.targetType.upper()]
            # By default, select all the float32 tensors/scalars
            targets = dict()
            targets.update({op: DataTypeConverter(self.targetType, self.name,
                                                  self.scale, self.zeroPoint)
                            for op in model.operands if op.type.type == "SUBGRAPH"})
            targets.update({op: tensorType
                            for op in model.operands if op.type.type == "TENSOR_FLOAT32"})
            if scalarType is not None:
                targets.update({op: scalarType
                                for op in model.operands if op.type.type == "FLOAT32"})
            self.Identify(targets)
        return self

    def TransformOperand(self, op, arg=None):
        if type(arg) is DataTypeConverter:
            # Handle nested SUBGRAPHs
            assert len(op.value) == 1
            assert type(op.value[0]) is Model
            op.value[0] = arg.ApplyTo(op.value[0])
            return op
        if len(arg) == 1:
            typeTuple = (arg[0], op.type.dimensions)
        else:
            typeTuple = (arg[0], op.type.dimensions, *arg[1:])
        # To handle Internal operands
        if op.value is None or op.type.GetNumberOfElements() == 0:
            op.type = Type.GetType(*typeTuple)
        else:
            v = Dequantize(op.GetValueAsNumpy().astype(np.float32), op.type)
            op.type = Type.GetType(*typeTuple)
            v = Quantize(v, op.type)
            op.SetValueFromNumpy(v)
        return op

# Convert model to turn on/off relaxed computation
class RelaxedModeConverter(ModelVariation, ImplicitVariation):
    supportsSubgraphs = True

    def __init__(self, isRelaxed=True, name=None):
        ModelVariation.__init__(self, name=name)
        if isinstance(isRelaxed, bool):
            self.isRelaxed = isRelaxed
        else:
            assert RelaxedModeConverter.IsCompatible(isRelaxed.lower())
            self.isRelaxed = True

    @staticmethod
    def IsCompatible(value):
        return value.lower() in ["relaxed"]

    def SetToDefaultName(self):
        self.name = "relaxed" if self.isRelaxed else "float"
        return self

    def TransformModel(self, model):
        model.RelaxedExecution(self.isRelaxed)
        return model

# Convert data layout between "NHWC" amd "NCHW"
class DataLayoutConverter(ModelVariation, ImplicitVariation):

    def __init__(self, targetLayout="nchw", name=None):
        ModelVariation.__init__(self, name=name)
        self.targetLayout = targetLayout.lower()
        assert DataLayoutConverter.IsCompatible(self.targetLayout)
        self.perm = (0, 3, 1, 2) if self.targetLayout == "nchw" else (0, 2, 3, 1)
        self.param = True if self.targetLayout == "nchw" else False

    @staticmethod
    def IsCompatible(value):
        return value.lower() in ["nhwc", "nchw"]

    def SetToDefaultName(self):
        self.name = self.targetLayout
        return self

    def TransformOperand(self, op, arg=None):
        if len(op.type.dimensions) == 4:
            # To handle Internal operands
            if op.value is not None and op.type.GetNumberOfElements() != 0:
                op.SetValueFromNumpy(op.GetValueAsNumpy().transpose(self.perm))
            newDim = [op.type.dimensions[i] for i in self.perm]
            op.type = Type.GetType(op.type.type, newDim, op.type.scale, op.type.zeroPoint)
        elif len(op.type.dimensions) == 1 and len(op.value) == 4:
            op.SetValueFromNumpy(op.GetValueAsNumpy()[list(self.perm)])
        elif op.type.type == "BOOL":
            op.SetValue(self.param)
        else:
            assert False, "%s not supported by DataLayoutConverter"%op
        return op

# Convert data by tansposing and removing axis
class AxisConverter(ModelVariation):

    def __init__(self, origin, target, dim, drop=[], name=None):
        ModelVariation.__init__(self, name=name)
        self.origin = origin
        self.target = target
        assert all(i >= -dim and i < dim for i in [self.origin, self.target])
        self.dim = dim
        self.perm = list(range(dim))
        self.perm.insert(target if target >= 0 else target + dim, self.perm.pop(origin))
        self.drop = [drop] if type(drop) is int else list(drop)
        assert all(i >= -dim and i < dim for i in self.drop)
        self.drop = [i if i >= 0 else i + dim for i in self.drop]
        assert target not in self.drop and target + dim not in self.drop

    def SetToDefaultName(self):
        axis = self.target if self.target >= 0 else self.target + self.dim
        axis -= sum(i < axis for i in self.drop)
        neg = "" if self.target >= 0 else "_neg"
        self.name = "dim%d_axis%d%s"%(self.dim - len(self.drop), axis, neg)
        return self

    def TransposeAxis(self, op):
        if op.type.type == "INT32":
            op.SetValue(self.target)
        elif len(op.type.dimensions) == self.dim:
            # To handle Internal operands
            if op.value is not None:
                op.SetValueFromNumpy(op.GetValueAsNumpy().transpose(self.perm))
            newDim = [op.type.dimensions[i] for i in self.perm]
            op.type = Type.GetType(op.type.type, newDim, op.type.scale, op.type.zeroPoint)
        else:
            assert False, "%s not supported by AxisConverter"%op
        return op

    def RemoveAxis(self, op):
        if op.type.type == "INT32":
            if op.value[0] >= 0:
                op.SetValue(op.value[0] - sum(i < op.value[0] for i in self.drop))
            else:
                op.SetValue(op.value[0] + sum(i > (op.value[0] + self.dim) for i in self.drop))
        elif len(op.type.dimensions) == self.dim:
            if op.value is not None:
                val = op.GetValueAsNumpy()
                for i in sorted(self.drop, reverse=True):
                    val = np.take(val, 0, axis=i)
                op.SetValueFromNumpy(val)
            newDim = [op.type.dimensions[i] for i in range(self.dim) if i not in self.drop]
            op.type = Type.GetType(op.type.type, newDim, op.type.scale, op.type.zeroPoint)
        else:
            assert False, "%s not supported by AxisConverter"%op
        return op

    def TransformOperand(self, op, arg=None):
        op = self.TransposeAxis(op)
        op = self.RemoveAxis(op)
        return op

# Convert Output based on activation
class ActivationConverter(ModelVariation, ImplicitVariation):
    # (Enum, low, high)
    actMap = {
        "none": (0, None, None),
        "relu": (1, 0.0, None),
        "relu1": (2, -1.0, 1.0),
        "relu6": (3, 0.0, 6.0),
    }
    def __init__(self, act="relu", name=None):
        ModelVariation.__init__(self, name=name)
        self.act = act.lower()
        assert ActivationConverter.IsCompatible(self.act)
        self.enum = ActivationConverter.actMap[self.act][0]
        self.low = ActivationConverter.actMap[self.act][1]
        self.high = ActivationConverter.actMap[self.act][2]

    @staticmethod
    def IsCompatible(value):
        return value.lower() in ActivationConverter.actMap.keys()

    def SetToDefaultName(self):
        self.name = self.act
        return self

    def TransformOperand(self, op, arg=None):
        if op.type.type == "INT32": # activation enum
            return op.SetValue(self.enum)
        else:
            assert isinstance(op, Output)
            v = op.GetValueAsNumpy()
            if self.low is not None:
                low = Quantize(self.low, op.type)
                v = np.maximum(v, low)
            if self.high is not None:
                high = Quantize(self.high, op.type)
                v = np.minimum(v, high)
            return op.SetValueFromNumpy(v)

# Convert all constant tensors as model inputs.
class AllTensorsAsInputsConverter(ModelVariation):
    supportsSubgraphs = True

    def __init__(self, name=None):
        ModelVariation.__init__(self, name=name)

    def SetToDefaultName(self):
        self.name = "all_tensors_as_inputs"
        return self

    def TransformModel(self, model):
        if len(model.operations) != 1:
            raise SkipVariation

        # Find all constant tensors.
        tensorParams = [
            p for p in model.operands
            if type(p) is Parameter and not p.type.IsScalar() and p.value is not None
        ]
        if not tensorParams:
            raise SkipVariation

        # Convert to model inputs.
        model.UpdateEquivalentOperands([op.ConvertTo(Input) for op in tensorParams])
        return model

def CompatibleWithADD(op):
    return (len(op.type.dimensions) <= 4 and
            len(op.value) > 0 and
            op.type.type in ["TENSOR_FLOAT32", "TENSOR_QUANT8_ASYMM",
                             "TENSOR_FLOAT16", "TENSOR_QUANT8_ASYMM_SIGNED"])

# Add a dummy ADD operation before each model input to make it as an internal operand.
class AllInputsAsInternalCoverter(ModelVariation):
    supportsSubgraphs = True

    def __init__(self, name=None):
        ModelVariation.__init__(self, name=name)

    def SetToDefaultName(self):
        self.name = "all_inputs_as_internal"
        return self

    def TransformModel(self, model):
        if len(model.operations) != 1:
            raise SkipVariation

        # Find all input tensors that can be an output of the ADD operation.
        modelInputs = [i for i in model.GetInputs() if CompatibleWithADD(i) and i.mayBeInternal]
        if not modelInputs:
            raise SkipVariation

        # Make every input an output of a dummy operation: input_new ADD dummy = input.
        for op in modelInputs:
            newInput = op.ConvertTo(Input, name=op.name + "_new")
            dummyParam = Parameter("dummy", (op.type.type, [1], op.type.scale, op.type.zeroPoint),
                                   [op.type.zeroPoint])
            model.Operation("ADD", newInput, dummyParam, 0).To(op)

        # Convert to internal operands.
        model.UpdateEquivalentOperands([op.ConvertTo(Internal) for op in modelInputs])
        return model

# Add a dummy ADD operation after each model output to make it as an internal operand.
class AllOutputsAsInternalCoverter(ModelVariation):
    supportsSubgraphs = True

    def __init__(self, name=None):
        ModelVariation.__init__(self, name=name)

    def SetToDefaultName(self):
        self.name = "all_outputs_as_internal"
        return self

    def TransformModel(self, model):
        if len(model.operations) != 1:
            raise SkipVariation

        # Find all output tensors that can be an input to an ADD operation.
        modelOutputs = [o for o in model.GetOutputs() if CompatibleWithADD(o)]
        if not modelOutputs:
            raise SkipVariation

        # Make every output an input of a dummy operation: output ADD dummy = output_new.
        for op in modelOutputs:
            newOutput = op.ConvertTo(Output, name=op.name + "_new")
            dummyParam = Parameter("dummy", (op.type.type, [1], op.type.scale, op.type.zeroPoint),
                                   [op.type.zeroPoint])
            model.Operation("ADD", op, dummyParam, 0).To(newOutput)

        # Convert to internal operands.
        model.UpdateEquivalentOperands([op.ConvertTo(Internal) for op in modelOutputs])
        return model

# An example is always attached to a model, and could have multiple variations
class Example:
    examples = []
    versionOverrides = {}

    def __init__(self, *args, model=None, name=None):
        self.model = Model.models[-1] if model is None else model
        self.name = name
        self.expectedMultinomialDistributionTolerance = 0
        self.expectFailure = False
        self.testDynamicOutputShape = True
        self.testLifeTimeVariation = True
        self.feedDicts = []
        for feedDict in args:
            if type(feedDict) is tuple or type(feedDict) is list:
                self.feedDicts.append(feedDict)
            elif type(feedDict) is dict:
                self.feedDicts.append((
                    {i: feedDict[i] for i in self.model.GetInputs()},
                    {o: feedDict[o] for o in self.model.GetOutputs()}
                ))
            else:
                assert False
        self.variations = []
        Example.examples.append(self)

    @staticmethod
    def SetVersion(ver, *args):
        for name in args:
            Example.versionOverrides[name] = ver

    # Main entrance of test generator
    @staticmethod
    def DumpAllExamples(DumpModel=None, model_fd=None,
                        DumpExample=None, example_fd=None,
                        DumpTest=None, test_fd=None):
        Example.CombineAllExamples()
        for example in Example.examples:
            example.Dump(DumpModel, model_fd, DumpExample, example_fd, DumpTest, test_fd)

    # Combine examples with the same model, same name, and same set of variations
    @staticmethod
    def CombineAllExamples():
        modelMap = {}
        newExamples = []
        for example in Example.examples:
            key = (example.model, example.name, tuple(tuple(e) for e in example.variations))
            if key in modelMap:
                modelMap[key].Combine(example)
            else:
                modelMap[key] = example
                newExamples.append(example)
        Example.examples = newExamples

    def AddVariations(self, *args, includeDefault=True, defaultName=None):
        self.variations.append([DefaultVariation(defaultName)] if includeDefault else [])
        self.variations[-1].extend(ImplicitVariation.ImplicitConvertion(i) for i in args)
        return self

    def AddNchw(self, *args, includeDefault=True, defaultName="nhwc"):
        var = DataLayoutConverter("nchw").Identify(args)
        self.AddVariations(var, includeDefault=includeDefault, defaultName=defaultName)
        return self

    def AddRelaxed(self, isRelaxed=True, includeDefault=True, defaultName=None):
        var = RelaxedModeConverter(isRelaxed)
        self.AddVariations(var, includeDefault=includeDefault, defaultName=defaultName)
        return self

    def AddRelu(self, *args, includeDefault=True, defaultName=None):
        var = ActivationConverter("relu").Identify(args)
        self.AddVariations(var, includeDefault=includeDefault, defaultName=defaultName)
        return self

    def AddAllActivations(self, *args):
        var = [ActivationConverter(i).Identify(args)
            for i in sorted(ActivationConverter.actMap.keys())]
        self.AddVariations(*var, includeDefault=False)
        return self

    def GuessOriginalAxisAndDim(self, *args):
        origin = None
        dim = None
        for arg in args:
            if arg.type.type == "INT32":
                origin = arg.value[0]
            else:
                if dim is None:
                    dim = len(arg.type.dimensions)
                else:
                    assert dim == len(arg.type.dimensions)
        assert dim is not None
        origin = dim - 1 if origin is None else origin
        origin = origin + dim if origin < 0 else origin
        return origin, dim

    def AddAxis(self, axis, *args, includeDefault=True, defaultName=None):
        origin, dim = self.GuessOriginalAxisAndDim(*args)
        axis = [axis] if type(axis) is int else list(axis)
        var = [AxisConverter(origin, a, dim).Identify(args) for a in axis]
        self.AddVariations(*var, includeDefault=includeDefault, defaultName=defaultName)
        return self

    def AddAllPositiveAxis(self, *args):
        origin, dim = self.GuessOriginalAxisAndDim(*args)
        var = [AxisConverter(origin, a, dim).Identify(args) for a in range(dim)]
        self.AddVariations(*var, includeDefault=False)
        return self

    def AddAllAxis(self, *args):
        origin, dim = self.GuessOriginalAxisAndDim(*args)
        var = [AxisConverter(origin, a, dim).Identify(args) for a in range(-dim, dim)]
        self.AddVariations(*var, includeDefault=False)
        return self

    def AddDims(self, dims, *args, includeDefault=True, defaultName=None):
        origin, dim = self.GuessOriginalAxisAndDim(*args)
        dims = [dims] if type(dims) is int else list(dims)
        drop = list(range(dim))
        drop.pop(origin)
        var = [AxisConverter(origin, origin, dim, drop[0:(dim-i)]).Identify(args) for i in dims]
        self.AddVariations(*var, includeDefault=includeDefault, defaultName=defaultName)
        return self

    def AddAllDims(self, *args):
        origin, dim = self.GuessOriginalAxisAndDim(*args)
        drop = list(range(dim))
        drop.pop(origin)
        var = [AxisConverter(origin, origin, dim, drop[0:i]).Identify(args) for i in range(dim)]
        self.AddVariations(*var, includeDefault=False)
        return self

    def AddAllDimsAndPositiveAxis(self, *args):
        origin, dim = self.GuessOriginalAxisAndDim(*args)
        var = [AxisConverter(origin, j, dim, range(i)).Identify(args) \
                for i in range(dim) for j in range(i, dim)]
        self.AddVariations(*var, includeDefault=False)
        return self

    def AddAllDimsAndAxis(self, *args):
        origin, dim = self.GuessOriginalAxisAndDim(*args)
        var = [AxisConverter(origin, k, dim, range(i)).Identify(args) \
                for i in range(dim) for j in range(i, dim) for k in [j, j - dim]]
        self.AddVariations(*var, includeDefault=False)
        return self

    def Combine(self, other):
        assert self.model is other.model, "Only examples targetting the same model can be combined"
        assert tuple(self.variations) == tuple(other.variations), \
            "Only examples with the same set of variations can be combined"
        assert self.name == other.name, "Only examples with the same name can be combined"
        self.feedDicts.extend(other.feedDicts)
        return self

    def Dump(self, DumpModel, model_fd, DumpExample, example_fd, DumpTest, test_fd):
        if self.testLifeTimeVariation and len(self.model.operations) == 1 and \
                self.expectedMultinomialDistributionTolerance == 0:
            self.AddVariations(AllTensorsAsInputsConverter())
            self.AddVariations(AllInputsAsInternalCoverter())
        [v.SetToDefaultName() for vs in self.variations for v in vs if v.name is None]

        for feedDict in self.feedDicts:
            self.model.Feed(feedDict)
            for variationList in itertools.product(*self.variations):
                modelOrigin = self.model
                self.model = copy.deepcopy(self.model)

                # Apply variations
                try:
                    for variation in variationList:
                        self.model = variation.ApplyTo(self.model)
                except SkipVariation:
                    self.model = modelOrigin
                    continue

                # Concat names for test and examples
                varNames = [v.name for v in variationList]
                self.testName = NamedTest(FileNames.specName, self.model.name, self.name, *varNames)
                self.examplesName = GlobalVariable("test_model", self.model.name, self.name,
                                                   *varNames)
                if str(self.testName) in Example.versionOverrides:
                    self.model.IntroducedIn(Example.versionOverrides[str(self.testName)])
                self.model.WithSuffix(*varNames).Compile()

                # Dump files
                if DumpExample is not None and example_fd is not None:
                    DumpExample(self, example_fd)
                if DumpTest is not None and test_fd is not None:
                    DumpTest(self, test_fd)

                # Restore model before variation
                self.model = modelOrigin
        return self

    # Specifies the RANDOM_MULTINOMIAL distribution tolerance.
    # If set to greater than zero, the input is compared as log-probabilities
    # to the output and must be within this tolerance to pass.
    def WithMultinomialDistributionTolerance(self, expectedTolerance):
        assert self.expectFailure is False
        self.expectedMultinomialDistributionTolerance = expectedTolerance
        return self

    # Specifies that this example is expected to fail during compilation or execution.
    def ExpectFailure(self):
        assert self.expectedMultinomialDistributionTolerance == 0
        self.expectFailure = True
        return self

    def DisableDynamicOutputShapeVariation(self):
        self.testDynamicOutputShape = False
        return self

    def DisableLifeTimeVariation(self):
        self.testLifeTimeVariation = False
        return self

class FileNames:
    specFiles = []
    specNames = []
    exampleFiles = []
    specFile = ""
    specName = ""
    exampleFile = ""
    version = ""
    fileIndex = 0

    @staticmethod
    def InitializeFileLists(spec, example):
        # get all spec files and target files
        if os.path.isfile(spec):
            FileNames.specFiles = [os.path.abspath(spec)]
        elif os.path.isdir(spec):
            FileNames.specFiles = sorted([os.path.abspath(os.path.join(spec, f))
                for f in os.listdir(spec) if f.endswith(".mod.py")])
        else:
            assert False, "%s is neither a file or a directory"%spec
        FileNames.specNames = [re.sub(r"\..*", "", os.path.basename(f))
            for f in FileNames.specFiles]
        FileNames.exampleFiles = FileNames.ParseTargetFiles(example, ".example.cpp")

    @staticmethod
    def ParseTargetFiles(arg, ext):
        numFiles = len(FileNames.specFiles)
        if arg is None:
            return [None] * numFiles
        absPath = os.path.abspath(arg)
        if os.path.isdir(arg):
            target = [os.path.join(absPath, f + ext) for f in FileNames.specNames]
        elif arg == "-":
            target = ["-"] * numFiles
        else:
            target = [absPath] * numFiles
        return target

    @staticmethod
    def NextFile():
        if FileNames.fileIndex >= len(FileNames.specFiles):
            return False
        FileNames.specFile = FileNames.specFiles[FileNames.fileIndex]
        FileNames.specName = FileNames.specNames[FileNames.fileIndex]
        FileNames.exampleFile = FileNames.exampleFiles[FileNames.fileIndex]
        FileNames.fileIndex += 1
        NamedObject.existingNames = set()
        NamedVariable.existingNames = set()
        NamedTest.existingNames = set()
        Type.typesMap = dict()
        Model.models = list()
        Example.examples = list()
        Configuration.use_shm_for_weights = False

        # Extract version from absolute file path.
        versionMatch = re.findall(r"/V\d_\d/", FileNames.specFile)
        if len(versionMatch) == 1:
            FileNames.version = versionMatch[0].strip('/')
        else:
            FileNames.version = None
        return True

class Configuration:
    use_shm_for_weights = False
    hook_mode = False

    @staticmethod
    def useSHM():
        return Configuration.use_shm_for_weights

def GetTestGeneratorMTime():
    tgFiles = ['test_generator.py', 'example_generator.py']
    tgDir = os.path.dirname(__file__)
    return max(os.path.getmtime(os.path.join(tgDir, filename))
               for filename in tgFiles)

def MightNeedRegeneration():
    specTime = os.path.getmtime(FileNames.specFile)
    tgTime = GetTestGeneratorMTime()
    return not os.path.exists(FileNames.exampleFile) or \
           os.path.getmtime(FileNames.exampleFile) <= max(specTime, tgTime)

def Read(filename):
    with open(filename) as reader:
        return reader.read()

def AtomicWrite(filename, data):
    # os.replace(src, dest) may fail if src and dest are on diffrent
    # filesystems.
    tempFile = filename + '.tmp'
    try:
        with open(tempFile, 'w') as writer:
            writer.write(data)
        os.replace(tempFile, filename)
        tempFile = None
    finally:
        if tempFile is not None and os.path.exists(tempFile):
            os.remove(tempFile)

def GetExecScope():
    return dict(
        ActivationConverter=ActivationConverter,
        AllInputsAsInternalCoverter=AllInputsAsInternalCoverter,
        AllOutputsAsInternalCoverter=AllOutputsAsInternalCoverter,
        AllTensorsAsInputsConverter=AllTensorsAsInputsConverter,
        BoolScalar=BoolScalar,
        Configuration=Configuration,
        DataLayoutConverter=DataLayoutConverter,
        DataTypeConverter=DataTypeConverter,
        Example=Example,
        Float16Scalar=Float16Scalar,
        Float32Scalar=Float32Scalar,
        Float32Vector=Float32Vector,
        IgnoredOutput=IgnoredOutput,
        Input=Input,
        Int32Scalar=Int32Scalar,
        Int32Vector=Int32Vector,
        Internal=Internal,
        Model=Model,
        Operand=Operand,
        Output=Output,
        Parameter=Parameter,
        RelaxedModeConverter=RelaxedModeConverter,
        SubgraphReference=SubgraphReference,
        SymmPerChannelQuantParams=SymmPerChannelQuantParams)

def ArgumentParser():
    parser = argparse.ArgumentParser()
    parser.add_argument("spec", help="the spec file or directory")
    parser.add_argument("--hook", help="hook mode", action='store_true')
    return parser

def ParseArgs(parser):
    args = parser.parse_args()
    Configuration.hook_mode = args.hook
    return args

def Run(InitializeFiles=None, DumpExample=None):
    exec_scope = GetExecScope()
    while FileNames.NextFile():
        try:
            if not MightNeedRegeneration():
                continue
            exec(Read(FileNames.specFile), exec_scope)
            example_buf = io.StringIO() if FileNames.exampleFile else None
            InitializeFiles(example_fd=example_buf)
            Example.DumpAllExamples(DumpExample=DumpExample, example_fd=example_buf)
            if FileNames.exampleFile is None:
                continue
            if Configuration.hook_mode and (not os.path.exists(FileNames.exampleFile) or
                                            Read(FileNames.exampleFile) != example_buf.getvalue()):
                print(('\n{filename} is out of date. '
                        'Please run {generate_all_tests_sh} before uploading.\n').format(
                                filename=FileNames.exampleFile,
                                generate_all_tests_sh=os.path.abspath(os.path.join(
                                        os.path.dirname(__file__), '..', '..', 'runtime', 'test',
                                        'specs', 'generate_all_tests.sh'))))
                sys.exit(1)
            AtomicWrite(FileNames.exampleFile, example_buf.getvalue())
        except Exception:
            traceback.print_exc()
            sys.exit("Exception raised when processing {}".format(FileNames.specFile))
