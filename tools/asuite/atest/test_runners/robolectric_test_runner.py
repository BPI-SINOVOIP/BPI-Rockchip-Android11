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
Robolectric test runner class.

This test runner will be short lived, once robolectric support v2 is in, then
robolectric tests will be invoked through AtestTFTestRunner.
"""

# pylint: disable=line-too-long

import json
import logging
import os
import re
import tempfile
import time

from functools import partial

import atest_utils
import constants

from test_runners import test_runner_base
from .event_handler import EventHandler

POLL_FREQ_SECS = 0.1
# A pattern to match event like below
#TEST_FAILED {'className':'SomeClass', 'testName':'SomeTestName',
#            'trace':'{"trace":"AssertionError: <true> is equal to <false>\n
#               at FailureStrategy.fail(FailureStrategy.java:24)\n
#               at FailureStrategy.fail(FailureStrategy.java:20)\n"}\n\n
EVENT_RE = re.compile(r'^(?P<event_name>[A-Z_]+) (?P<json_data>{(.\r*|\n)*})(?:\n|$)')


class RobolectricTestRunner(test_runner_base.TestRunnerBase):
    """Robolectric Test Runner class."""
    NAME = 'RobolectricTestRunner'
    # We don't actually use EXECUTABLE because we're going to use
    # atest_utils.build to kick off the test but if we don't set it, the base
    # class will raise an exception.
    EXECUTABLE = 'make'

    # pylint: disable=useless-super-delegation
    def __init__(self, results_dir, **kwargs):
        """Init stuff for robolectric runner class."""
        super(RobolectricTestRunner, self).__init__(results_dir, **kwargs)
        self.is_verbose = logging.getLogger().isEnabledFor(logging.DEBUG)

    def run_tests(self, test_infos, extra_args, reporter):
        """Run the list of test_infos. See base class for more.

        Args:
            test_infos: A list of TestInfos.
            extra_args: Dict of extra args to add to test run.
            reporter: An instance of result_report.ResultReporter.

        Returns:
            0 if tests succeed, non-zero otherwise.
        """
        if os.getenv(test_runner_base.OLD_OUTPUT_ENV_VAR):
            return self.run_tests_raw(test_infos, extra_args, reporter)
        return self.run_tests_pretty(test_infos, extra_args, reporter)

    def run_tests_raw(self, test_infos, extra_args, reporter):
        """Run the list of test_infos with raw output.

        Args:
            test_infos: List of TestInfo.
            extra_args: Dict of extra args to add to test run.
            reporter: A ResultReporter Instance.

        Returns:
            0 if tests succeed, non-zero otherwise.
        """
        reporter.register_unsupported_runner(self.NAME)
        ret_code = constants.EXIT_CODE_SUCCESS
        for test_info in test_infos:
            full_env_vars = self._get_full_build_environ(test_info,
                                                         extra_args)
            run_cmd = self.generate_run_commands([test_info], extra_args)[0]
            subproc = self.run(run_cmd,
                               output_to_stdout=self.is_verbose,
                               env_vars=full_env_vars)
            ret_code |= self.wait_for_subprocess(subproc)
        return ret_code

    def run_tests_pretty(self, test_infos, extra_args, reporter):
        """Run the list of test_infos with pretty output mode.

        Args:
            test_infos: List of TestInfo.
            extra_args: Dict of extra args to add to test run.
            reporter: A ResultReporter Instance.

        Returns:
            0 if tests succeed, non-zero otherwise.
        """
        ret_code = constants.EXIT_CODE_SUCCESS
        for test_info in test_infos:
            # Create a temp communication file.
            with tempfile.NamedTemporaryFile(dir=self.results_dir) as event_file:
                # Prepare build environment parameter.
                full_env_vars = self._get_full_build_environ(test_info,
                                                             extra_args,
                                                             event_file)
                run_cmd = self.generate_run_commands([test_info], extra_args)[0]
                subproc = self.run(run_cmd,
                                   output_to_stdout=self.is_verbose,
                                   env_vars=full_env_vars)
                event_handler = EventHandler(reporter, self.NAME)
                # Start polling.
                self.handle_subprocess(subproc,
                                       partial(self._exec_with_robo_polling,
                                               event_file,
                                               subproc,
                                               event_handler))
                ret_code |= self.wait_for_subprocess(subproc)
        return ret_code

    def _get_full_build_environ(self, test_info=None, extra_args=None,
                                event_file=None):
        """Helper to get full build environment.

       Args:
           test_info: TestInfo object.
           extra_args: Dict of extra args to add to test run.
           event_file: A file-like object that can be used as a temporary
                       storage area.
       """
        full_env_vars = os.environ.copy()
        env_vars = self.generate_env_vars(test_info,
                                          extra_args,
                                          event_file)
        full_env_vars.update(env_vars)
        return full_env_vars

    def _exec_with_robo_polling(self, communication_file, robo_proc,
                                event_handler):
        """Polling data from communication file

        Polling data from communication file. Exit when communication file
        is empty and subprocess ended.

        Args:
            communication_file: A monitored communication file.
            robo_proc: The build process.
            event_handler: A file-like object storing the events of robolectric tests.
        """
        buf = ''
        while True:
            # Make sure that ATest gets content from current position.
            communication_file.seek(0, 1)
            data = communication_file.read()
            if isinstance(data, bytes):
                data = data.decode()
            buf += data
            reg = re.compile(r'(.|\n)*}\n\n')
            if not reg.match(buf) or data == '':
                if robo_proc.poll() is not None:
                    logging.debug('Build process exited early')
                    return
                time.sleep(POLL_FREQ_SECS)
            else:
                # Read all new data and handle it at one time.
                for event in re.split(r'\n\n', buf):
                    match = EVENT_RE.match(event)
                    if match:
                        try:
                            event_data = json.loads(match.group('json_data'),
                                                    strict=False)
                        except ValueError:
                            # Parse event fail, continue to parse next one.
                            logging.debug('"%s" is not valid json format.',
                                          match.group('json_data'))
                            continue
                        event_name = match.group('event_name')
                        event_handler.process_event(event_name, event_data)
                buf = ''

    @staticmethod
    def generate_env_vars(test_info, extra_args, event_file=None):
        """Turn the args into env vars.

        Robolectric tests specify args through env vars, so look for class
        filters and debug args to apply to the env.

        Args:
            test_info: TestInfo class that holds the class filter info.
            extra_args: Dict of extra args to apply for test run.
            event_file: A file-like object storing the events of robolectric
            tests.

        Returns:
            Dict of env vars to pass into invocation.
        """
        env_var = {}
        for arg in extra_args:
            if constants.WAIT_FOR_DEBUGGER == arg:
                env_var['DEBUG_ROBOLECTRIC'] = 'true'
                continue
        filters = test_info.data.get(constants.TI_FILTER)
        if filters:
            robo_filter = next(iter(filters))
            env_var['ROBOTEST_FILTER'] = robo_filter.class_name
            if robo_filter.methods:
                logging.debug('method filtering not supported for robolectric '
                              'tests yet.')
        if event_file:
            env_var['EVENT_FILE_ROBOLECTRIC'] = event_file.name
        return env_var

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
            robo_command = atest_utils.get_build_cmd() + [str(test_info.test_name)]
            run_cmd = ' '.join(x for x in robo_command)
            if constants.DRY_RUN in extra_args:
                run_cmd = run_cmd.replace(
                    os.environ.get(constants.ANDROID_BUILD_TOP) + os.sep, '')
            run_cmds.append(run_cmd)
        return run_cmds
