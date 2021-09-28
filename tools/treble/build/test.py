# Copyright 2019 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Runs all tests in this project.
"""

import unittest
import os
import sys

def run():
  start_dir = os.path.dirname(os.path.abspath(__file__))
  loader = unittest.TestLoader()
  suite = loader.discover(start_dir, pattern='*_test.py')

  runner = unittest.TextTestRunner(verbosity=2)
  result = runner.run(suite)
  sys.exit(not result.wasSuccessful())


if __name__ == '__main__':
  run()
