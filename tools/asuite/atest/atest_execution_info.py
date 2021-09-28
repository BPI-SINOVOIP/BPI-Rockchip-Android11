# Copyright 2019, The Android Open Source Project
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

"""ATest execution info generator."""

# pylint: disable=line-too-long

from __future__ import print_function

import glob
import logging
import json
import os
import sys

import atest_utils as au
import constants

from metrics import metrics_utils

_ARGS_KEY = 'args'
_STATUS_PASSED_KEY = 'PASSED'
_STATUS_FAILED_KEY = 'FAILED'
_STATUS_IGNORED_KEY = 'IGNORED'
_SUMMARY_KEY = 'summary'
_TOTAL_SUMMARY_KEY = 'total_summary'
_TEST_RUNNER_KEY = 'test_runner'
_TEST_NAME_KEY = 'test_name'
_TEST_TIME_KEY = 'test_time'
_TEST_DETAILS_KEY = 'details'
_TEST_RESULT_NAME = 'test_result'
_EXIT_CODE_ATTR = 'EXIT_CODE'
_MAIN_MODULE_KEY = '__main__'
_UUID_LEN = 30
_RESULT_LEN = 35
_COMMAND_LEN = 50
_LOGCAT_FMT = '{}/log/invocation_*/{}*logcat-on-failure*'

_SUMMARY_MAP_TEMPLATE = {_STATUS_PASSED_KEY : 0,
                         _STATUS_FAILED_KEY : 0,
                         _STATUS_IGNORED_KEY : 0,}

PREPARE_END_TIME = None


def preparation_time(start_time):
    """Return the preparation time.

    Args:
        start_time: The time.

    Returns:
        The preparation time if PREPARE_END_TIME is set, None otherwise.
    """
    return PREPARE_END_TIME - start_time if PREPARE_END_TIME else None


def symlink_latest_result(test_result_dir):
    """Make the symbolic link to latest result.

    Args:
        test_result_dir: A string of the dir path.
    """
    symlink = os.path.join(constants.ATEST_RESULT_ROOT, 'LATEST')
    if os.path.exists(symlink) or os.path.islink(symlink):
        os.remove(symlink)
    os.symlink(test_result_dir, symlink)


def print_test_result(root, history_arg):
    """Make a list of latest n test result.

    Args:
        root: A string of the test result root path.
        history_arg: A string of an integer or uuid. If it's an integer string,
                     the number of lines of test result will be given; else it
                     will be treated a uuid and print test result accordingly
                     in detail.
    """
    if not history_arg.isdigit():
        path = os.path.join(constants.ATEST_RESULT_ROOT, history_arg,
                            'test_result')
        print_test_result_by_path(path)
        return
    target = '%s/20*_*_*' % root
    paths = glob.glob(target)
    paths.sort(reverse=True)
    print('{:-^{uuid_len}} {:-^{result_len}} {:-^{command_len}}'
          .format('uuid', 'result', 'command',
                  uuid_len=_UUID_LEN,
                  result_len=_RESULT_LEN,
                  command_len=_COMMAND_LEN))
    for path in paths[0: int(history_arg)+1]:
        result_path = os.path.join(path, 'test_result')
        if os.path.isfile(result_path):
            try:
                with open(result_path) as json_file:
                    result = json.load(json_file)
                    total_summary = result.get(_TOTAL_SUMMARY_KEY, {})
                    summary_str = ', '.join([k+':'+str(v)
                                             for k, v in total_summary.items()])
                    print('{:<{uuid_len}} {:<{result_len}} atest {:<{command_len}}'
                          .format(os.path.basename(path),
                                  summary_str,
                                  result.get(_ARGS_KEY, ''),
                                  uuid_len=_UUID_LEN,
                                  result_len=_RESULT_LEN,
                                  command_len=_COMMAND_LEN))
            except ValueError:
                pass


def print_test_result_by_path(path):
    """Print latest test result.

    Args:
        path: A string of test result path.
    """
    if os.path.isfile(path):
        with open(path) as json_file:
            result = json.load(json_file)
            print("\natest {}".format(result.get(_ARGS_KEY, '')))
            print('\nTotal Summary:\n{}'.format(au.delimiter('-')))
            total_summary = result.get(_TOTAL_SUMMARY_KEY, {})
            print(', '.join([(k+':'+str(v))
                             for k, v in total_summary.items()]))
            fail_num = total_summary.get(_STATUS_FAILED_KEY)
            if fail_num > 0:
                message = '%d test failed' % fail_num
                print('\n')
                print(au.colorize(message, constants.RED))
                print('-' * len(message))
                test_runner = result.get(_TEST_RUNNER_KEY, {})
                for runner_name in test_runner.keys():
                    test_dict = test_runner.get(runner_name, {})
                    for test_name in test_dict:
                        test_details = test_dict.get(test_name, {})
                        for fail in test_details.get(_STATUS_FAILED_KEY):
                            print(au.colorize('{}'.format(
                                fail.get(_TEST_NAME_KEY)), constants.RED))
                            failure_files = glob.glob(_LOGCAT_FMT.format(
                                os.path.dirname(path), fail.get(_TEST_NAME_KEY)
                                ))
                            if failure_files:
                                print('{} {}'.format(
                                    au.colorize('LOGCAT-ON-FAILURES:',
                                                constants.CYAN),
                                    failure_files[0]))
                            print('{} {}'.format(
                                au.colorize('STACKTRACE:\n', constants.CYAN),
                                fail.get(_TEST_DETAILS_KEY)))


