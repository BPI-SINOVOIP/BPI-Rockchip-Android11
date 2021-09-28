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
"""Unittests for suite_plan_finder."""

import os
import unittest
import mock

# pylint: disable=import-error
import unittest_constants as uc
import unittest_utils
from test_finders import test_finder_utils
from test_finders import test_info
from test_finders import suite_plan_finder
from test_runners import suite_plan_test_runner


# pylint: disable=protected-access
class SuitePlanFinderUnittests(unittest.TestCase):
    """Unit tests for suite_plan_finder.py"""

    def setUp(self):
        """Set up stuff for testing."""
        self.suite_plan_finder = suite_plan_finder.SuitePlanFinder()
        self.suite_plan_finder.suite_plan_dirs = [os.path.join(uc.ROOT, uc.CTS_INT_DIR)]
        self.suite_plan_finder.root_dir = uc.ROOT

    def test_get_test_info_from_path(self):
        """Test _get_test_info_from_path.
        Strategy:
            If suite_path is to cts file -->
                test_info: test_name=cts,
                           test_runner=TestSuiteTestRunner,
                           build_target=set(['cts']
                           suite='cts')
            If suite_path is to cts-common file -->
                test_info: test_name=cts-common,
                           test_runner=TestSuiteTestRunner,
                           build_target=set(['cts']
                           suite='cts')
            If suite_path is to common file --> test_info: None
            If suite_path is to non-existing file --> test_info: None
        """
        suite_plan = 'cts'
        path = os.path.join(uc.ROOT, uc.CTS_INT_DIR, suite_plan+'.xml')
        want_info = test_info.TestInfo(test_name=suite_plan,
                                       test_runner=suite_plan_test_runner.SuitePlanTestRunner.NAME,
                                       build_targets={suite_plan},
                                       suite=suite_plan)
        unittest_utils.assert_equal_testinfos(
            self, want_info, self.suite_plan_finder._get_test_info_from_path(path))

        suite_plan = 'cts-common'
        path = os.path.join(uc.ROOT, uc.CTS_INT_DIR, suite_plan+'.xml')
        want_info = test_info.TestInfo(test_name=suite_plan,
                                       test_runner=suite_plan_test_runner.SuitePlanTestRunner.NAME,
                                       build_targets={'cts'},
                                       suite='cts')
        unittest_utils.assert_equal_testinfos(
            self, want_info, self.suite_plan_finder._get_test_info_from_path(path))

        suite_plan = 'common'
        path = os.path.join(uc.ROOT, uc.CTS_INT_DIR, 'cts-common.xml')
        want_info = None
        unittest_utils.assert_equal_testinfos(
            self, want_info, self.suite_plan_finder._get_test_info_from_path(path, suite_plan))

        path = os.path.join(uc.ROOT, 'cts-common.xml')
        want_info = None
        unittest_utils.assert_equal_testinfos(
            self, want_info, self.suite_plan_finder._get_test_info_from_path(path))

    @mock.patch.object(test_finder_utils, 'search_integration_dirs')
    def test_find_test_by_suite_name(self, _search):
        """Test find_test_by_suite_name.
        Strategy:
            suite_name: cts --> test_info: test_name=cts,
                                           test_runner=TestSuiteTestRunner,
                                           build_target=set(['cts']
                                           suite='cts')
            suite_name: CTS --> test_info: None
            suite_name: cts-common --> test_info: test_name=cts-common,
                                                  test_runner=TestSuiteTestRunner,
                                                  build_target=set(['cts'],
                                                  suite='cts')
        """
        suite_name = 'cts'
        t_info = self.suite_plan_finder.find_test_by_suite_name(suite_name)
        want_info = test_info.TestInfo(test_name=suite_name,
                                       test_runner=suite_plan_test_runner.SuitePlanTestRunner.NAME,
                                       build_targets={suite_name},
                                       suite=suite_name)
        unittest_utils.assert_equal_testinfos(self, t_info[0], want_info)

        suite_name = 'CTS'
        _search.return_value = None
        t_info = self.suite_plan_finder.find_test_by_suite_name(suite_name)
        want_info = None
        unittest_utils.assert_equal_testinfos(self, t_info, want_info)

        suite_name = 'cts-common'
        suite = 'cts'
        _search.return_value = [os.path.join(uc.ROOT, uc.CTS_INT_DIR, suite_name + '.xml')]
        t_info = self.suite_plan_finder.find_test_by_suite_name(suite_name)
        want_info = test_info.TestInfo(test_name=suite_name,
                                       test_runner=suite_plan_test_runner.SuitePlanTestRunner.NAME,
                                       build_targets=set([suite]),
                                       suite=suite)
        unittest_utils.assert_equal_testinfos(self, t_info[0], want_info)

    @mock.patch('os.path.realpath',
                side_effect=unittest_utils.realpath_side_effect)
    @mock.patch('os.path.isdir', return_value=True)
    @mock.patch('os.path.isfile', return_value=True)
    @mock.patch.object(test_finder_utils, 'get_int_dir_from_path')
    @mock.patch('os.path.exists', return_value=True)
    def test_find_suite_plan_test_by_suite_path(self, _exists, _find, _isfile, _isdir, _real):
        """Test find_test_by_suite_name.
        Strategy:
            suite_name: cts.xml --> test_info:
                                        test_name=cts,
                                        test_runner=TestSuiteTestRunner,
                                        build_target=set(['cts']
                                        suite='cts')
            suite_name: cts-common.xml --> test_info:
                                               test_name=cts-common,
                                               test_runner=TestSuiteTestRunner,
                                               build_target=set(['cts'],
                                               suite='cts')
            suite_name: cts-camera.xml --> test_info:
                                               test_name=cts-camera,
                                               test_runner=TestSuiteTestRunner,
                                               build_target=set(['cts'],
                                               suite='cts')
        """
        suite_int_name = 'cts'
        suite = 'cts'
        path = os.path.join(uc.CTS_INT_DIR, suite_int_name + '.xml')
        _find.return_value = uc.CTS_INT_DIR
        t_info = self.suite_plan_finder.find_test_by_suite_path(path)
        want_info = test_info.TestInfo(test_name=suite_int_name,
                                       test_runner=suite_plan_test_runner.SuitePlanTestRunner.NAME,
                                       build_targets=set([suite]),
                                       suite=suite)
        unittest_utils.assert_equal_testinfos(self, t_info[0], want_info)

        suite_int_name = 'cts-common'
        suite = 'cts'
        path = os.path.join(uc.CTS_INT_DIR, suite_int_name + '.xml')
        _find.return_value = uc.CTS_INT_DIR
        t_info = self.suite_plan_finder.find_test_by_suite_path(path)
        want_info = test_info.TestInfo(test_name=suite_int_name,
                                       test_runner=suite_plan_test_runner.SuitePlanTestRunner.NAME,
                                       build_targets=set([suite]),
                                       suite=suite)
        unittest_utils.assert_equal_testinfos(self, t_info[0], want_info)

        suite_int_name = 'cts-camera'
        suite = 'cts'
        path = os.path.join(uc.CTS_INT_DIR, suite_int_name + '.xml')
        _find.return_value = uc.CTS_INT_DIR
        t_info = self.suite_plan_finder.find_test_by_suite_path(path)
        want_info = test_info.TestInfo(test_name=suite_int_name,
                                       test_runner=suite_plan_test_runner.SuitePlanTestRunner.NAME,
                                       build_targets=set([suite]),
                                       suite=suite)
        unittest_utils.assert_equal_testinfos(self, t_info[0], want_info)


if __name__ == '__main__':
    unittest.main()
