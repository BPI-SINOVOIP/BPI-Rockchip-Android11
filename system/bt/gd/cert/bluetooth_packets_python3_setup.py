#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

# Usage:
# 1. Run envsetup and lunch first in an Android checkout
# 2. Make target bluetooth_packets_python3 that will generate C++ sources for the
#    Extension
# 3. Build only:
#       python3 bluetooth_packets_python3_setup.py build_ext
#   Then Find the .so file in build/lib.linux-x86_64-3.X
# 4. Install:
#       python3 bluetooth_packets_python3_setup.py install --user

import os
import glob
from setuptools import setup, Extension

ANDROID_BUILD_TOP = os.getenv("ANDROID_BUILD_TOP")
PYBIND11_INCLUDE_DIR = os.path.join(ANDROID_BUILD_TOP,
                                    "external/python/pybind11/include")
GD_DIR = os.path.join(ANDROID_BUILD_TOP, "system/bt/gd")
BT_PACKETS_GEN_DIR = os.path.join(
    ANDROID_BUILD_TOP,
    "out/soong/.intermediates/system/bt/gd/BluetoothGeneratedPackets_h/gen")
BT_PACKETS_PY3_GEN_DIR = os.path.join(
    ANDROID_BUILD_TOP,
    "out/soong/.intermediates/system/bt/gd/BluetoothGeneratedPackets_python3_cc/gen"
)

BT_PACKETS_BASE_SRCS = [
    os.path.join(GD_DIR, "l2cap/fcs.cc"),
    os.path.join(GD_DIR, "packet/bit_inserter.cc"),
    os.path.join(GD_DIR, "packet/byte_inserter.cc"),
    os.path.join(GD_DIR, "packet/byte_observer.cc"),
    os.path.join(GD_DIR, "packet/iterator.cc"),
    os.path.join(GD_DIR, "packet/fragmenting_inserter.cc"),
    os.path.join(GD_DIR, "packet/packet_view.cc"),
    os.path.join(GD_DIR, "packet/raw_builder.cc"),
    os.path.join(GD_DIR, "packet/view.cc"),
]

BT_PACKETS_PY3_SRCs = \
  [os.path.join(GD_DIR, "packet/python3_module.cc")] \
  + glob.glob(os.path.join(BT_PACKETS_PY3_GEN_DIR, "hci", "*.cc")) \
  + glob.glob(os.path.join(BT_PACKETS_PY3_GEN_DIR, "l2cap", "*.cc")) \
  + glob.glob(os.path.join(BT_PACKETS_PY3_GEN_DIR, "security", "*.cc"))

bluetooth_packets_python3_module = Extension(
    'bluetooth_packets_python3',
    sources=BT_PACKETS_BASE_SRCS + BT_PACKETS_PY3_SRCs,
    include_dirs=[
        GD_DIR, BT_PACKETS_GEN_DIR, BT_PACKETS_PY3_GEN_DIR, PYBIND11_INCLUDE_DIR
    ],
    extra_compile_args=['-std=c++17'])

setup(
    name='bluetooth_packets_python3',
    version='1.0',
    author="Android Open Source Project",
    description="""Bluetooth Packet Library""",
    ext_modules=[bluetooth_packets_python3_module],
    py_modules=["bluetooth_packets_python3"],
)
