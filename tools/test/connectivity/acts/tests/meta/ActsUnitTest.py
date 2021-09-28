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
import subprocess
import sys

from mobly import config_parser as mobly_config_parser

import acts
from acts import base_test
from acts import signals

# The number of seconds to wait before considering the test to have timed out.
TIMEOUT = 60


class ActsUnitTest(base_test.BaseTestClass):
    """A class to run the ACTS unit tests in parallel.

    This is a hack to run the ACTS unit tests through CI. Please use the main
    function below if you need to run these tests.
    """

    def test_units(self):
        """Runs all the ACTS unit tests in test_suite.py."""
        test_script = os.path.join(os.path.dirname(acts.__path__[0]),
                                   'tests/test_suite.py')
        test_process = subprocess.Popen([sys.executable, test_script],
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT)

        killed = False
        try:
            stdout, _ = test_process.communicate(timeout=TIMEOUT)
        except subprocess.TimeoutExpired:
            killed = True
            self.log.error('Test %s timed out after %s seconds.' %
                           (test_process.args, TIMEOUT))
            test_process.kill()
            stdout, _ = test_process.communicate()

        if test_process.returncode != 0 or killed:
            self.log.error('=' * 79)
            self.log.error('Test %s failed with error %s.' %
                           (test_process.args, test_process.returncode))
            self.log.error('=' * 79)
            self.log.error('Failure for `%s`:\n%s' %
                           (test_process.args,
                            stdout.decode('utf-8', errors='replace')))
            raise signals.TestFailure(
                'One or more unit tests failed. See the logs.')
        else:
            self.log.debug('Output for `%s`:\n%s' %
                           (test_process.args,
                            stdout.decode('utf-8', errors='replace')))


def main():
    test_run_config = mobly_config_parser.TestRunConfig()
    test_run_config.testbed_name = 'UnitTests'
    test_run_config.log_path = ''
    ActsUnitTest(test_run_config).test_units()


if __name__ == '__main__':
    main()
