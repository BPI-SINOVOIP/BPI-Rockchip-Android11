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
import itertools

from future import standard_library

standard_library.install_aliases()

import importlib
import inspect
import fnmatch
import json
import logging
import os
import pkgutil
import sys

from acts import base_test
from acts import keys
from acts import logger
from acts import records
from acts import signals
from acts import utils
from acts import error

from mobly.records import ExceptionRecord


def _find_test_class():
    """Finds the test class in a test script.

    Walk through module members and find the subclass of BaseTestClass. Only
    one subclass is allowed in a test script.

    Returns:
        The test class in the test module.
    """
    test_classes = []
    main_module_members = sys.modules['__main__']
    for _, module_member in main_module_members.__dict__.items():
        if inspect.isclass(module_member):
            if issubclass(module_member, base_test.BaseTestClass):
                test_classes.append(module_member)
    if len(test_classes) != 1:
        logging.error('Expected 1 test class per file, found %s.',
                      [t.__name__ for t in test_classes])
        sys.exit(1)
    return test_classes[0]


def execute_one_test_class(test_class, test_config, test_identifier):
    """Executes one specific test class.

    You could call this function in your own cli test entry point if you choose
    not to use act.py.

    Args:
        test_class: A subclass of acts.base_test.BaseTestClass that has the test
                    logic to be executed.
        test_config: A dict representing one set of configs for a test run.
        test_identifier: A list of tuples specifying which test cases to run in
                         the test class.

    Returns:
        True if all tests passed without any error, False otherwise.

    Raises:
        If signals.TestAbortAll is raised by a test run, pipe it through.
    """
    tr = TestRunner(test_config, test_identifier)
    try:
        tr.run(test_class)
        return tr.results.is_all_pass
    except signals.TestAbortAll:
        raise
    except:
        logging.exception('Exception when executing %s.', tr.testbed_name)
    finally:
        tr.stop()


