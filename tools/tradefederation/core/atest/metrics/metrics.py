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
Metrics class.
"""

import constants

from . import metrics_base

class AtestStartEvent(metrics_base.MetricsBase):
    """
    Create Atest start event and send to clearcut.

    Usage:
        metrics.AtestStartEvent(
            command_line='example_atest_command',
            test_references=['example_test_reference'],
            cwd='example/working/dir',
            os='example_os')
    """
    _EVENT_NAME = 'atest_start_event'
    command_line = constants.INTERNAL
    test_references = constants.INTERNAL
    cwd = constants.INTERNAL
    os = constants.INTERNAL

class AtestExitEvent(metrics_base.MetricsBase):
    """
    Create Atest exit event and send to clearcut.

    Usage:
        metrics.AtestExitEvent(
            duration=metrics_utils.convert_duration(end-start),
            exit_code=0,
            stacktrace='some_trace',
            logs='some_logs')
    """
    _EVENT_NAME = 'atest_exit_event'
    duration = constants.EXTERNAL
    exit_code = constants.EXTERNAL
    stacktrace = constants.INTERNAL
    logs = constants.INTERNAL

class FindTestFinishEvent(metrics_base.MetricsBase):
    """
    Create find test finish event and send to clearcut.

    Occurs after a SINGLE test reference has been resolved to a test or
    not found.

    Usage:
        metrics.FindTestFinishEvent(
            duration=metrics_utils.convert_duration(end-start),
            success=true,
            test_reference='hello_world_test',
            test_finders=['example_test_reference', 'ref2'],
            test_info="test_name: hello_world_test -
                test_runner:AtestTradefedTestRunner -
                build_targets:
                    set(['MODULES-IN-platform_testing-tests-example-native']) -
                data:{'rel_config':
                    'platform_testing/tests/example/native/AndroidTest.xml',
                    'filter': frozenset([])} -
                suite:None - module_class: ['NATIVE_TESTS'] -
                install_locations:set(['device', 'host'])")
    """
    _EVENT_NAME = 'find_test_finish_event'
    duration = constants.EXTERNAL
    success = constants.EXTERNAL
    test_reference = constants.INTERNAL
    test_finders = constants.INTERNAL
    test_info = constants.INTERNAL

class BuildFinishEvent(metrics_base.MetricsBase):
    """
    Create build finish event and send to clearcut.

    Occurs after the build finishes, either successfully or not.

    Usage:
        metrics.BuildFinishEvent(
            duration=metrics_utils.convert_duration(end-start),
            success=true,
            targets=['target1', 'target2'])
    """
    _EVENT_NAME = 'build_finish_event'
    duration = constants.EXTERNAL
    success = constants.EXTERNAL
    targets = constants.INTERNAL

class RunnerFinishEvent(metrics_base.MetricsBase):
    """
    Create run finish event and send to clearcut.

    Occurs when a single test runner has completed.

    Usage:
        metrics.RunnerFinishEvent(
            duration=metrics_utils.convert_duration(end-start),
            success=true,
            runner_name='AtestTradefedTestRunner'
            test=[{name:'hello_world_test', result:0, stacktrace:''},
                  {name:'test2', result:1, stacktrace:'xxx'}])
    """
    _EVENT_NAME = 'runner_finish_event'
    duration = constants.EXTERNAL
    success = constants.EXTERNAL
    runner_name = constants.EXTERNAL
    test = constants.INTERNAL

class RunTestsFinishEvent(metrics_base.MetricsBase):
    """
    Create run tests finish event and send to clearcut.

    Occurs after all test runners and tests have finished.

    Usage:
        metrics.RunTestsFinishEvent(
            duration=metrics_utils.convert_duration(end-start))
    """
    _EVENT_NAME = 'run_tests_finish_event'
    duration = constants.EXTERNAL

class LocalDetectEvent(metrics_base.MetricsBase):
    """
    Create local detection event and send it to clearcut.

    Usage:
        metrics.LocalDetectEvent(
            detect_type=0,
            result=0)
    """
    _EVENT_NAME = 'local_detect_event'
    detect_type = constants.EXTERNAL
    result = constants.EXTERNAL
