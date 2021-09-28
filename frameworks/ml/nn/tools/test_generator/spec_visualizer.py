#!/usr/bin/python3

# Copyright 2019, The Android Open Source Project
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

"""Spec Visualizer

Visualize python spec file for test generator.
Invoked by ml/nn/runtime/test/specs/visualize_spec.sh;
See that script for details on how this script is used.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
import argparse
import json
import os
import sys
from string import Template

# Stuff from test generator
import test_generator as tg
from test_generator import ActivationConverter
from test_generator import BoolScalar
from test_generator import Configuration
from test_generator import DataTypeConverter
from test_generator import DataLayoutConverter
from test_generator import Example
from test_generator import Float16Scalar
from test_generator import Float32Scalar
from test_generator import Float32Vector
from test_generator import GetJointStr
from test_generator import IgnoredOutput
from test_generator import Input
from test_generator import Int32Scalar
from test_generator import Int32Vector
from test_generator import Internal
from test_generator import Model
from test_generator import Operand
from test_generator import Output
from test_generator import Parameter
from test_generator import RelaxedModeConverter
from test_generator import SymmPerChannelQuantParams


TEMPLATE_FILE = os.path.join(os.path.dirname(os.path.realpath(__file__)), "spec_viz_template.html")
global_graphs = dict()


def FormatArray(data, is_scalar=False):
    if is_scalar:
        assert len(data) == 1
        return str(data[0])
    else:
        return "[%s]" % (", ".join(str(i) for i in data))


def FormatDict(data):
    return "<br/>".join("<b>%s:</b> %s"%(k.capitalize(), v) for k, v in data.items())


def GetOperandInfo(op):
    op_info = {"lifetime": op.lifetime, "type": op.type.type}

    if not op.type.IsScalar():
        op_info["dimensions"] = FormatArray(op.type.dimensions)

    if op.type.scale != 0:
        op_info["scale"] = op.type.scale
        op_info["zero point"] = op.type.zeroPoint
    if op.type.type == "TENSOR_QUANT8_SYMM_PER_CHANNEL":
        op_info["scale"] = FormatArray(op.type.extraParams.scales)
        op_info["channel dim"] = op.type.extraParams.channelDim

    return op_info


def FormatOperand(op):
    # All keys and values in op_info will appear in the tooltip. We only display the operand data
    # if the length is less than 10. This should be convenient enough for most parameters.
    op_info = GetOperandInfo(op)
    if isinstance(op, Parameter) and len(op.value) <= 10:
        op_info["data"] = FormatArray(op.value, op.type.IsScalar())

    template = "<span class='tooltip'><span class='tooltipcontent'>{tooltip_content}</span><a href=\"{inpage_link}\">{op_name}</a></span>"
    return template.format(
        op_name=str(op),
        tooltip_content=FormatDict(op_info),
        inpage_link="#details-operands-%d" % (op.model_index),
    )


def GetSubgraph(example):
    """Produces the nodes and edges information for d3 visualization."""

    node_index_map = {}
    topological_order = []

    def AddToTopologicalOrder(op):
        if op not in node_index_map:
            node_index_map[op] = len(topological_order)
            topological_order.append(op)

    # Get the topological order, both operands and operations are treated the same.
    # Given that the example.model.operations is already topologically sorted, here we simply
    # iterate through and insert inputs and outputs.
    for op in example.model.operations:
        for i in op.ins:
            AddToTopologicalOrder(i)
        AddToTopologicalOrder(op)
        for o in op.outs:
            AddToTopologicalOrder(o)

    # Assign layers to the nodes.
    layers = {}
    for node in topological_order:
        layers[node] = max([layers[i] for i in node.ins], default=-1) + 1
    for node in reversed(topological_order):
        layers[node] = min([layers[o] for o in node.outs], default=layers[node]+1) - 1
    num_layers = max(layers.values()) + 1

    # Assign coordinates to the nodes. Nodes are equally spaced.
    CoordX = lambda index: (index + 0.5) * 200  # 200px spacing horizontally
    CoordY = lambda index: (index + 0.5) * 100  # 100px spacing vertically
    coords = {}
    layer_cnt = [0] * num_layers
    for node in topological_order:
        coords[node] = (CoordX(layer_cnt[layers[node]]), CoordY(layers[node]))
        layer_cnt[layers[node]] += 1

    # Create edges and nodes dictionaries for d3 visualization.
    OpName = lambda idx: "operation%d" % idx
    edges = []
    nodes = []
    for ind, op in enumerate(example.model.operations):
        for tensor in op.ins:
            edges.append({
                "source": str(tensor),
                "target": OpName(ind)
            })
        for tensor in op.outs:
            edges.append({
                "target": str(tensor),
                "source": OpName(ind)
            })
        nodes.append({
            "index": ind,
            "id": OpName(ind),
            "name": op.optype,
            "group": 2,
            "x": coords[op][0],
            "y": coords[op][1],
        })

    for ind, op in enumerate(example.model.operands):
        nodes.append({
            "index": ind,
            "id": str(op),
            "name": str(op),
            "group": 1,
            "x": coords[op][0],
            "y": coords[op][1],
        })

    return {"nodes": nodes, "edges": edges}


# The following Get**Info methods will each return a list of dictionaries,
# whose content will appear in the tables and sidebar views.
def GetConfigurationsInfo(example):
    return [{
        "relaxed": str(example.model.isRelaxed),
        "use shared memory": str(tg.Configuration.useSHM()),
        "expect failure": str(example.expectFailure),
    }]


def GetOperandsInfo(example):
    ret = []
    for index, op in enumerate(example.model.operands):
        ret.append({
                "index": index,
                "name": str(op),
                "group": "operand"
            })
        ret[-1].update(GetOperandInfo(op))
        if isinstance(op, (Parameter, Input, Output)):
            ret[-1]["data"] = FormatArray(op.value, op.type.IsScalar())
    return ret


def GetOperationsInfo(example):
    return [{
            "index": index,
            "name": op.optype,
            "group": "operation",
            "opcode": op.optype,
            "inputs": ", ".join(FormatOperand(i) for i in op.ins),
            "outputs": ", ".join(FormatOperand(o) for o in op.outs),
        } for index,op in enumerate(example.model.operations)]


# TODO: Remove the unused fd from the parameter.
def ProcessExample(example, fd):
    """Process an example and save the information into the global dictionary global_graphs."""

    global global_graphs
    print("    Processing variation %s" % example.testName)
    global_graphs[str(example.testName)] = {
        "subgraph": GetSubgraph(example),
        "details": {
            "configurations": GetConfigurationsInfo(example),
            "operands": GetOperandsInfo(example),
            "operations": GetOperationsInfo(example)
        }
    }


def DumpHtml(spec_file, out_file):
    """Dump the final HTML file by replacing entries from a template file."""

    with open(TEMPLATE_FILE, "r") as template_fd:
        html_template = template_fd.read()

    with open(out_file, "w") as out_fd:
        out_fd.write(Template(html_template).substitute(
            spec_name=os.path.basename(spec_file),
            graph_dump=json.dumps(global_graphs),
        ))


def ParseCmdLine():
    parser = argparse.ArgumentParser()
    parser.add_argument("spec", help="the spec file")
    parser.add_argument("-o", "--out", help="the output html path", default="out.html")
    args = parser.parse_args()
    tg.FileNames.InitializeFileLists(args.spec, "-")
    tg.FileNames.NextFile()
    return os.path.abspath(args.spec), os.path.abspath(args.out)


if __name__ == '__main__':
    spec_file, out_file = ParseCmdLine()
    print("Visualizing from spec: %s" % spec_file)
    exec(open(spec_file, "r").read())
    Example.DumpAllExamples(DumpExample=ProcessExample, example_fd=0)
    DumpHtml(spec_file, out_file)
    print("Output HTML file: %s" % out_file)

