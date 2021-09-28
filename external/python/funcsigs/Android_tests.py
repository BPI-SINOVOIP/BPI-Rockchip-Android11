# Copyright 2018 Google Inc. All rights reserved.
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

import os
import sys
import unittest

# this runs any test*.py files located anywhere in this subtree
if __name__ == '__main__':
  test_loader = unittest.defaultTestLoader
  # output is normal text formatting (as if running from a CLI), prints to stderr
  test_runner = unittest.TextTestRunner(stream=sys.stderr)

  # look at tests relative to the dir of this file.
  this_dir = os.path.dirname(os.path.abspath(__file__))

  # automagically find all tests recursively whose file name matches test*.py
  test_suite = test_loader.discover(this_dir, pattern='test*.py')

  # execute all the found tests
  test_runner.run(test_suite)

