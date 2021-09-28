# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
import logging
import os
import shutil
import subprocess
import sys
from importlib import import_module

from google.protobuf import descriptor_pb2
from google.protobuf import text_format


def compile_proto(proto_path, output_dir):
    """Invoke Protocol Compiler to generate python from given source .proto."""
    # Find compiler path
    protoc = None
    if 'PROTOC' in os.environ and os.path.exists(os.environ['PROTOC']):
        protoc = os.environ['PROTOC']
    if not protoc:
        protoc = shutil.which('protoc')
    if not protoc:
        logging.error(
            "Cannot find protobuf compiler (>=3.0.0), please install"
            "protobuf-compiler package. Prefer copying from <top>/prebuilts/tools"
        )
        logging.error("    prebuilts/tools/linux-x86_64/protoc/bin/protoc")
        logging.error("If prebuilts are not available, use apt-get:")
        logging.error("    sudo apt-get install protobuf-compiler")
        return None
    # Validate input proto path
    if not os.path.exists(proto_path):
        logging.error('Can\'t find required file: %s\n' % proto_path)
        return None
    # Validate output py-proto path
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    elif not os.path.isdir(output_dir):
        logging.error("Output path is not a valid directory: %s" %
                      (output_dir))
        return None
    input_dir = os.path.dirname(proto_path)
    output_filename = os.path.basename(proto_path).replace('.proto', '_pb2.py')
    output_path = os.path.join(output_dir, output_filename)
    # Compiling proto
    logging.debug('Generating %s' % output_path)
    protoc_command = [
        protoc, '-I=%s' % (input_dir), '--python_out=%s' % (output_dir),
        proto_path
    ]
    logging.debug('Running command %s' % protoc_command)
    if subprocess.call(protoc_command, stderr=subprocess.STDOUT) != 0:
        logging.error("Fail to compile proto")
        return None
    output_module_name = os.path.splitext(output_filename)[0]
    return output_module_name


def compile_import_proto(output_dir, proto_path):
    """Compiles the given protobuf file and return the module.

    Args:
        output_dir: The directory to put the compilation output.
        proto_path: The path to the .proto file that needs to be compiled.
    Returns:
        The protobuf module.
    """
    output_module_name = compile_proto(proto_path, output_dir)
    if not output_module_name:
        return None
    sys.path.append(output_dir)
    output_module = None
    try:
        output_module = import_module(output_module_name)
    except ImportError:
        logging.error("Cannot import generated py-proto %s" %
                      (output_module_name))
    return output_module


def parse_proto_to_ascii(binary_proto_msg):
    """ Parses binary protobuf message to human readable ascii string.

    Args:
        binary_proto_msg: The binary format of the proto message.
    Returns:
        The ascii format of the proto message.
    """
    return text_format.MessageToString(binary_proto_msg)


def to_descriptor_proto(proto):
    """Retrieves the descriptor proto for the given protobuf message.

    Args:
        proto: the original message.
    Returns:
        The descriptor proto for the input meessage.
    """
    descriptor_proto = descriptor_pb2.DescriptorProto()
    proto.DESCRIPTOR.CopyToProto(descriptor_proto)
    return descriptor_proto
