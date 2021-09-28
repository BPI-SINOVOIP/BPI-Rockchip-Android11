#!/usr/bin/env python
#
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
Atest Argument Parser class for atest.
"""

# pylint: disable=line-too-long

import argparse
import pydoc

import constants

# Constants used for AtestArgParser and EPILOG_TEMPLATE
HELP_DESC = ('A command line tool that allows users to build, install, and run '
             'Android tests locally, greatly speeding test re-runs without '
             'requiring knowledge of Trade Federation test harness command line'
             ' options.')

# Constants used for arg help message(sorted in alphabetic)
ALL_ABI = 'Set to run tests for all abis.'
BUILD = 'Run a build.'
CLEAR_CACHE = 'Wipe out the test_infos cache of the test.'
COLLECT_TESTS_ONLY = ('Collect a list test cases of the instrumentation tests '
                      'without testing them in real.')
DISABLE_TEARDOWN = 'Disable test teardown and cleanup.'
DRY_RUN = 'Dry run atest without building, installing and running tests in real.'
ENABLE_FILE_PATTERNS = 'Enable FILE_PATTERNS in TEST_MAPPING.'
HISTORY = ('Show test results in chronological order(with specified number or '
           'all by default).')
HOST = ('Run the test completely on the host without a device. '
        '(Note: running a host test that requires a device without '
        '--host will fail.)')
INCLUDE_SUBDIRS = 'Search TEST_MAPPING files in subdirs as well.'
INFO = 'Show module information.'
INSTALL = 'Install an APK.'
INSTANT = ('Run the instant_app version of the module if the module supports it. '
           'Note: Nothing\'s going to run if it\'s not an Instant App test and '
           '"--instant" is passed.')
ITERATION = 'Loop-run tests until the max iteration is reached. (10 by default)'
LATEST_RESULT = 'Print latest test result.'
LIST_MODULES = 'List testable modules for the given suite.'
NO_METRICS = 'Do not send metrics.'
REBUILD_MODULE_INFO = ('Forces a rebuild of the module-info.json file. '
                       'This may be necessary following a repo sync or '
                       'when writing a new test.')
RERUN_UNTIL_FAILURE = ('Rerun all tests until a failure occurs or the max '
                       'iteration is reached. (10 by default)')
RETRY_ANY_FAILURE = ('Rerun failed tests until passed or the max iteration '
                     'is reached. (10 by default)')
SERIAL = 'The device to run the test on.'
TEST = ('Run the tests. WARNING: Many test configs force cleanup of device '
        'after test run. In this case, "-d" must be used in previous test run to '
        'disable cleanup for "-t" to work. Otherwise, device will need to be '
        'setup again with "-i".')
TEST_MAPPING = 'Run tests defined in TEST_MAPPING files.'
TF_TEMPLATE = ('Add extra tradefed template for ATest suite, '
               'e.g. atest <test> --tf-template <template_key>=<template_path>')
TF_DEBUG = 'Enable tradefed debug mode with a specify port. Default value is 10888.'
SHARDING = 'Option to specify sharding count. The default value is 2'
UPDATE_CMD_MAPPING = ('Update the test command of input tests. Warning: result '
                      'will be saved under tools/tradefederation/core/atest/test_data.')
USER_TYPE = 'Run test with specific user type, e.g. atest <test> --user-type secondary_user'
VERBOSE = 'Display DEBUG level logging.'
VERIFY_CMD_MAPPING = 'Verify the test command of input tests.'
VERSION = 'Display version string.'
WAIT_FOR_DEBUGGER = 'Wait for debugger prior to execution (Instrumentation tests only).'

def _positive_int(value):
    """Verify value by whether or not a positive integer.

    Args:
        value: A string of a command-line argument.

    Returns:
        int of value, if it is an positive integer.
        Otherwise, raise argparse.ArgumentTypeError.
    """
    err_msg = "invalid positive int value: '%s'" % value
    try:
        converted_value = int(value)
        if converted_value < 1:
            raise argparse.ArgumentTypeError(err_msg)
        return converted_value
    except ValueError:
        raise argparse.ArgumentTypeError(err_msg)


class AtestArgParser(argparse.ArgumentParser):
    """Atest wrapper of ArgumentParser."""

    def __init__(self):
        """Initialise an ArgumentParser instance."""
        super(AtestArgParser, self).__init__(
            description=HELP_DESC, add_help=False)

    def add_atest_args(self):
        """A function that does ArgumentParser.add_argument()"""
        self.add_argument('tests', nargs='*', help='Tests to build and/or run.')
        # Options that to do with testing.
        self.add_argument('-a', '--all-abi', action='store_true', help=ALL_ABI)
        self.add_argument('-b', '--build', action='append_const', dest='steps',
                          const=constants.BUILD_STEP, help=BUILD)
        self.add_argument('-d', '--disable-teardown', action='store_true',
                          help=DISABLE_TEARDOWN)
        self.add_argument('--host', action='store_true', help=HOST)
        self.add_argument('-i', '--install', action='append_const',
                          dest='steps', const=constants.INSTALL_STEP,
                          help=INSTALL)
        self.add_argument('-m', constants.REBUILD_MODULE_INFO_FLAG,
                          action='store_true', help=REBUILD_MODULE_INFO)
        self.add_argument('-s', '--serial', help=SERIAL)
        self.add_argument('--sharding', nargs='?', const=2,
                          type=_positive_int, default=0,
                          help=SHARDING)
        self.add_argument('-t', '--test', action='append_const', dest='steps',
                          const=constants.TEST_STEP, help=TEST)
        self.add_argument('-w', '--wait-for-debugger', action='store_true',
                          help=WAIT_FOR_DEBUGGER)

        # Options related to Test Mapping
        self.add_argument('-p', '--test-mapping', action='store_true',
                          help=TEST_MAPPING)
        self.add_argument('--include-subdirs', action='store_true',
                          help=INCLUDE_SUBDIRS)
        # TODO(146980564): Remove enable-file-patterns when support
        # file-patterns in TEST_MAPPING by default.
        self.add_argument('--enable-file-patterns', action='store_true',
                          help=ENABLE_FILE_PATTERNS)

        # Options for information queries and dry-runs:
        # A group of options for dry-runs. They are mutually exclusive
        # in a command line.
        group = self.add_mutually_exclusive_group()
        group.add_argument('--collect-tests-only', action='store_true',
                           help=COLLECT_TESTS_ONLY)
        group.add_argument('--dry-run', action='store_true', help=DRY_RUN)
        self.add_argument('-h', '--help', action='store_true',
                          help='Print this help message.')
        self.add_argument('--info', action='store_true', help=INFO)
        self.add_argument('-L', '--list-modules', help=LIST_MODULES)
        self.add_argument('-v', '--verbose', action='store_true', help=VERBOSE)
        self.add_argument('-V', '--version', action='store_true', help=VERSION)

        # Obsolete options that will be removed soon.
        self.add_argument('--generate-baseline', nargs='?',
                          type=int, const=5, default=0,
                          help='Generate baseline metrics, run 5 iterations by'
                               'default. Provide an int argument to specify '
                               '# iterations.')
        self.add_argument('--generate-new-metrics', nargs='?',
                          type=int, const=5, default=0,
                          help='Generate new metrics, run 5 iterations by '
                               'default. Provide an int argument to specify '
                               '# iterations.')
        self.add_argument('--detect-regression', nargs='*',
                          help='Run regression detection algorithm. Supply '
                               'path to baseline and/or new metrics folders.')

        # Options related to module parameterization
        self.add_argument('--instant', action='store_true', help=INSTANT)
        self.add_argument('--user-type', help=USER_TYPE)

        # Option for dry-run command mapping result and cleaning cache.
        self.add_argument('-c', '--clear-cache', action='store_true',
                          help=CLEAR_CACHE)
        self.add_argument('-u', '--update-cmd-mapping', action='store_true',
                          help=UPDATE_CMD_MAPPING)
        self.add_argument('-y', '--verify-cmd-mapping', action='store_true',
                          help=VERIFY_CMD_MAPPING)
        # Options for Tradefed debug mode.
        self.add_argument('-D', '--tf-debug', nargs='?', const=10888,
                          type=_positive_int, default=0,
                          help=TF_DEBUG)
        # Options for Tradefed customization related.
        self.add_argument('--tf-template', action='append',
                          help=TF_TEMPLATE)

        # A group of options for rerun strategy. They are mutually exclusive
        # in a command line.
        group = self.add_mutually_exclusive_group()
        # Option for rerun tests for the specified number iterations.
        group.add_argument('--iterations', nargs='?',
                           type=_positive_int, const=10, default=0,
                           metavar='MAX_ITERATIONS', help=ITERATION)
        group.add_argument('--rerun-until-failure', nargs='?',
                           type=_positive_int, const=10, default=0,
                           metavar='MAX_ITERATIONS', help=RERUN_UNTIL_FAILURE)
        group.add_argument('--retry-any-failure', nargs='?',
                           type=_positive_int, const=10, default=0,
                           metavar='MAX_ITERATIONS', help=RETRY_ANY_FAILURE)

        # A group of options for history. They are mutually exclusive
        # in a command line.
        history_group = self.add_mutually_exclusive_group()
        # History related options.
        history_group.add_argument('--latest-result', action='store_true',
                                   help=LATEST_RESULT)
        history_group.add_argument('--history', nargs='?', const='99999',
                                   help=HISTORY)

        # Options for disabling collecting data for metrics.
        self.add_argument(constants.NO_METRICS_ARG, action='store_true',
                          help=NO_METRICS)

        # This arg actually doesn't consume anything, it's primarily used for
        # the help description and creating custom_args in the NameSpace object.
        self.add_argument('--', dest='custom_args', nargs='*',
                          help='Specify custom args for the test runners. '
                               'Everything after -- will be consumed as '
                               'custom args.')

    def get_args(self):
        """This method is to get args from actions and return optional args.

        Returns:
            A list of optional arguments.
        """
        argument_list = []
        # The output of _get_optional_actions(): [['-t', '--test'], [--info]]
        # return an argument list: ['-t', '--test', '--info']
        for arg in self._get_optional_actions():
            argument_list.extend(arg.option_strings)
        return argument_list


def print_epilog_text():
    """Pagination print EPILOG_TEXT.

    Returns:
        STDOUT from pydoc.pager().
    """
    epilog_text = EPILOG_TEMPLATE.format(ALL_ABI=ALL_ABI,
                                         BUILD=BUILD,
                                         CLEAR_CACHE=CLEAR_CACHE,
                                         COLLECT_TESTS_ONLY=COLLECT_TESTS_ONLY,
                                         DISABLE_TEARDOWN=DISABLE_TEARDOWN,
                                         DRY_RUN=DRY_RUN,
                                         ENABLE_FILE_PATTERNS=ENABLE_FILE_PATTERNS,
                                         HELP_DESC=HELP_DESC,
                                         HISTORY=HISTORY,
                                         HOST=HOST,
                                         INCLUDE_SUBDIRS=INCLUDE_SUBDIRS,
                                         INFO=INFO,
                                         INSTALL=INSTALL,
                                         INSTANT=INSTANT,
                                         ITERATION=ITERATION,
                                         LATEST_RESULT=LATEST_RESULT,
                                         LIST_MODULES=LIST_MODULES,
                                         NO_METRICS=NO_METRICS,
                                         REBUILD_MODULE_INFO=REBUILD_MODULE_INFO,
                                         RERUN_UNTIL_FAILURE=RERUN_UNTIL_FAILURE,
                                         RETRY_ANY_FAILURE=RETRY_ANY_FAILURE,
                                         SERIAL=SERIAL,
                                         SHARDING=SHARDING,
                                         TEST=TEST,
                                         TEST_MAPPING=TEST_MAPPING,
                                         TF_DEBUG=TF_DEBUG,
                                         TF_TEMPLATE=TF_TEMPLATE,
                                         USER_TYPE=USER_TYPE,
                                         UPDATE_CMD_MAPPING=UPDATE_CMD_MAPPING,
                                         VERBOSE=VERBOSE,
                                         VERSION=VERSION,
                                         VERIFY_CMD_MAPPING=VERIFY_CMD_MAPPING,
                                         WAIT_FOR_DEBUGGER=WAIT_FOR_DEBUGGER)
    return pydoc.pager(epilog_text)


EPILOG_TEMPLATE = r'''ATEST(1)                       ASuite/ATest

NAME
        atest - {HELP_DESC}


SYNOPSIS
        atest [OPTION]... [TEST_TARGET]... -- [CUSTOM_ARGS]...


OPTIONS
        Below arguments are catagorised by features and purposes. Arguments marked with default will apply even the user does not pass it explicitly.

        [ Testing ]
        -a, --all-abi
            {ALL_ABI}

        -b, --build:
            {BUILD} (default)

        -d, --disable-teardown
            {DISABLE_TEARDOWN}

        -D --tf-debug
            {TF_DEBUG}

        --history
            {HISTORY}

        --host
            {HOST}

        -i, --install
            {INSTALL} (default)

        -m, --rebuild-module-info
            {REBUILD_MODULE_INFO} (default)

        -s, --serial
            {SERIAL}

        --sharding
          {SHARDING}

        -t, --test
            {TEST} (default)

        --tf-template
            {TF_TEMPLATE}

        -w, --wait-for-debugger
            {WAIT_FOR_DEBUGGER}


        [ Test Mapping ]
        -p, --test-mapping
            {TEST_MAPPING}

        --include-subdirs
            {INCLUDE_SUBDIRS}

        --enable-file-patterns
            {ENABLE_FILE_PATTERNS}


        [ Information/Queries ]
        --collect-tests-only
            {COLLECT_TESTS_ONLY}

        --info
            {INFO}

        -L, --list-modules
            {LIST_MODULES}

        --latest-result
            {LATEST_RESULT}

        -v, --verbose
            {VERBOSE}

        -V, --version
            {VERSION}


        [ Dry-Run and Caching ]
        --dry-run
            {DRY_RUN}

        -c, --clear-cache
            {CLEAR_CACHE}

        -u, --update-cmd-mapping
            {UPDATE_CMD_MAPPING}

        -y, --verify-cmd-mapping
            {VERIFY_CMD_MAPPING}


        [ Module Parameterization ]
        --instant
            {INSTANT}

        --user-type
            {USER_TYPE}


        [ Iteration Testing ]
        --iterations
            {ITERATION}

        --rerun-until-failure
            {RERUN_UNTIL_FAILURE}

        --retry-any-failure
            {RETRY_ANY_FAILURE}

        [ Metrics ]
        --no-metrics
            {NO_METRICS}


EXAMPLES
    - - - - - - - - -
    IDENTIFYING TESTS
    - - - - - - - - -

    The positional argument <tests> should be a reference to one or more of the tests you'd like to run. Multiple tests can be run in one command by separating test references with spaces.

    Usage template: atest <reference_to_test_1> <reference_to_test_2>

    A <reference_to_test> can be satisfied by the test's MODULE NAME, MODULE:CLASS, CLASS NAME, TF INTEGRATION TEST, FILE PATH or PACKAGE NAME. Explanations and examples of each follow.


    < MODULE NAME >

        Identifying a test by its module name will run the entire module. Input the name as it appears in the LOCAL_MODULE or LOCAL_PACKAGE_NAME variables in that test's Android.mk or Android.bp file.

        Note: Use < TF INTEGRATION TEST > to run non-module tests integrated directly into TradeFed.

        Examples:
            atest FrameworksServicesTests
            atest CtsJankDeviceTestCases


    < MODULE:CLASS >

        Identifying a test by its class name will run just the tests in that class and not the whole module. MODULE:CLASS is the preferred way to run a single class. MODULE is the same as described above. CLASS is the name of the test class in the .java file. It can either be the fully qualified class name or just the basic name.

        Examples:
            atest FrameworksServicesTests:ScreenDecorWindowTests
            atest FrameworksServicesTests:com.android.server.wm.ScreenDecorWindowTests
            atest CtsJankDeviceTestCases:CtsDeviceJankUi


    < CLASS NAME >

        A single class can also be run by referencing the class name without the module name.

        Examples:
            atest ScreenDecorWindowTests
            atest CtsDeviceJankUi

        However, this will take more time than the equivalent MODULE:CLASS reference, so we suggest using a MODULE:CLASS reference whenever possible. Examples below are ordered by performance from the fastest to the slowest:

        Examples:
            atest FrameworksServicesTests:com.android.server.wm.ScreenDecorWindowTests
            atest FrameworksServicesTests:ScreenDecorWindowTests
            atest ScreenDecorWindowTests

    < TF INTEGRATION TEST >

        To run tests that are integrated directly into TradeFed (non-modules), input the name as it appears in the output of the "tradefed.sh list configs" cmd.

        Examples:
           atest example/reboot
           atest native-benchmark


    < FILE PATH >

        Both module-based tests and integration-based tests can be run by inputting the path to their test file or dir as appropriate. A single class can also be run by inputting the path to the class's java file.

        Both relative and absolute paths are supported.

        Example - 2 ways to run the `CtsJankDeviceTestCases` module via path:
        1. run module from android <repo root>:
            atest cts/tests/jank/jank

        2. from <android root>/cts/tests/jank:
            atest .

        Example - run a specific class within CtsJankDeviceTestCases module from <android repo> root via path:
           atest cts/tests/jank/src/android/jank/cts/ui/CtsDeviceJankUi.java

        Example - run an integration test from <android repo> root via path:
           atest tools/tradefederation/contrib/res/config/example/reboot.xml


    < PACKAGE NAME >

        Atest supports searching tests from package name as well.

        Examples:
           atest com.android.server.wm
           atest android.jank.cts


    - - - - - - - - - - - - - - - - - - - - - - - - - -
    SPECIFYING INDIVIDUAL STEPS: BUILD, INSTALL OR RUN
    - - - - - - - - - - - - - - - - - - - - - - - - - -

    The -b, -i and -t options allow you to specify which steps you want to run. If none of those options are given, then all steps are run. If any of these options are provided then only the listed steps are run.

    Note: -i alone is not currently support and can only be included with -t.
    Both -b and -t can be run alone.

    Examples:
        atest -b <test>    (just build targets)
        atest -t <test>    (run tests only)
        atest -it <test>   (install apk and run tests)
        atest -bt <test>   (build targets, run tests, but skip installing apk)


    Atest now has the ability to force a test to skip its cleanup/teardown step. Many tests, e.g. CTS, cleanup the device after the test is run, so trying to rerun your test with -t will fail without having the --disable-teardown parameter. Use -d before -t to skip the test clean up step and test iteratively.

        atest -d <test>    (disable installing apk and cleanning up device)
        atest -t <test>

    Note that -t disables both setup/install and teardown/cleanup of the device. So you can continue to rerun your test with just

        atest -t <test>

    as many times as you want.


    - - - - - - - - - - - - -
    RUNNING SPECIFIC METHODS
    - - - - - - - - - - - - -

    It is possible to run only specific methods within a test class. To run only specific methods, identify the class in any of the ways supported for identifying a class (MODULE:CLASS, FILE PATH, etc) and then append the name of the method or method using the following template:

      <reference_to_class>#<method1>

    Multiple methods can be specified with commas:

      <reference_to_class>#<method1>,<method2>,<method3>...

    Examples:
      atest com.android.server.wm.ScreenDecorWindowTests#testMultipleDecors

      atest FrameworksServicesTests:ScreenDecorWindowTests#testFlagChange,testRemoval


    - - - - - - - - - - - - -
    RUNNING MULTIPLE CLASSES
    - - - - - - - - - - - - -

    To run multiple classes, deliminate them with spaces just like you would when running multiple tests.  Atest will handle building and running classes in the most efficient way possible, so specifying a subset of classes in a module will improve performance over running the whole module.


    Examples:
    - two classes in same module:
      atest FrameworksServicesTests:ScreenDecorWindowTests FrameworksServicesTests:DimmerTests

    - two classes, different modules:
      atest FrameworksServicesTests:ScreenDecorWindowTests CtsJankDeviceTestCases:CtsDeviceJankUi


    - - - - - - - - - - -
    RUNNING NATIVE TESTS
    - - - - - - - - - - -

    Atest can run native test.

    Example:
    - Input tests:
      atest -a libinput_tests inputflinger_tests

    Use -a|--all-abi to run the tests for all available device architectures, which in this example is armeabi-v7a (ARM 32-bit) and arm64-v8a (ARM 64-bit).

    To select a specific native test to run, use colon (:) to specify the test name and hashtag (#) to further specify an individual method. For example, for the following test definition:

        TEST_F(InputDispatcherTest, InjectInputEvent_ValidatesKeyEvents)

    You can run the entire test using:

        atest inputflinger_tests:InputDispatcherTest

    or an individual test method using:

        atest inputflinger_tests:InputDispatcherTest#InjectInputEvent_ValidatesKeyEvents


    - - - - - - - - - - - - - -
    RUNNING TESTS IN ITERATION
    - - - - - - - - - - - - - -

    To run tests in iterations, simply pass --iterations argument. No matter pass or fail, atest won't stop testing until the max iteration is reached.

    Example:
        atest <test> --iterations    # 10 iterations(by default).
        atest <test> --iterations 5  # run <test> 5 times.

    Two approaches that assist users to detect flaky tests:

    1) Run all tests until a failure occurs or the max iteration is reached.

    Example:
        - 10 iterations(by default).
        atest <test> --rerun-until-failure
        - stop when failed or reached the 20th run.
        atest <test> --rerun-until-failure 20

    2) Run failed tests until passed or the max iteration is reached.

    Example:
        - 10 iterations(by default).
        atest <test> --retry-any-failure
        - stop when passed or reached the 20th run.
        atest <test> --retry-any-failure 20


    - - - - - - - - - - - - - - - -
    REGRESSION DETECTION (obsolute)
    - - - - - - - - - - - - - - - -

    ********************** Warning **********************
    Please STOP using arguments below -- they are obsolete and will be removed in a near future:
        --detect-regression
        --generate-baseline
        --generate-new-metrics

    Please check RUNNING TESTS IN ITERATION out for alternatives.
    ******************************************************

    Generate pre-patch or post-patch metrics without running regression detection:

    Example:
        atest <test> --generate-baseline <optional iter>
        atest <test> --generate-new-metrics <optional iter>

    Local regression detection can be run in three options:

    1) Provide a folder containing baseline (pre-patch) metrics (generated previously). Atest will run the tests n (default 5) iterations, generate a new set of post-patch metrics, and compare those against existing metrics.

    Example:
        atest <test> --detect-regression </path/to/baseline> --generate-new-metrics <optional iter>

    2) Provide a folder containing post-patch metrics (generated previously). Atest will run the tests n (default 5) iterations, generate a new set of pre-patch metrics, and compare those against those provided. Note: the developer needs to revert the device/tests to pre-patch state to generate baseline metrics.

    Example:
        atest <test> --detect-regression </path/to/new> --generate-baseline <optional iter>

    3) Provide 2 folders containing both pre-patch and post-patch metrics. Atest will run no tests but the regression detection algorithm.

    Example:
        atest --detect-regression </path/to/baseline> </path/to/new>


    - - - - - - - - - - - -
    TESTS IN TEST MAPPING
    - - - - - - - - - - - -

    Atest can run tests in TEST_MAPPING files:

    1) Run presubmit tests in TEST_MAPPING files in current and parent
       directories. You can also specify a target directory.

    Example:
        atest  (run presubmit tests in TEST_MAPPING files in current and parent directories)
        atest --test-mapping </path/to/project>
               (run presubmit tests in TEST_MAPPING files in </path/to/project> and its parent directories)

    2) Run a specified test group in TEST_MAPPING files.

    Example:
        atest :postsubmit
              (run postsubmit tests in TEST_MAPPING files in current and parent directories)
        atest :all
              (Run tests from all groups in TEST_MAPPING files)
        atest --test-mapping </path/to/project>:postsubmit
              (run postsubmit tests in TEST_MAPPING files in </path/to/project> and its parent directories)

    3) Run tests in TEST_MAPPING files including sub directories

    By default, atest will only search for tests in TEST_MAPPING files in current (or given directory) and its parent directories. If you want to run tests in TEST_MAPPING files in the sub-directories, you can use option --include-subdirs to force atest to include those tests too.

    Example:
        atest --include-subdirs [optional </path/to/project>:<test_group_name>]
              (run presubmit tests in TEST_MAPPING files in current, sub and parent directories)
    A path can be provided optionally if you want to search for tests in a given directory, with optional test group name. By default, the test group is presubmit.


    - - - - - - - - - - - - - -
    ADDITIONAL ARGS TO TRADEFED
    - - - - - - - - - - - - - -

    When trying to pass custom arguments for the test runners, everything after '--'
    will be consumed as custom args.

    Example:
        atest -v <test> -- <custom_args1> <custom_args2>


                                                     2019-12-19
'''
