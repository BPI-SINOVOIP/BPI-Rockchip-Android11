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
Suite Plan Finder class.
"""

import logging
import os
import re

# pylint: disable=import-error
import constants
from test_finders import test_finder_base
from test_finders import test_finder_utils
from test_finders import test_info
from test_runners import suite_plan_test_runner

_SUITE_PLAN_NAME_RE = re.compile(r'^.*\/(?P<suite>.*)-tradefed\/res\/config\/'
                                 r'(?P<suite_plan_name>.*).xml$')


class SuitePlanFinder(test_finder_base.TestFinderBase):
    """Suite Plan Finder class."""
    NAME = 'SUITE_PLAN'
    _SUITE_PLAN_TEST_RUNNER = suite_plan_test_runner.SuitePlanTestRunner.NAME

    def __init__(self, module_info=None):
        super(SuitePlanFinder, self).__init__()
        self.root_dir = os.environ.get(constants.ANDROID_BUILD_TOP)
        self.mod_info = module_info
        self.suite_plan_dirs = self._get_suite_plan_dirs()

    def _get_mod_paths(self, module_name):
        """Return the paths of the given module name."""
        if self.mod_info:
            return self.mod_info.get_paths(module_name)
        return []

    def _get_suite_plan_dirs(self):
        """Get suite plan dirs from MODULE_INFO based on targets.

        Strategy:
            Search module-info.json using SUITE_PLANS to get all the suite
            plan dirs.

        Returns:
            A tuple of lists of strings of suite plan dir rel to repo root.
            None if the path can not be found in module-info.json.
        """
        return [d for x in constants.SUITE_PLANS for d in
                self._get_mod_paths(x+'-tradefed') if d is not None]

    def _get_test_info_from_path(self, path, suite_name=None):
        """Get the test info from the result of using regular expression
        matching with the give path.

        Args:
            path: A string of the test's absolute or relative path.
            suite_name: A string of the suite name.

        Returns:
            A populated TestInfo namedtuple if regular expression
            matches, else None.
        """
        # Don't use names that simply match the path,
        # must be the actual name used by *TS to run the test.
        match = _SUITE_PLAN_NAME_RE.match(path)
        if not match:
            logging.error('Suite plan test outside config dir: %s', path)
            return None
        suite = match.group('suite')
        suite_plan_name = match.group('suite_plan_name')
        if suite_name:
            if suite_plan_name != suite_name:
                logging.warn('Input (%s) not valid suite plan name, '
                             'did you mean: %s?', suite_name, suite_plan_name)
                return None
        return test_info.TestInfo(
            test_name=suite_plan_name,
            test_runner=self._SUITE_PLAN_TEST_RUNNER,
            build_targets=set([suite]),
            suite=suite)

    def find_test_by_suite_path(self, suite_path):
        """Find the first test info matching the given path.

        Strategy:
            If suite_path is to file --> Return TestInfo if the file
            exists in the suite plan dirs, else return None.
            If suite_path is to dir --> Return None

        Args:
            suite_path: A string of the path to the test's file or dir.

        Returns:
            A list of populated TestInfo namedtuple if test found, else None.
            This is a list with at most 1 element.
        """
        path, _ = test_finder_utils.split_methods(suite_path)
        # Make sure we're looking for a config.
        if not path.endswith('.xml'):
            return None
        path = os.path.realpath(path)
        suite_plan_dir = test_finder_utils.get_int_dir_from_path(
            path, self.suite_plan_dirs)
        if suite_plan_dir:
            rel_config = os.path.relpath(path, self.root_dir)
            return [self._get_test_info_from_path(rel_config)]
        return None

    def find_test_by_suite_name(self, suite_name):
        """Find the test for the given suite name.

        Strategy:
            If suite_name is cts --> Return TestInfo to indicate suite runner
            to make cts and run test using cts-tradefed.
            If suite_name is cts-common --> Return TestInfo to indicate suite
            runner to make cts and run test using cts-tradefed if file exists
            in the suite plan dirs, else return None.

        Args:
            suite_name: A string of suite name.

        Returns:
            A list of populated TestInfo namedtuple if suite_name matches
            a suite in constants.SUITE_PLAN, else check if the file
            existing in the suite plan dirs, else return None.
        """
        logging.debug('Finding test by suite: %s', suite_name)
        test_infos = []
        if suite_name in constants.SUITE_PLANS:
            test_infos.append(test_info.TestInfo(
                test_name=suite_name,
                test_runner=self._SUITE_PLAN_TEST_RUNNER,
                build_targets=set([suite_name]),
                suite=suite_name))
        else:
            test_files = test_finder_utils.search_integration_dirs(
                suite_name, self.suite_plan_dirs)
            if not test_files:
                return None
            for test_file in test_files:
                _test_info = self._get_test_info_from_path(test_file, suite_name)
                if _test_info:
                    test_infos.append(_test_info)
        return test_infos
