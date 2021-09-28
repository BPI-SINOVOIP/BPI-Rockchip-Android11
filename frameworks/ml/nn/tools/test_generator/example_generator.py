#!/usr/bin/python3

# Copyright 2018, The Android Open Source Project
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

"""Example generator

Compiles spec files and generates the corresponding C++ TestModel definitions.
Invoked by ml/nn/runtime/test/specs/generate_all_tests.sh;
See that script for details on how this script is used.

"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
import os
import sys
import traceback

import test_generator as tg

# See ToCpp()
COMMENT_KEY = "__COMMENT__"

# Take a model from command line
def ParseCmdLine():
    parser = tg.ArgumentParser()
    parser.add_argument("-e", "--example", help="the output example file or directory")
    args = tg.ParseArgs(parser)
    tg.FileNames.InitializeFileLists(args.spec, args.example)

# Write headers for generated files, which are boilerplate codes only related to filenames
def InitializeFiles(example_fd):
    specFileBase = os.path.basename(tg.FileNames.specFile)
    fileHeader = """\
// Generated from {spec_file}
// DO NOT EDIT
// clang-format off
#include "TestHarness.h"
using namespace test_helper;
"""
    if example_fd is not None:
        print(fileHeader.format(spec_file=specFileBase), file=example_fd)

def IndentedStr(s, indent):
    return ("\n" + " " * indent).join(s.split('\n'))

def ToCpp(var, indent=0):
    """Get the C++-style representation of a Python object.

    For Python dictionary, it will be mapped to C++ struct aggregate initialization:
        {
            .key0 = value0,
            .key1 = value1,
            ...
        }

    For Python list, it will be mapped to C++ list initalization:
        {value0, value1, ...}

    In both cases, value0, value1, ... are stringified by invoking this method recursively.
    """
    if isinstance(var, dict):
        if not var:
            return "{}"
        comment = var.get(COMMENT_KEY)
        comment = "" if comment is None else " // %s" % comment
        str_pair = lambda k, v: "    .%s = %s" % (k, ToCpp(v, indent + 4))
        agg_init = "{%s\n%s\n}" % (comment,
                                   ",\n".join(str_pair(k, var[k])
                                              for k in sorted(var.keys())
                                              if k != COMMENT_KEY))
        return IndentedStr(agg_init, indent)
    elif isinstance(var, (list, tuple)):
        return "{%s}" % (", ".join(ToCpp(i, indent) for i in var))
    elif type(var) is bool:
        return "true" if var else "false"
    elif type(var) is float:
        return tg.PrettyPrintAsFloat(var)
    else:
        return str(var)

def GetSymmPerChannelQuantParams(extraParams):
    """Get the dictionary that corresponds to test_helper::TestSymmPerChannelQuantParams."""
    if extraParams is None or extraParams.hide:
        return {}
    else:
        return {"scales": extraParams.scales, "channelDim": extraParams.channelDim}

def GetOperandStruct(operand):
    """Get the dictionary that corresponds to test_helper::TestOperand."""
    return {
        COMMENT_KEY: operand.name,
        "type": "TestOperandType::" + operand.type.type,
        "dimensions": operand.type.dimensions,
        "scale": operand.type.scale,
        "zeroPoint": operand.type.zeroPoint,
        "numberOfConsumers": len(operand.outs),
        "lifetime": "TestOperandLifeTime::" + operand.lifetime,
        "channelQuant": GetSymmPerChannelQuantParams(operand.type.extraParams),
        "isIgnored": isinstance(operand, tg.IgnoredOutput),
        "data": "TestBuffer::createFromVector<{cpp_type}>({data})".format(
            cpp_type=operand.type.GetCppTypeString(),
            data=operand.GetListInitialization(),
        )
    }

def GetOperationStruct(operation):
    """Get the dictionary that corresponds to test_helper::TestOperation."""
    return {
        "type": "TestOperationType::" + operation.optype,
        "inputs": [op.model_index for op in operation.ins],
        "outputs": [op.model_index for op in operation.outs],
    }

def GetSubgraphStruct(subgraph):
    """Get the dictionary that corresponds to test_helper::TestSubgraph."""
    return {
        COMMENT_KEY: subgraph.name,
        "operands": [GetOperandStruct(op) for op in subgraph.operands],
        "operations": [GetOperationStruct(op) for op in subgraph.operations],
        "inputIndexes": [op.model_index for op in subgraph.GetInputs()],
        "outputIndexes": [op.model_index for op in subgraph.GetOutputs()],
    }

def GetModelStruct(example):
    """Get the dictionary that corresponds to test_helper::TestModel."""
    return {
        "main": GetSubgraphStruct(example.model),
        "referenced": [GetSubgraphStruct(model) for model in example.model.GetReferencedModels()],
        "isRelaxed": example.model.isRelaxed,
        "expectedMultinomialDistributionTolerance":
                example.expectedMultinomialDistributionTolerance,
        "expectFailure": example.expectFailure,
        "minSupportedVersion": "TestHalVersion::%s" % (
                example.model.version if example.model.version is not None else "UNKNOWN"),
    }

def DumpExample(example, example_fd):
    assert example.model.compiled
    template = """\
namespace generated_tests::{spec_name} {{

const TestModel& get_{example_name}() {{
    static TestModel model = {aggregate_init};
    return model;
}}

const auto dummy_{example_name} = TestModelManager::get().add("{test_name}", get_{example_name}());

}}  // namespace generated_tests::{spec_name}
"""
    print(template.format(
            spec_name=tg.FileNames.specName,
            test_name=str(example.testName),
            example_name=str(example.examplesName),
            aggregate_init=ToCpp(GetModelStruct(example), indent=4),
        ), file=example_fd)


if __name__ == '__main__':
    ParseCmdLine()
    tg.Run(InitializeFiles=InitializeFiles, DumpExample=DumpExample)
