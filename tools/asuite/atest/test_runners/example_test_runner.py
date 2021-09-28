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

"""Example test runner class."""


from test_runners import test_runner_base


class ExampleTestRunner(test_runner_base.TestRunnerBase):
    """Base Test Runner class."""
    NAME = 'ExampleTestRunner'
    EXECUTABLE = 'echo'
    _RUN_CMD = '{exe} ExampleTestRunner - test:{test}'
    _BUILD_REQ = set()

    # pylint: disable=unused-argument
    def run_tests(self, test_infos, extra_args, reporter):
        """Run the list of test_infos.

        Args:
            test_infos: List of TestInfo.
            extra_args: Dict of extra args to add to test run.
            reporter: An instance of result_report.ResultReporter
        """
        run_cmds = self.generate_run_commands(test_infos, extra_args)
        for run_cmd in run_cmds:
            super(ExampleTestRunner, self).run(run_cmd)

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
        return set()

    # pylint: disable=unused-argument
    def generate_run_commands(self, test_infos, extra_args, port=None):
        """Generate a list of run commands from TestInfos.

        Args:
            test_infos: A set of TestInfo instances.
            extra_args: A Dict of extra args to append.
            port: Optional. An int of the port number to send events to.
                  Subprocess reporter in TF won't try to connect if it's None.

        Returns:
            A list of run commands to run the tests.
        """
        run_cmds = []
        for test_info in test_infos:
            run_cmd_dict = {'exe': self.EXECUTABLE,
                            'test': test_info.test_name}
            run_cmds.extend(self._RUN_CMD.format(**run_cmd_dict))
        return run_cmds
