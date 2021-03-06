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

import os
import sys
import unittest


def main():
    suite = unittest.TestLoader().discover(
        start_dir=os.path.dirname(__file__), pattern='*_test.py')
    return suite


if __name__ == '__main__':
    test_suite = main()
    runner = unittest.TextTestRunner()
    test_run = runner.run(test_suite)
    sys.exit(not test_run.wasSuccessful())
