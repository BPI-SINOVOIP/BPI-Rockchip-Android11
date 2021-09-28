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
SUITE Tradefed test runner class.
"""

import copy
import logging

import atest_utils
import constants

from test_runners import atest_tf_test_runner

class SuitePlanTestRunner(atest_tf_test_runner.AtestTradefedTestRunner):
    """Suite Plan Test Runner class."""
    NAME = 'SuitePlanTestRunner'
    EXECUTABLE = '%s-tradefed'
    _RUN_CMD = ('{exe} run commandAndExit {test} {args}')

    def __init__(self, results_dir, **kwargs):
        """Init stuff for suite tradefed runner class."""
        super(SuitePlanTestRunner, self).__init__(results_dir, **kwargs)
        self.run_cmd_dict = {'exe': '',
                             'test': '',
                             'args': ''}

    def get_test_runner_build_reqs(self):
        """Return the build requirements.

        Returns:
            Set of build targets.
        """
        build_req = set()
        build_req |= super(SuitePlanTestRunner,
                           self).get_test_runner_build_reqs()
        return build_req

    def run_tests(self, test_infos, extra_args, reporter):
        """Run the list of test_infos.
        Args:
            test_infos: List of TestInfo.
            extra_args: Dict of extra args to add to test run.
            reporter: An instance of result_report.ResultReporter.

        Returns:
            Return code of the process for running tests.
        """
        reporter.register_unsupported_runner(self.NAME)
        run_cmds = self.generate_run_commands(test_infos, extra_args)
        ret_code = constants.EXIT_CODE_SUCCESS
        for run_cmd in run_cmds:
            proc = super(SuitePlanTestRunner, self).run(run_cmd,
                                                        output_to_stdout=True)
            ret_code |= self.wait_for_subprocess(proc)
        return ret_code

    def _parse_extra_args(self, extra_args):
        """Convert the extra args into something *ts-tf can understand.

        We want to transform the top-level args from atest into specific args
        that *ts-tradefed supports. The only arg we take as is
        EXTRA_ARG since that is what the user intentionally wants to pass to
        the test runner.

        Args:
            extra_args: Dict of args

        Returns:
            List of args to append.
        """
        args_to_append = []
        args_not_supported = []
        for arg in extra_args:
            if constants.SERIAL == arg:
                args_to_append.append('--serial')
                args_to_append.append(extra_args[arg])
                continue
            if constants.CUSTOM_ARGS == arg:
                args_to_append.extend(extra_args[arg])
                continue
            if constants.DRY_RUN == arg:
                continue
            args_not_supported.append(arg)
        if args_not_supported:
            logging.info('%s does not support the following args: %s',
                         self.EXECUTABLE, args_not_supported)
        return args_to_append

    # pylint: disable=arguments-differ
    def generate_run_commands(self, test_infos, extra_args):
        """Generate a list of run commands from TestInfos.

        Args:
            test_infos: List of TestInfo tests to run.
            extra_args: Dict of extra args to add to test run.

        Returns:
            A List of strings that contains the run command
            which *ts-tradefed supports.
        """
        cmds = []
        args = []
        args.extend(self._parse_extra_args(extra_args))
        args.extend(atest_utils.get_result_server_args())
        for test_info in test_infos:
            cmd_dict = copy.deepcopy(self.run_cmd_dict)
            cmd_dict['test'] = test_info.test_name
            cmd_dict['args'] = ' '.join(args)
            cmd_dict['exe'] = self.EXECUTABLE % test_info.suite
            cmds.append(self._RUN_CMD.format(**cmd_dict))
        return cmds
