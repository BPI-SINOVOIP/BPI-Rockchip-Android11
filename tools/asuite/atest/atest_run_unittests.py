#!/usr/bin/env python3
#
# Copyright 2018 The Android Open Source Project
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

"""Main entrypoint for all of atest's unittest."""

# pylint: disable=line-too-long

import logging
import os
import sys
import unittest

from importlib import import_module

# Setup logging to be silent so unittests can pass through TF.
logging.disable(logging.ERROR)

def get_test_modules():
    """Returns a list of testable modules.

    Finds all the test files (*_unittest.py) and get their no-absolute
    path (internal/lib/utils_test.py) and translate it to an import path and
    strip the py ext (internal.lib.utils_test).

    Returns:
        List of strings (the testable module import path).
    """
    testable_modules = []
    base_path = os.path.dirname(os.path.realpath(__file__))

    for dirpath, _, files in os.walk(base_path):
        for f in files:
            if f.endswith("_unittest.py"):
                # Now transform it into a no-absolute import path.
                full_file_path = os.path.join(dirpath, f)
                rel_file_path = os.path.relpath(full_file_path, base_path)
                rel_file_path, _ = os.path.splitext(rel_file_path)
                rel_file_path = rel_file_path.replace(os.sep, ".")
                testable_modules.append(rel_file_path)

    return testable_modules

def main(_):
    """Main unittest entry.

    Args:
        argv: A list of system arguments. (unused)

    Returns:
        0 if success. None-zero if fails.
    """
    test_modules = get_test_modules()
    for mod in test_modules:
        import_module(mod)

    loader = unittest.defaultTestLoader
    test_suite = loader.loadTestsFromNames(test_modules)
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(test_suite)
    sys.exit(not result.wasSuccessful())


if __name__ == '__main__':
    main(sys.argv[1:])