def has_non_test_options(args):
    """
    check whether non-test option in the args.

    Args:
        args: An argspace.Namespace class instance holding parsed args.

    Returns:
        True, if args has at least one non-test option.
        False, otherwise.
    """
    return (args.collect_tests_only
            or args.dry_run
            or args.help
            or args.history
            or args.info
            or args.version
            or args.latest_result)


class AtestExecutionInfo:
    """Class that stores the whole test progress information in JSON format.

    ----
    For example, running command
        atest hello_world_test HelloWorldTest

    will result in storing the execution detail in JSON:
    {
      "args": "hello_world_test HelloWorldTest",
      "test_runner": {
          "AtestTradefedTestRunner": {
              "hello_world_test": {
                  "FAILED": [
                      {"test_time": "(5ms)",
                       "details": "Hello, Wor...",
                       "test_name": "HelloWorldTest#PrintHelloWorld"}
                      ],
                  "summary": {"FAILED": 1, "PASSED": 0, "IGNORED": 0}
              },
              "HelloWorldTests": {
                  "PASSED": [
                      {"test_time": "(27ms)",
                       "details": null,
                       "test_name": "...HelloWorldTest#testHalloWelt"},
                      {"test_time": "(1ms)",
                       "details": null,
                       "test_name": "....HelloWorldTest#testHelloWorld"}
                      ],
                  "summary": {"FAILED": 0, "PASSED": 2, "IGNORED": 0}
              }
          }
      },
      "total_summary": {"FAILED": 1, "PASSED": 2, "IGNORED": 0}
    }
    """

    result_reporters = []

    def __init__(self, args, work_dir, args_ns):
        """Initialise an AtestExecutionInfo instance.

        Args:
            args: Command line parameters.
            work_dir: The directory for saving information.
            args_ns: An argspace.Namespace class instance holding parsed args.

        Returns:
               A json format string.
        """
        self.args = args
        self.work_dir = work_dir
        self.result_file = None
        self.args_ns = args_ns

    def __enter__(self):
        """Create and return information file object."""
        full_file_name = os.path.join(self.work_dir, _TEST_RESULT_NAME)
        try:
            self.result_file = open(full_file_name, 'w')
        except IOError:
            logging.error('Cannot open file %s', full_file_name)
        return self.result_file

    def __exit__(self, exit_type, value, traceback):
        """Write execution information and close information file."""
        if self.result_file:
            self.result_file.write(AtestExecutionInfo.
                                   _generate_execution_detail(self.args))
            self.result_file.close()
            if not has_non_test_options(self.args_ns):
                symlink_latest_result(self.work_dir)
        main_module = sys.modules.get(_MAIN_MODULE_KEY)
        main_exit_code = getattr(main_module, _EXIT_CODE_ATTR,
                                 constants.EXIT_CODE_ERROR)
        if main_exit_code == constants.EXIT_CODE_SUCCESS:
            metrics_utils.send_exit_event(main_exit_code)
        else:
            metrics_utils.handle_exc_and_send_exit_event(main_exit_code)

    @staticmethod
    def _generate_execution_detail(args):
        """Generate execution detail.

        Args:
            args: Command line parameters that you want to save.

        Returns:
            A json format string.
        """
        info_dict = {_ARGS_KEY: ' '.join(args)}
        try:
            AtestExecutionInfo._arrange_test_result(
                info_dict,
                AtestExecutionInfo.result_reporters)
            return json.dumps(info_dict)
        except ValueError as err:
            logging.warning('Parsing test result failed due to : %s', err)

    @staticmethod
    def _arrange_test_result(info_dict, reporters):
        """Append test result information in given dict.

        Arrange test information to below
        "test_runner": {
            "test runner name": {
                "test name": {
                    "FAILED": [
                        {"test time": "",
                         "details": "",
                         "test name": ""}
                    ],
                "summary": {"FAILED": 0, "PASSED": 0, "IGNORED": 0}
                },
            },
        "total_summary": {"FAILED": 0, "PASSED": 0, "IGNORED": 0}

        Args:
            info_dict: A dict you want to add result information in.
            reporters: A list of result_reporter.

        Returns:
            A dict contains test result information data.
        """
        info_dict[_TEST_RUNNER_KEY] = {}
        for reporter in reporters:
            for test in reporter.all_test_results:
                runner = info_dict[_TEST_RUNNER_KEY].setdefault(
                    test.runner_name, {})
                group = runner.setdefault(test.group_name, {})
                result_dict = {_TEST_NAME_KEY : test.test_name,
                               _TEST_TIME_KEY : test.test_time,
                               _TEST_DETAILS_KEY : test.details}
                group.setdefault(test.status, []).append(result_dict)

        total_test_group_summary = _SUMMARY_MAP_TEMPLATE.copy()
        for runner in info_dict[_TEST_RUNNER_KEY]:
            for group in info_dict[_TEST_RUNNER_KEY][runner]:
                group_summary = _SUMMARY_MAP_TEMPLATE.copy()
                for status in info_dict[_TEST_RUNNER_KEY][runner][group]:
                    count = len(info_dict[_TEST_RUNNER_KEY][runner][group][status])
                    if status in _SUMMARY_MAP_TEMPLATE:
                        group_summary[status] = count
                        total_test_group_summary[status] += count
                info_dict[_TEST_RUNNER_KEY][runner][group][_SUMMARY_KEY] = group_summary
        info_dict[_TOTAL_SUMMARY_KEY] = total_test_group_summary
        return info_dict
