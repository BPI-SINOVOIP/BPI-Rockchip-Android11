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

"""Command Line Translator for atest."""

# pylint: disable=line-too-long
# pylint: disable=too-many-lines

from __future__ import print_function

import fnmatch
import json
import logging
import os
import re
import sys
import time

import atest_error
import atest_utils
import constants
import test_finder_handler
import test_mapping

from metrics import metrics
from metrics import metrics_utils
from test_finders import module_finder

FUZZY_FINDER = 'FUZZY'
CACHE_FINDER = 'CACHE'

# Pattern used to identify comments start with '//' or '#' in TEST_MAPPING.
_COMMENTS_RE = re.compile(r'(?m)[\s\t]*(#|//).*|(\".*?\")')
_COMMENTS = frozenset(['//', '#'])

#pylint: disable=no-self-use
class CLITranslator:
    """
    CLITranslator class contains public method translate() and some private
    helper methods. The atest tool can call the translate() method with a list
    of strings, each string referencing a test to run. Translate() will
    "translate" this list of test strings into a list of build targets and a
    list of TradeFederation run commands.

    Translation steps for a test string reference:
        1. Narrow down the type of reference the test string could be, i.e.
           whether it could be referencing a Module, Class, Package, etc.
        2. Try to find the test files assuming the test string is one of these
           types of reference.
        3. If test files found, generate Build Targets and the Run Command.
    """

    def __init__(self, module_info=None, print_cache_msg=True):
        """CLITranslator constructor

        Args:
            module_info: ModuleInfo class that has cached module-info.json.
            print_cache_msg: Boolean whether printing clear cache message or not.
                             True will print message while False won't print.
        """
        self.mod_info = module_info
        self.enable_file_patterns = False
        self.msg = ''
        if print_cache_msg:
            self.msg = ('(Test info has been cached for speeding up the next '
                        'run, if test info need to be updated, please add -c '
                        'to clean the old cache.)')

    # pylint: disable=too-many-locals
    def _find_test_infos(self, test, tm_test_detail):
        """Return set of TestInfos based on a given test.

        Args:
            test: A string representing test references.
            tm_test_detail: The TestDetail of test configured in TEST_MAPPING
                files.

        Returns:
            Set of TestInfos based on the given test.
        """
        test_infos = set()
        test_find_starts = time.time()
        test_found = False
        test_finders = []
        test_info_str = ''
        find_test_err_msg = None
        for finder in test_finder_handler.get_find_methods_for_test(
                self.mod_info, test):
            # For tests in TEST_MAPPING, find method is only related to
            # test name, so the details can be set after test_info object
            # is created.
            try:
                found_test_infos = finder.find_method(
                    finder.test_finder_instance, test)
            except atest_error.TestDiscoveryException as e:
                find_test_err_msg = e
            if found_test_infos:
                finder_info = finder.finder_info
                for test_info in found_test_infos:
                    if tm_test_detail:
                        test_info.data[constants.TI_MODULE_ARG] = (
                            tm_test_detail.options)
                        test_info.from_test_mapping = True
                        test_info.host = tm_test_detail.host
                    if finder_info != CACHE_FINDER:
                        test_info.test_finder = finder_info
                    test_infos.add(test_info)
                test_found = True
                print("Found '%s' as %s" % (
                    atest_utils.colorize(test, constants.GREEN),
                    finder_info))
                if finder_info == CACHE_FINDER and test_infos:
                    test_finders.append(list(test_infos)[0].test_finder)
                test_finders.append(finder_info)
                test_info_str = ','.join([str(x) for x in found_test_infos])
                break
        if not test_found:
            f_results = self._fuzzy_search_and_msg(test, find_test_err_msg)
            if f_results:
                test_infos.update(f_results)
                test_found = True
                test_finders.append(FUZZY_FINDER)
        metrics.FindTestFinishEvent(
            duration=metrics_utils.convert_duration(
                time.time() - test_find_starts),
            success=test_found,
            test_reference=test,
            test_finders=test_finders,
            test_info=test_info_str)
        # Cache test_infos by default except running with TEST_MAPPING which may
        # include customized flags and they are likely to mess up other
        # non-test_mapping tests.
        if test_infos and not tm_test_detail:
            atest_utils.update_test_info_cache(test, test_infos)
            print(self.msg)
        return test_infos

    def _fuzzy_search_and_msg(self, test, find_test_err_msg):
        """ Fuzzy search and print message.

        Args:
            test: A string representing test references
            find_test_err_msg: A string of find test error message.

        Returns:
            A list of TestInfos if found, otherwise None.
        """
        print('No test found for: %s' %
              atest_utils.colorize(test, constants.RED))
        # Currently we focus on guessing module names. Append names on
        # results if more finders support fuzzy searching.
        mod_finder = module_finder.ModuleFinder(self.mod_info)
        results = mod_finder.get_fuzzy_searching_results(test)
        if len(results) == 1 and self._confirm_running(results):
            found_test_infos = mod_finder.find_test_by_module_name(results[0])
            # found_test_infos is a list with at most 1 element.
            if found_test_infos:
                return found_test_infos
        elif len(results) > 1:
            self._print_fuzzy_searching_results(results)
        else:
            print('No matching result for {0}.'.format(test))
        if find_test_err_msg:
            print('%s\n' % (atest_utils.colorize(
                find_test_err_msg, constants.MAGENTA)))
        else:
            print('(This can happen after a repo sync or if the test'
                  ' is new. Running: with "%s" may resolve the issue.)'
                  '\n' % (atest_utils.colorize(
                      constants.REBUILD_MODULE_INFO_FLAG,
                      constants.RED)))
        return None

    def _get_test_infos(self, tests, test_mapping_test_details=None):
        """Return set of TestInfos based on passed in tests.

        Args:
            tests: List of strings representing test references.
            test_mapping_test_details: List of TestDetail for tests configured
                in TEST_MAPPING files.

        Returns:
            Set of TestInfos based on the passed in tests.
        """
        test_infos = set()
        if not test_mapping_test_details:
            test_mapping_test_details = [None] * len(tests)
        for test, tm_test_detail in zip(tests, test_mapping_test_details):
            found_test_infos = self._find_test_infos(test, tm_test_detail)
            test_infos.update(found_test_infos)
        return test_infos

    def _confirm_running(self, results):
        """Listen to an answer from raw input.

        Args:
            results: A list of results.

        Returns:
            True is the answer is affirmative.
        """
        decision = input('Did you mean {0}? [Y/n] '.format(
            atest_utils.colorize(results[0], constants.GREEN)))
        return decision in constants.AFFIRMATIVES

    def _print_fuzzy_searching_results(self, results):
        """Print modules when fuzzy searching gives multiple results.

        If the result is lengthy, just print the first 10 items only since we
        have already given enough-accurate result.

        Args:
            results: A list of guessed testable module names.

        """
        atest_utils.colorful_print('Did you mean the following modules?',
                                   constants.WHITE)
        for mod in results[:10]:
            atest_utils.colorful_print(mod, constants.GREEN)

    def filter_comments(self, test_mapping_file):
        """Remove comments in TEST_MAPPING file to valid format. Only '//' and
        '#' are regarded as comments.

        Args:
            test_mapping_file: Path to a TEST_MAPPING file.

        Returns:
            Valid json string without comments.
        """
        def _replace(match):
            """Replace comments if found matching the defined regular
            expression.

            Args:
                match: The matched regex pattern

            Returns:
                "" if it matches _COMMENTS, otherwise original string.
            """
            line = match.group(0).strip()
            return "" if any(map(line.startswith, _COMMENTS)) else line
        with open(test_mapping_file) as json_file:
            return re.sub(_COMMENTS_RE, _replace, json_file.read())

    def _read_tests_in_test_mapping(self, test_mapping_file):
        """Read tests from a TEST_MAPPING file.

        Args:
            test_mapping_file: Path to a TEST_MAPPING file.

        Returns:
            A tuple of (all_tests, imports), where
            all_tests is a dictionary of all tests in the TEST_MAPPING file,
                grouped by test group.
            imports is a list of test_mapping.Import to include other test
                mapping files.
        """
        all_tests = {}
        imports = []
        test_mapping_dict = json.loads(self.filter_comments(test_mapping_file))
        for test_group_name, test_list in test_mapping_dict.items():
            if test_group_name == constants.TEST_MAPPING_IMPORTS:
                for import_detail in test_list:
                    imports.append(
                        test_mapping.Import(test_mapping_file, import_detail))
            else:
                grouped_tests = all_tests.setdefault(test_group_name, set())
                tests = []
                for test in test_list:
                    if (self.enable_file_patterns and
                            not test_mapping.is_match_file_patterns(
                                test_mapping_file, test)):
                        continue
                    test_mod_info = self.mod_info.name_to_module_info.get(
                        test['name'])
                    if not test_mod_info:
                        print('WARNING: %s is not a valid build target and '
                              'may not be discoverable by TreeHugger. If you '
                              'want to specify a class or test-package, '
                              'please set \'name\' to the test module and use '
                              '\'options\' to specify the right tests via '
                              '\'include-filter\'.\nNote: this can also occur '
                              'if the test module is not built for your '
                              'current lunch target.\n' %
                              atest_utils.colorize(test['name'], constants.RED))
                    elif not any(x in test_mod_info['compatibility_suites'] for
                                 x in constants.TEST_MAPPING_SUITES):
                        print('WARNING: Please add %s to either suite: %s for '
                              'this TEST_MAPPING file to work with TreeHugger.' %
                              (atest_utils.colorize(test['name'],
                                                    constants.RED),
                               atest_utils.colorize(constants.TEST_MAPPING_SUITES,
                                                    constants.GREEN)))
                    tests.append(test_mapping.TestDetail(test))
                grouped_tests.update(tests)
        return all_tests, imports

    def _find_files(self, path, file_name=constants.TEST_MAPPING):
        """Find all files with given name under the given path.

        Args:
            path: A string of path in source.

        Returns:
            A list of paths of the files with the matching name under the given
            path.
        """
        test_mapping_files = []
        for root, _, filenames in os.walk(path):
            for filename in fnmatch.filter(filenames, file_name):
                test_mapping_files.append(os.path.join(root, filename))
        return test_mapping_files

    def _get_tests_from_test_mapping_files(
            self, test_group, test_mapping_files):
        """Get tests in the given test mapping files with the match group.

        Args:
            test_group: Group of tests to run. Default is set to `presubmit`.
            test_mapping_files: A list of path of TEST_MAPPING files.

        Returns:
            A tuple of (tests, all_tests, imports), where,
            tests is a set of tests (test_mapping.TestDetail) defined in
            TEST_MAPPING file of the given path, and its parent directories,
            with matching test_group.
            all_tests is a dictionary of all tests in TEST_MAPPING files,
            grouped by test group.
            imports is a list of test_mapping.Import objects that contains the
            details of where to import a TEST_MAPPING file.
        """
        all_imports = []
        # Read and merge the tests in all TEST_MAPPING files.
        merged_all_tests = {}
        for test_mapping_file in test_mapping_files:
            all_tests, imports = self._read_tests_in_test_mapping(
                test_mapping_file)
            all_imports.extend(imports)
            for test_group_name, test_list in all_tests.items():
                grouped_tests = merged_all_tests.setdefault(
                    test_group_name, set())
                grouped_tests.update(test_list)

        tests = set(merged_all_tests.get(test_group, []))
        if test_group == constants.TEST_GROUP_ALL:
            for grouped_tests in merged_all_tests.values():
                tests.update(grouped_tests)
        return tests, merged_all_tests, all_imports

    # pylint: disable=too-many-arguments
    # pylint: disable=too-many-locals
    def _find_tests_by_test_mapping(
            self, path='', test_group=constants.TEST_GROUP_PRESUBMIT,
            file_name=constants.TEST_MAPPING, include_subdirs=False,
            checked_files=None):
        """Find tests defined in TEST_MAPPING in the given path.

        Args:
            path: A string of path in source. Default is set to '', i.e., CWD.
            test_group: Group of tests to run. Default is set to `presubmit`.
            file_name: Name of TEST_MAPPING file. Default is set to
                `TEST_MAPPING`. The argument is added for testing purpose.
            include_subdirs: True to include tests in TEST_MAPPING files in sub
                directories.
            checked_files: Paths of TEST_MAPPING files that have been checked.

        Returns:
            A tuple of (tests, all_tests), where,
            tests is a set of tests (test_mapping.TestDetail) defined in
            TEST_MAPPING file of the given path, and its parent directories,
            with matching test_group.
            all_tests is a dictionary of all tests in TEST_MAPPING files,
            grouped by test group.
        """
        path = os.path.realpath(path)
        test_mapping_files = set()
        all_tests = {}
        test_mapping_file = os.path.join(path, file_name)
        if os.path.exists(test_mapping_file):
            test_mapping_files.add(test_mapping_file)
        # Include all TEST_MAPPING files in sub-directories if `include_subdirs`
        # is set to True.
        if include_subdirs:
            test_mapping_files.update(self._find_files(path, file_name))
        # Include all possible TEST_MAPPING files in parent directories.
        root_dir = os.environ.get(constants.ANDROID_BUILD_TOP, os.sep)
        while path not in (root_dir, os.sep):
            path = os.path.dirname(path)
            test_mapping_file = os.path.join(path, file_name)
            if os.path.exists(test_mapping_file):
                test_mapping_files.add(test_mapping_file)

        if checked_files is None:
            checked_files = set()
        test_mapping_files.difference_update(checked_files)
        checked_files.update(test_mapping_files)
        if not test_mapping_files:
            return test_mapping_files, all_tests

        tests, all_tests, imports = self._get_tests_from_test_mapping_files(
            test_group, test_mapping_files)

        # Load TEST_MAPPING files from imports recursively.
        if imports:
            for import_detail in imports:
                path = import_detail.get_path()
                # (b/110166535 #19) Import path might not exist if a project is
                # located in different directory in different branches.
                if path is None:
                    logging.warning(
                        'Failed to import TEST_MAPPING at %s', import_detail)
                    continue
                # Search for tests based on the imported search path.
                import_tests, import_all_tests = (
                    self._find_tests_by_test_mapping(
                        path, test_group, file_name, include_subdirs,
                        checked_files))
                # Merge the collections
                tests.update(import_tests)
                for group, grouped_tests in import_all_tests.items():
                    all_tests.setdefault(group, set()).update(grouped_tests)

        return tests, all_tests

    def _gather_build_targets(self, test_infos):
        targets = set()
        for test_info in test_infos:
            targets |= test_info.build_targets
        return targets

    def _get_test_mapping_tests(self, args):
        """Find the tests in TEST_MAPPING files.

        Args:
            args: arg parsed object.

        Returns:
            A tuple of (test_names, test_details_list), where
            test_names: a list of test name
            test_details_list: a list of test_mapping.TestDetail objects for
                the tests in TEST_MAPPING files with matching test group.
        """
        # Pull out tests from test mapping
        src_path = ''
        test_group = constants.TEST_GROUP_PRESUBMIT
        if args.tests:
            if ':' in args.tests[0]:
                src_path, test_group = args.tests[0].split(':')
            else:
                src_path = args.tests[0]

        test_details, all_test_details = self._find_tests_by_test_mapping(
            path=src_path, test_group=test_group,
            include_subdirs=args.include_subdirs, checked_files=set())
        test_details_list = list(test_details)
        if not test_details_list:
            logging.warning(
                'No tests of group `%s` found in TEST_MAPPING at %s or its '
                'parent directories.\nYou might be missing atest arguments,'
                ' try `atest --help` for more information',
                test_group, os.path.realpath(''))
            if all_test_details:
                tests = ''
                for test_group, test_list in all_test_details.items():
                    tests += '%s:\n' % test_group
                    for test_detail in sorted(test_list):
                        tests += '\t%s\n' % test_detail
                logging.warning(
                    'All available tests in TEST_MAPPING files are:\n%s',
                    tests)
            metrics_utils.send_exit_event(constants.EXIT_CODE_TEST_NOT_FOUND)
            sys.exit(constants.EXIT_CODE_TEST_NOT_FOUND)

        logging.debug(
            'Test details:\n%s',
            '\n'.join([str(detail) for detail in test_details_list]))
        test_names = [detail.name for detail in test_details_list]
        return test_names, test_details_list


    def translate(self, args):
        """Translate atest command line into build targets and run commands.

        Args:
            args: arg parsed object.

        Returns:
            A tuple with set of build_target strings and list of TestInfos.
        """
        tests = args.tests
        # Test details from TEST_MAPPING files
        test_details_list = None
        if atest_utils.is_test_mapping(args):
            if args.enable_file_patterns:
                self.enable_file_patterns = True
            tests, test_details_list = self._get_test_mapping_tests(args)
        atest_utils.colorful_print("\nFinding Tests...", constants.CYAN)
        logging.debug('Finding Tests: %s', tests)
        start = time.time()
        test_infos = self._get_test_infos(tests, test_details_list)
        logging.debug('Found tests in %ss', time.time() - start)
        for test_info in test_infos:
            logging.debug('%s\n', test_info)
        build_targets = self._gather_build_targets(test_infos)
        return build_targets, test_infos
