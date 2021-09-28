#!/usr/bin/env python
#
# Copyright (C) 2019 The Android Open Source Project
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

import logging
import os
import subprocess
import sys
import tempfile
import zipfile

THIS_DIR = os.path.realpath(os.path.dirname(__file__))


def logger():
    """Returns the module level logger."""
    return logging.getLogger(__name__)


def unchecked_call(cmd, *args, **kwargs):
    """subprocess.call with logging."""
    logger().info('unchecked_call: %s', subprocess.list2cmdline(cmd))
    return subprocess.call(cmd, *args, **kwargs)


def check_call(cmd, *args, **kwargs):
    """subprocess.check_call with logging."""
    logger().info('check_call: %s', subprocess.list2cmdline(cmd))
    subprocess.check_call(cmd, *args, **kwargs)


def android_build_top():
    return os.path.realpath(os.path.join(THIS_DIR, '../../..'))


def clang_build():
    gofile = os.path.join(android_build_top(), 'build', 'soong', 'cc', 'config', 'global.go')
    try:
        lines = file(gofile).readlines()
        versionLine = [l for l in lines if 'ClangDefaultVersion' in l][0]
        start, end = versionLine.index('"'), versionLine.rindex('"')
        return versionLine[start + 1: end]
    except Exception as err:
        raise RuntimeError("Extracting Clang version failed with {0}".format(err))


def llvm_profdata():
    return os.path.join(android_build_top(), 'prebuilts', 'clang', 'host',
                        'linux-x86', clang_build(), 'bin', 'llvm-profdata')


def run_llvm_profdata(inputs, output):
    check_call([llvm_profdata(), 'merge', '-output=' + output] + inputs)
