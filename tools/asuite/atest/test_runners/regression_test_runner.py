# Copyright 2018, The Android Open Source Project
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

"""
Regression Detection test runner class.
"""

import constants

from test_runners import test_runner_base


class RegressionTestRunner(test_runner_base.TestRunnerBase):
    """Regression Test Runner class."""
    NAME = 'RegressionTestRunner'
    EXECUTABLE = 'tradefed.sh'
    _RUN_CMD = '{exe} run commandAndExit regression -n {args}'
    _BUILD_REQ = {'tradefed-core', constants.ATEST_TF_MODULE}

    def __init__(self, results_dir):
        """Init stuff for base class."""
        super(RegressionTestRunner, self).__init__(results_dir)
        self.run_cmd_dict = {'exe': self.EXECUTABLE,
                             'args': ''}

    # pylint: disable=unused-argument
    def run_tests(self, test_infos, extra_args, reporter):
        """Run the list of test_infos.

        Args:
            test_infos: List of TestInfo.
            extra_args: Dict of args to add to regression detection test run.
            reporter: A ResultReporter instance.

        Returns:
            Return code of the process for running tests.
        """
        run_cmds = self.generate_run_commands(test_infos, extra_args)
        proc = super(RegressionTestRunner, self).run(run_cmds[0],
                                                     output_to_stdout=True)
        proc.wait()
        return proc.returncode

    # pylint: disable=unnecessary-pass
    # Please keep above disable flag to ensure host_env_check is overriden.
    def host_env_check(self):
        """Check that host env has everything we need.

        We actually can assume the host env is fine because we have the same
        requirements that atest has. Update this to check for android env vars
        if that changes.
        """
        pass

    def get_test_runner_build_reqs(self):
        """Return the build requirements.

        Returns:
            Set of build targets.
        """
        return self._BUILD_REQ

    # pylint: disable=unused-argument
    def generate_run_commands(self, test_infos, extra_args, port=None):
        """Generate a list of run commands from TestInfos.

        Args:
            test_infos: A set of TestInfo instances.
            extra_args: A Dict of extra args to append.
            port: Optional. An int of the port number to send events to.
                  Subprocess reporter in TF won't try to connect if it's None.

        Returns:
            A list that contains the string of atest tradefed run command.
            Only one command is returned.
        """
        pre = extra_args.pop(constants.PRE_PATCH_FOLDER)
        post = extra_args.pop(constants.POST_PATCH_FOLDER)
        args = ['--pre-patch-metrics', pre, '--post-patch-metrics', post]
        self.run_cmd_dict['args'] = ' '.join(args)
        run_cmd = self._RUN_CMD.format(**self.run_cmd_dict)
        return [run_cmd]
