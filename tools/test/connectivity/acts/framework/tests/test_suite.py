#!/usr/bin/env python3
#
#   Copyright 2017 - The Android Open Source Project
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
import tempfile
import unittest
import multiprocessing


def run_tests(test_suite, output_file):
    # Redirects stdout and stderr to the given output file.
    new_stdout = open(output_file, 'w+')
    os.dup2(new_stdout.fileno(), 1)
    test_run = unittest.TextTestRunner(stream=new_stdout, verbosity=2).run(test_suite)
    return test_run.wasSuccessful()


class TestResult(object):
    def __init__(self, process_result, output_file, test_suite):
        self.process_result = process_result
        self.output_file = output_file
        self.test_suite = test_suite


def run_all_unit_tests():
    # Due to some incredibly powerful black magic, running this twice
    # causes the metrics/, test_utils/ and test_runner_test.py tests to load
    # properly. They do no load properly the first time.
    suite = unittest.TestLoader().discover(
        start_dir=os.path.dirname(__file__), pattern='*_test.py')
    suite = unittest.TestLoader().discover(
        start_dir=os.path.dirname(__file__), pattern='*_test.py')

    process_pool = multiprocessing.Pool(10)
    output_dir = tempfile.mkdtemp()

    results = []

    for index, test in enumerate(suite._tests):
        output_file = os.path.join(output_dir, 'test_%s.output' % index)
        process_result = process_pool.apply_async(run_tests,
                                                  args=(test, output_file))
        results.append(TestResult(process_result, output_file, test))

    success = True
    for index, result in enumerate(results):
        try:
            if not result.process_result.get(timeout=60):
                success = False
                print('Received the following test failure:')
                with open(result.output_file, 'r') as out_file:
                    print(out_file.read(), file=sys.stderr)
        except multiprocessing.TimeoutError:
            success = False
            print('The following test timed out: %r' % result.test_suite,
                  file=sys.stderr)
            with open(result.output_file, 'r') as out_file:
                print(out_file.read())

    exit(not success)


if __name__ == '__main__':
    run_all_unit_tests()
