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
Aggregates test runners, groups tests by test runners and kicks off tests.
"""

# pylint: disable=line-too-long
# pylint: disable=import-outside-toplevel

import itertools
import time
import traceback

import atest_error
import constants
import result_reporter

from metrics import metrics
from metrics import metrics_utils
from test_runners import atest_tf_test_runner
from test_runners import robolectric_test_runner
from test_runners import suite_plan_test_runner
from test_runners import vts_tf_test_runner

_TEST_RUNNERS = {
    atest_tf_test_runner.AtestTradefedTestRunner.NAME: atest_tf_test_runner.AtestTradefedTestRunner,
    robolectric_test_runner.RobolectricTestRunner.NAME: robolectric_test_runner.RobolectricTestRunner,
    suite_plan_test_runner.SuitePlanTestRunner.NAME: suite_plan_test_runner.SuitePlanTestRunner,
    vts_tf_test_runner.VtsTradefedTestRunner.NAME: vts_tf_test_runner.VtsTradefedTestRunner,
}


def _get_test_runners():
    """Returns the test runners.

    If external test runners are defined outside atest, they can be try-except
    imported into here.

    Returns:
        Dict of test runner name to test runner class.
    """
    test_runners_dict = _TEST_RUNNERS
    # Example import of example test runner:
    try:
        from test_runners import example_test_runner
        test_runners_dict[example_test_runner.ExampleTestRunner.NAME] = example_test_runner.ExampleTestRunner
    except ImportError:
        pass
    return test_runners_dict


def group_tests_by_test_runners(test_infos):
    """Group the test_infos by test runners

    Args:
        test_infos: List of TestInfo.

    Returns:
        List of tuples (test runner, tests).
    """
    tests_by_test_runner = []
    test_runner_dict = _get_test_runners()
    key = lambda x: x.test_runner
    sorted_test_infos = sorted(list(test_infos), key=key)
    for test_runner, tests in itertools.groupby(sorted_test_infos, key):
        # groupby returns a grouper object, we want to operate on a list.
        tests = list(tests)
        test_runner_class = test_runner_dict.get(test_runner)
        if test_runner_class is None:
            raise atest_error.UnknownTestRunnerError('Unknown Test Runner %s' %
                                                     test_runner)
        tests_by_test_runner.append((test_runner_class, tests))
    return tests_by_test_runner


def get_test_runner_reqs(module_info, test_infos):
    """Returns the requirements for all test runners specified in the tests.

    Args:
        module_info: ModuleInfo object.
        test_infos: List of TestInfo.

    Returns:
        Set of build targets required by the test runners.
    """
    dummy_result_dir = ''
    test_runner_build_req = set()
    for test_runner, _ in group_tests_by_test_runners(test_infos):
        test_runner_build_req |= test_runner(
            dummy_result_dir,
            module_info=module_info).get_test_runner_build_reqs()
    return test_runner_build_req


def run_all_tests(results_dir, test_infos, extra_args,
                  delay_print_summary=False):
    """Run the given tests.

    Args:
        results_dir: String directory to store atest results.
        test_infos: List of TestInfo.
        extra_args: Dict of extra args for test runners to use.

    Returns:
        0 if tests succeed, non-zero otherwise.
    """
    reporter = result_reporter.ResultReporter()
    reporter.print_starting_text()
    tests_ret_code = constants.EXIT_CODE_SUCCESS
    for test_runner, tests in group_tests_by_test_runners(test_infos):
        test_name = ' '.join([test.test_name for test in tests])
        test_start = time.time()
        is_success = True
        ret_code = constants.EXIT_CODE_TEST_FAILURE
        stacktrace = ''
        try:
            test_runner = test_runner(results_dir)
            ret_code = test_runner.run_tests(tests, extra_args, reporter)
            tests_ret_code |= ret_code
        # pylint: disable=broad-except
        except Exception:
            stacktrace = traceback.format_exc()
            reporter.runner_failure(test_runner.NAME, stacktrace)
            tests_ret_code = constants.EXIT_CODE_TEST_FAILURE
            is_success = False
        metrics.RunnerFinishEvent(
            duration=metrics_utils.convert_duration(time.time() - test_start),
            success=is_success,
            runner_name=test_runner.NAME,
            test=[{'name': test_name,
                   'result': ret_code,
                   'stacktrace': stacktrace}])
    if delay_print_summary:
        return tests_ret_code, reporter
    return (reporter.print_summary(extra_args.get(constants.COLLECT_TESTS_ONLY))
            or tests_ret_code, reporter)
