#!/usr/bin/env python3
#
#   Copyright 2016 - The Android Open Source Project
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

import argparse
import os
import re
import signal
import sys
import traceback
from builtins import str

from mobly import config_parser as mobly_config_parser

from acts import config_parser
from acts import keys
from acts import signals
from acts import test_runner
from acts import utils
from acts.config_parser import ActsConfigError


def _run_test(parsed_config, test_identifiers, repeat=1):
    """Instantiate and runs test_runner.TestRunner.

    This is the function to start separate processes with.

    Args:
        parsed_config: A mobly.config_parser.TestRunConfig that is a set of
                       configs for one test_runner.TestRunner.
        test_identifiers: A list of tuples, each identifies what test case to
                          run on what test class.
        repeat: Number of times to iterate the specified tests.

    Returns:
        True if all tests passed without any error, False otherwise.
    """
    runner = _create_test_runner(parsed_config, test_identifiers)
    try:
        for i in range(repeat):
            runner.run()
        return runner.results.is_all_pass
    except signals.TestAbortAll:
        return True
    except:
        print("Exception when executing %s, iteration %s." %
              (runner.testbed_name, i))
        print(traceback.format_exc())
    finally:
        runner.stop()


def _create_test_runner(parsed_config, test_identifiers):
    """Instantiates one test_runner.TestRunner object and register termination
    signal handlers that properly shut down the test_runner.TestRunner run.

    Args:
        parsed_config: A mobly.config_parser.TestRunConfig that is a set of
                       configs for one test_runner.TestRunner.
        test_identifiers: A list of tuples, each identifies what test case to
                          run on what test class.

    Returns:
        A test_runner.TestRunner object.
    """
    try:
        t = test_runner.TestRunner(parsed_config, test_identifiers)
    except:
        print("Failed to instantiate test runner, abort.")
        print(traceback.format_exc())
        sys.exit(1)
    # Register handler for termination signals.
    handler = config_parser.gen_term_signal_handler([t])
    signal.signal(signal.SIGTERM, handler)
    signal.signal(signal.SIGINT, handler)
    return t


def _run_tests(parsed_configs, test_identifiers, repeat):
    """Executes requested tests sequentially.

    Requested test runs will commence one after another according to the order
    of their corresponding configs.

    Args:
        parsed_configs: A list of mobly.config_parser.TestRunConfig, each is a
                        set of configs for one test_runner.TestRunner.
        test_identifiers: A list of tuples, each identifies what test case to
                          run on what test class.
        repeat: Number of times to iterate the specified tests.

    Returns:
        True if all test runs executed successfully, False otherwise.
    """
    ok = True
    for c in parsed_configs:
        try:
            ret = _run_test(c, test_identifiers, repeat)
            ok = ok and ret
        except Exception as e:
            print("Exception occurred when executing test bed %s. %s" %
                  (c.testbed_name, e))
    return ok


def main(argv):
    """This is a sample implementation of a cli entry point for ACTS test
    execution.

    Or you could implement your own cli entry point using acts.config_parser
    functions and acts.test_runner.execute_one_test_class.
    """
    parser = argparse.ArgumentParser(
        description=("Specify tests to run. If nothing specified, "
                     "run all test cases found."))
    parser.add_argument('-c',
                        '--config',
                        type=str,
                        required=True,
                        metavar="<PATH>",
                        help="Path to the test configuration file.")
    parser.add_argument(
        '-ci',
        '--campaign_iterations',
        metavar="<CAMPAIGN_ITERATIONS>",
        nargs='?',
        type=int,
        const=1,
        default=1,
        help="Number of times to run the campaign or a group of test cases.")
    parser.add_argument('-tb',
                        '--testbed',
                        nargs='+',
                        type=str,
                        metavar="[<TEST BED NAME1> <TEST BED NAME2> ...]",
                        help="Specify which test beds to run tests on.")
    parser.add_argument('-lp',
                        '--logpath',
                        type=str,
                        metavar="<PATH>",
                        help="Root path under which all logs will be placed.")
    parser.add_argument(
        '-tp',
        '--testpaths',
        nargs='*',
        type=str,
        metavar="<PATH> <PATH>",
        help="One or more non-recursive test class search paths.")

    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('-tc',
                       '--testclass',
                       nargs='+',
                       type=str,
                       metavar="[TestClass1 TestClass2:test_xxx ...]",
                       help="A list of test classes/cases to run.")
    group.add_argument(
        '-tf',
        '--testfile',
        nargs=1,
        type=str,
        metavar="<PATH>",
        help=("Path to a file containing a comma delimited list of test "
              "classes to run."))
    parser.add_argument('-ti',
                        '--test_case_iterations',
                        metavar="<TEST_CASE_ITERATIONS>",
                        nargs='?',
                        type=int,
                        help="Number of times to run every test case.")

    args = parser.parse_args(argv)
    test_list = None
    if args.testfile:
        test_list = config_parser.parse_test_file(args.testfile[0])
    elif args.testclass:
        test_list = args.testclass
    if re.search(r'\.ya?ml$', args.config):
        parsed_configs = mobly_config_parser.load_test_config_file(
            args.config, args.testbed)
    else:
        parsed_configs = config_parser.load_test_config_file(
            args.config, args.testbed)

    for test_run_config in parsed_configs:
        if args.testpaths:
            tp_key = keys.Config.key_test_paths.value
            test_run_config.controller_configs[tp_key] = args.testpaths
        if args.logpath:
            test_run_config.log_path = args.logpath
        if args.test_case_iterations:
            ti_key = keys.Config.key_test_case_iterations.value
            test_run_config.user_params[ti_key] = args.test_case_iterations

        # Sets the --testpaths flag to the default test directory if left unset.
        testpath_key = keys.Config.key_test_paths.value
        if (testpath_key not in test_run_config.controller_configs
                or test_run_config.controller_configs[testpath_key] is None):
            test_run_config.controller_configs[testpath_key] = utils.abs_path(
                utils.os.path.join(os.path.dirname(__file__),
                                   '../../../tests/'))

        # TODO(markdr): Find a way to merge this with the validation done in
        # Mobly's load_test_config_file.
        if not test_run_config.log_path:
            raise ActsConfigError("Required key %s missing in test config." %
                                  keys.Config.key_log_path.value)
        test_run_config.log_path = utils.abs_path(test_run_config.log_path)

    # Prepare args for test runs
    test_identifiers = config_parser.parse_test_list(test_list)

    exec_result = _run_tests(parsed_configs, test_identifiers,
                             args.campaign_iterations)
    if exec_result is False:
        # return 1 upon test failure.
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    main(sys.argv[1:])
