# Copyright 2017, The Android Open Source Project
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
Base test runner class.

Class that other test runners will instantiate for test runners.
"""

from __future__ import print_function
import errno
import logging
import signal
import subprocess
import tempfile
import os
import sys

from collections import namedtuple

# pylint: disable=import-error
import atest_error
import atest_utils
import constants

OLD_OUTPUT_ENV_VAR = 'ATEST_OLD_OUTPUT'

# TestResult contains information of individual tests during a test run.
TestResult = namedtuple('TestResult', ['runner_name', 'group_name',
                                       'test_name', 'status', 'details',
                                       'test_count', 'test_time',
                                       'runner_total', 'group_total',
                                       'additional_info', 'test_run_name'])
ASSUMPTION_FAILED = 'ASSUMPTION_FAILED'
FAILED_STATUS = 'FAILED'
PASSED_STATUS = 'PASSED'
IGNORED_STATUS = 'IGNORED'
ERROR_STATUS = 'ERROR'

class TestRunnerBase(object):
    """Base Test Runner class."""
    NAME = ''
    EXECUTABLE = ''

    def __init__(self, results_dir, **kwargs):
        """Init stuff for base class."""
        self.results_dir = results_dir
        self.test_log_file = None
        if not self.NAME:
            raise atest_error.NoTestRunnerName('Class var NAME is not defined.')
        if not self.EXECUTABLE:
            raise atest_error.NoTestRunnerExecutable('Class var EXECUTABLE is '
                                                     'not defined.')
        if kwargs:
            logging.debug('ignoring the following args: %s', kwargs)

    def run(self, cmd, output_to_stdout=False, env_vars=None):
        """Shell out and execute command.

        Args:
            cmd: A string of the command to execute.
            output_to_stdout: A boolean. If False, the raw output of the run
                              command will not be seen in the terminal. This
                              is the default behavior, since the test_runner's
                              run_tests() method should use atest's
                              result reporter to print the test results.

                              Set to True to see the output of the cmd. This
                              would be appropriate for verbose runs.
            env_vars: Environment variables passed to the subprocess.
        """
        if not output_to_stdout:
            self.test_log_file = tempfile.NamedTemporaryFile(mode='w',
                                                             dir=self.results_dir,
                                                             delete=True)
        logging.debug('Executing command: %s', cmd)
        return subprocess.Popen(cmd, preexec_fn=os.setsid, shell=True,
                                stderr=subprocess.STDOUT, stdout=self.test_log_file,
                                env=env_vars)

    # pylint: disable=broad-except
    def handle_subprocess(self, subproc, func):
        """Execute the function. Interrupt the subproc when exception occurs.

        Args:
            subproc: A subprocess to be terminated.
            func: A function to be run.
        """
        try:
            signal.signal(signal.SIGINT, self._signal_passer(subproc))
            func()
        except Exception as error:
            # exc_info=1 tells logging to log the stacktrace
            logging.debug('Caught exception:', exc_info=1)
            # Remember our current exception scope, before new try block
            # Python3 will make this easier, the error itself stores
            # the scope via error.__traceback__ and it provides a
            # "raise from error" pattern.
            # https://docs.python.org/3.5/reference/simple_stmts.html#raise
            exc_type, exc_msg, traceback_obj = sys.exc_info()
            # If atest crashes, try to kill subproc group as well.
            try:
                logging.debug('Killing subproc: %s', subproc.pid)
                os.killpg(os.getpgid(subproc.pid), signal.SIGINT)
            except OSError:
                # this wipes our previous stack context, which is why
                # we have to save it above.
                logging.debug('Subproc already terminated, skipping')
            finally:
                if self.test_log_file:
                    with open(self.test_log_file.name, 'r') as f:
                        intro_msg = "Unexpected Issue. Raw Output:"
                        print(atest_utils.colorize(intro_msg, constants.RED))
                        print(f.read())
                # Ignore socket.recv() raising due to ctrl-c
                if not error.args or error.args[0] != errno.EINTR:
                    raise exc_type, exc_msg, traceback_obj

    def wait_for_subprocess(self, proc):
        """Check the process status. Interrupt the TF subporcess if user
        hits Ctrl-C.

        Args:
            proc: The tradefed subprocess.

        Returns:
            Return code of the subprocess for running tests.
        """
        try:
            logging.debug('Runner Name: %s, Process ID: %s', self.NAME, proc.pid)
            signal.signal(signal.SIGINT, self._signal_passer(proc))
            proc.wait()
            return proc.returncode
        except:
            # If atest crashes, kill TF subproc group as well.
            os.killpg(os.getpgid(proc.pid), signal.SIGINT)
            raise

    def _signal_passer(self, proc):
        """Return the signal_handler func bound to proc.

        Args:
            proc: The tradefed subprocess.

        Returns:
            signal_handler function.
        """
        def signal_handler(_signal_number, _frame):
            """Pass SIGINT to proc.

            If user hits ctrl-c during atest run, the TradeFed subprocess
            won't stop unless we also send it a SIGINT. The TradeFed process
            is started in a process group, so this SIGINT is sufficient to
            kill all the child processes TradeFed spawns as well.
            """
            logging.info('Ctrl-C received. Killing subprocess group')
            os.killpg(os.getpgid(proc.pid), signal.SIGINT)
        return signal_handler

    def run_tests(self, test_infos, extra_args, reporter):
        """Run the list of test_infos.

        Should contain code for kicking off the test runs using
        test_runner_base.run(). Results should be processed and printed
        via the reporter passed in.

        Args:
            test_infos: List of TestInfo.
            extra_args: Dict of extra args to add to test run.
            reporter: An instance of result_report.ResultReporter.
        """
        raise NotImplementedError

    def host_env_check(self):
        """Checks that host env has met requirements."""
        raise NotImplementedError

    def get_test_runner_build_reqs(self):
        """Returns a list of build targets required by the test runner."""
        raise NotImplementedError

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
        raise NotImplementedError
