#!/usr/bin/env python
#
# Copyright 2018 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""setup.py to generate pb2.py from proto files."""

from __future__ import print_function

try:
    from distutils.spawn import find_executable
except ImportError:
    from shutil import which as find_executable
import os
import subprocess
import sys

import setuptools

ACLOUD_DIR = os.path.realpath(os.path.dirname(__file__))

try:
    with open("README.md", "r") as fh:
        LONG_DESCRIPTION = fh.read()
except IOError:
    LONG_DESCRIPTION = ""

# Find the Protocol Compiler. (Taken from protobuf/python/setup.py)
if "PROTOC" in os.environ and os.path.exists(os.environ["PROTOC"]):
    PROTOC = os.environ["PROTOC"]
else:
    PROTOC = find_executable("protoc")


def GenerateProto(source):
    """Generate a _pb2.py from a .proto file.

    Invokes the Protocol Compiler to generate a _pb2.py from the given
    .proto file.  Does nothing if the output already exists and is newer than
    the input.

    Args:
      source: The source proto file that needs to be compiled.
    """

    output = source.replace(".proto", "_pb2.py")

    if not os.path.exists(output) or (
            os.path.exists(source) and
            os.path.getmtime(source) > os.path.getmtime(output)):
        print("Generating %s..." % output)

        if not os.path.exists(source):
            sys.stderr.write("Can't find required file: %s\n" % source)
            sys.exit(-1)

        if PROTOC is None:
            sys.stderr.write(
                "protoc is not found.  Please compile it "
                "or install the binary package.\n"
            )
            sys.exit(-1)

        protoc_command = [PROTOC, "-I%s" % ACLOUD_DIR, "--python_out=.", source]
        if subprocess.call(protoc_command) != 0:
            sys.exit(-1)


# Generate the protobuf files that we depend on.
GenerateProto(os.path.join(ACLOUD_DIR, "internal/proto/user_config.proto"))
GenerateProto(os.path.join(ACLOUD_DIR, "internal/proto/internal_config.proto"))
open(os.path.join(ACLOUD_DIR, "internal/proto/__init__.py"), "a").close()

setuptools.setup(
    python_requires=">=2",
    name="acloud",
    version="1.0",
    setup_requires=["pytest-runner"],
    tests_require=["pytest", "mock"],
    author="Kevin Cheng, Keun Soo Yim",
    author_email="kevcheng@google.com, yim@google.com",
    description="Acloud is a command line tool that assists users to "
    "create an Android Virtual Device (AVD).",
    long_description=LONG_DESCRIPTION,
    long_description_content_type="text/markdown",
    packages=[
        "acloud",
        "acloud.internal",
        "acloud.public",
        "acloud.delete",
        "acloud.create",
        "acloud.setup",
        "acloud.metrics",
        "acloud.internal.lib",
        "acloud.internal.proto",
        "acloud.public.data",
        "acloud.public.acloud_kernel",
        "acloud.public.actions",
        "acloud.pull",
        "acloud.reconnect",
        "acloud.list",
    ],
    package_dir={"acloud": ".",},
    package_data={"acloud.public.data": ["default.config"]},
    include_package_data=True,
    platforms="POSIX",
    entry_points={"console_scripts": ["acloud=acloud.public.acloud_main:main"],},
    install_requires=[
        "google-api-python-client",
        "oauth2client==3.0.0",
        "protobuf",
        "python-dateutil",
    ],
    license="Apache License, Version 2.0",
    classifiers=[
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: Apache Software License",
        "Natural Language :: English",
        "Environment :: Console",
        "Intended Audience :: Developers",
        "Topic :: Utilities",
    ],
)