class TestRunner(object):
    """The class that instantiates test classes, executes test cases, and
    report results.

    Attributes:
        test_run_config: The TestRunConfig object specifying what tests to run.
        id: A string that is the unique identifier of this test run.
        log: The logger object used throughout this test run.
        test_classes: A dictionary where we can look up the test classes by name
            to instantiate. Supports unix shell style wildcards.
        run_list: A list of tuples specifying what tests to run.
        results: The test result object used to record the results of this test
            run.
        running: A boolean signifies whether this test run is ongoing or not.
    """
    def __init__(self, test_configs, run_list):
        self.test_run_config = test_configs
        self.testbed_name = self.test_run_config.testbed_name
        start_time = logger.get_log_file_timestamp()
        self.id = '{}@{}'.format(self.testbed_name, start_time)
        self.test_run_config.log_path = os.path.abspath(
            os.path.join(self.test_run_config.log_path, self.testbed_name,
                         start_time))
        logger.setup_test_logger(self.log_path, self.testbed_name)
        self.log = logging.getLogger()
        self.test_run_config.summary_writer = records.TestSummaryWriter(
            os.path.join(self.log_path, records.OUTPUT_FILE_SUMMARY))
        self.run_list = run_list
        self.dump_config()
        self.results = records.TestResult()
        self.running = False

    @property
    def log_path(self):
        """The path to write logs of this test run to."""
        return self.test_run_config.log_path

    @property
    def summary_writer(self):
        """The object responsible for writing summary and results data."""
        return self.test_run_config.summary_writer

    def import_test_modules(self, test_paths):
        """Imports test classes from test scripts.

        1. Locate all .py files under test paths.
        2. Import the .py files as modules.
        3. Find the module members that are test classes.
        4. Categorize the test classes by name.

        Args:
            test_paths: A list of directory paths where the test files reside.

        Returns:
            A dictionary where keys are test class name strings, values are
            actual test classes that can be instantiated.
        """
        def is_testfile_name(name, ext):
            if ext == '.py':
                if name.endswith('Test') or name.endswith('_test'):
                    return True
            return False

        file_list = utils.find_files(test_paths, is_testfile_name)
        test_classes = {}
        for path, name, _ in file_list:
            sys.path.append(path)
            try:
                with utils.SuppressLogOutput(
                        log_levels=[logging.INFO, logging.ERROR]):
                    module = importlib.import_module(name)
            except:
                for test_cls_name, _ in self.run_list:
                    alt_name = name.replace('_', '').lower()
                    alt_cls_name = test_cls_name.lower()
                    # Only block if a test class on the run list causes an
                    # import error. We need to check against both naming
                    # conventions: AaaBbb and aaa_bbb.
                    if name == test_cls_name or alt_name == alt_cls_name:
                        msg = ('Encountered error importing test class %s, '
                               'abort.') % test_cls_name
                        # This exception is logged here to help with debugging
                        # under py2, because "raise X from Y" syntax is only
                        # supported under py3.
                        self.log.exception(msg)
                        raise ValueError(msg)
                continue
            for member_name in dir(module):
                if not member_name.startswith('__'):
                    if member_name.endswith('Test'):
                        test_class = getattr(module, member_name)
                        if inspect.isclass(test_class):
                            test_classes[member_name] = test_class
        return test_classes

    def set_test_util_logs(self, module=None):
        """Sets the log object to each test util module.

        This recursively include all modules under acts.test_utils and sets the
        main test logger to each module.

        Args:
            module: A module under acts.test_utils.
        """
        # Initial condition of recursion.
        if not module:
            module = importlib.import_module('acts.test_utils')
        # Somehow pkgutil.walk_packages is not working for me.
        # Using iter_modules for now.
        pkg_iter = pkgutil.iter_modules(module.__path__, module.__name__ + '.')
        for _, module_name, ispkg in pkg_iter:
            m = importlib.import_module(module_name)
            if ispkg:
                self.set_test_util_logs(module=m)
            else:
                self.log.debug('Setting logger to test util module %s',
                               module_name)
                setattr(m, 'log', self.log)

    def run_test_class(self, test_cls_name, test_cases=None):
        """Instantiates and executes a test class.

        If test_cases is None, the test cases listed by self.tests will be
        executed instead. If self.tests is empty as well, no test case in this
        test class will be executed.

        Args:
            test_cls_name: Name of the test class to execute.
            test_cases: List of test case names to execute within the class.

        Raises:
            ValueError is raised if the requested test class could not be found
            in the test_paths directories.
        """
        matches = fnmatch.filter(self.test_classes.keys(), test_cls_name)
        if not matches:
            self.log.info(
                'Cannot find test class %s or classes matching pattern, '
                'skipping for now.' % test_cls_name)
            record = records.TestResultRecord('*all*', test_cls_name)
            record.test_skip(signals.TestSkip('Test class does not exist.'))
            self.results.add_record(record)
            return
        if matches != [test_cls_name]:
            self.log.info('Found classes matching pattern %s: %s',
                          test_cls_name, matches)

        for test_cls_name_match in matches:
            test_cls = self.test_classes[test_cls_name_match]
            test_cls_instance = test_cls(self.test_run_config)
            try:
                cls_result = test_cls_instance.run(test_cases)
                self.results += cls_result
            except signals.TestAbortAll as e:
                self.results += e.results
                raise e

    def run(self, test_class=None):
        """Executes test cases.

        This will instantiate controller and test classes, and execute test
        classes. This can be called multiple times to repeatedly execute the
        requested test cases.

        A call to TestRunner.stop should eventually happen to conclude the life
        cycle of a TestRunner.

        Args:
            test_class: The python module of a test class. If provided, run this
                        class; otherwise, import modules in under test_paths
                        based on run_list.
        """
        if not self.running:
            self.running = True

        if test_class:
            self.test_classes = {test_class.__name__: test_class}
        else:
            t_paths = self.test_run_config.controller_configs[
                keys.Config.key_test_paths.value]
            self.test_classes = self.import_test_modules(t_paths)
        self.log.debug('Executing run list %s.', self.run_list)
        for test_cls_name, test_case_names in self.run_list:
            if not self.running:
                break

            if test_case_names:
                self.log.debug('Executing test cases %s in test class %s.',
                               test_case_names, test_cls_name)
            else:
                self.log.debug('Executing test class %s', test_cls_name)

            try:
                self.run_test_class(test_cls_name, test_case_names)
            except error.ActsError as e:
                self.results.error.append(ExceptionRecord(e))
                self.log.error('Test Runner Error: %s' % e.message)
            except signals.TestAbortAll as e:
                self.log.warning(
                    'Abort all subsequent test classes. Reason: %s', e)
                raise

    def stop(self):
        """Releases resources from test run. Should always be called after
        TestRunner.run finishes.

        This function concludes a test run and writes out a test report.
        """
        if self.running:
            msg = '\nSummary for test run %s: %s\n' % (
                self.id, self.results.summary_str())
            self._write_results_to_file()
            self.log.info(msg.strip())
            logger.kill_test_logger(self.log)
            self.running = False

    def _write_results_to_file(self):
        """Writes test results to file(s) in a serializable format."""
        # Old JSON format
        path = os.path.join(self.log_path, 'test_run_summary.json')
        with open(path, 'w') as f:
            f.write(self.results.json_str())
        # New YAML format
        self.summary_writer.dump(self.results.summary_dict(),
                                 records.TestSummaryEntryType.SUMMARY)

    def dump_config(self):
        """Writes the test config to a JSON file under self.log_path"""
        config_path = os.path.join(self.log_path, 'test_configs.json')
        with open(config_path, 'a') as f:
            json.dump(dict(
                itertools.chain(
                    self.test_run_config.user_params.items(),
                    self.test_run_config.controller_configs.items())),
                      f,
                      skipkeys=True,
                      indent=4)

    def write_test_campaign(self):
        """Log test campaign file."""
        path = os.path.join(self.log_path, 'test_campaign.log')
        with open(path, 'w') as f:
            for test_class, test_cases in self.run_list:
                f.write('%s:\n%s' % (test_class, ',\n'.join(test_cases)))
                f.write('\n\n')
