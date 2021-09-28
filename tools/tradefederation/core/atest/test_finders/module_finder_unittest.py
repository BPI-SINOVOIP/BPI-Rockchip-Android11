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

"""Unittests for module_finder."""

import re
import unittest
import os
import mock

# pylint: disable=import-error
import atest_error
import constants
import module_info
import unittest_constants as uc
import unittest_utils
from test_finders import module_finder
from test_finders import test_finder_utils
from test_finders import test_info
from test_runners import atest_tf_test_runner as atf_tr

MODULE_CLASS = '%s:%s' % (uc.MODULE_NAME, uc.CLASS_NAME)
MODULE_PACKAGE = '%s:%s' % (uc.MODULE_NAME, uc.PACKAGE)
CC_MODULE_CLASS = '%s:%s' % (uc.CC_MODULE_NAME, uc.CC_CLASS_NAME)
KERNEL_TEST_CLASS = 'test_class_1'
KERNEL_TEST_CONFIG = 'KernelTest.xml'
KERNEL_MODULE_CLASS = '%s:%s' % (constants.REQUIRED_KERNEL_TEST_MODULES[0],
                                 KERNEL_TEST_CLASS)
KERNEL_CONFIG_FILE = os.path.join(uc.TEST_DATA_DIR, KERNEL_TEST_CONFIG)
KERNEL_CLASS_FILTER = test_info.TestFilter(KERNEL_TEST_CLASS, frozenset())
KERNEL_MODULE_CLASS_DATA = {constants.TI_REL_CONFIG: KERNEL_CONFIG_FILE,
                            constants.TI_FILTER: frozenset([KERNEL_CLASS_FILTER])}
KERNEL_MODULE_CLASS_INFO = test_info.TestInfo(
    constants.REQUIRED_KERNEL_TEST_MODULES[0],
    atf_tr.AtestTradefedTestRunner.NAME,
    uc.CLASS_BUILD_TARGETS, KERNEL_MODULE_CLASS_DATA)
FLAT_METHOD_INFO = test_info.TestInfo(
    uc.MODULE_NAME,
    atf_tr.AtestTradefedTestRunner.NAME,
    uc.MODULE_BUILD_TARGETS,
    data={constants.TI_FILTER: frozenset([uc.FLAT_METHOD_FILTER]),
          constants.TI_REL_CONFIG: uc.CONFIG_FILE})
MODULE_CLASS_METHOD = '%s#%s' % (MODULE_CLASS, uc.METHOD_NAME)
CC_MODULE_CLASS_METHOD = '%s#%s' % (CC_MODULE_CLASS, uc.CC_METHOD_NAME)
CLASS_INFO_MODULE_2 = test_info.TestInfo(
    uc.MODULE2_NAME,
    atf_tr.AtestTradefedTestRunner.NAME,
    uc.CLASS_BUILD_TARGETS,
    data={constants.TI_FILTER: frozenset([uc.CLASS_FILTER]),
          constants.TI_REL_CONFIG: uc.CONFIG2_FILE})
CC_CLASS_INFO_MODULE_2 = test_info.TestInfo(
    uc.CC_MODULE2_NAME,
    atf_tr.AtestTradefedTestRunner.NAME,
    uc.CLASS_BUILD_TARGETS,
    data={constants.TI_FILTER: frozenset([uc.CC_CLASS_FILTER]),
          constants.TI_REL_CONFIG: uc.CC_CONFIG2_FILE})
DEFAULT_INSTALL_PATH = ['/path/to/install']
ROBO_MOD_PATH = ['/shared/robo/path']
NON_RUN_ROBO_MOD_NAME = 'robo_mod'
RUN_ROBO_MOD_NAME = 'run_robo_mod'
NON_RUN_ROBO_MOD = {constants.MODULE_NAME: NON_RUN_ROBO_MOD_NAME,
                    constants.MODULE_PATH: ROBO_MOD_PATH,
                    constants.MODULE_CLASS: ['random_class']}
RUN_ROBO_MOD = {constants.MODULE_NAME: RUN_ROBO_MOD_NAME,
                constants.MODULE_PATH: ROBO_MOD_PATH,
                constants.MODULE_CLASS: [constants.MODULE_CLASS_ROBOLECTRIC]}

SEARCH_DIR_RE = re.compile(r'^find ([^ ]*).*$')

#pylint: disable=unused-argument
def classoutside_side_effect(find_cmd, shell=False):
    """Mock the check output of a find cmd where class outside module path."""
    search_dir = SEARCH_DIR_RE.match(find_cmd).group(1).strip()
    if search_dir == uc.ROOT:
        return uc.FIND_ONE
    return None


#pylint: disable=protected-access
class ModuleFinderUnittests(unittest.TestCase):
    """Unit tests for module_finder.py"""

    def setUp(self):
        """Set up stuff for testing."""
        self.mod_finder = module_finder.ModuleFinder()
        self.mod_finder.module_info = mock.Mock(spec=module_info.ModuleInfo)
        self.mod_finder.module_info.path_to_module_info = {}
        self.mod_finder.root_dir = uc.ROOT

    def test_is_vts_module(self):
        """Test _load_module_info_file regular operation."""
        mod_name = 'mod'
        is_vts_module_info = {'compatibility_suites': ['vts10', 'tests']}
        self.mod_finder.module_info.get_module_info.return_value = is_vts_module_info
        self.assertTrue(self.mod_finder._is_vts_module(mod_name))

        is_not_vts_module = {'compatibility_suites': ['vts10', 'cts']}
        self.mod_finder.module_info.get_module_info.return_value = is_not_vts_module
        self.assertFalse(self.mod_finder._is_vts_module(mod_name))

    # pylint: disable=unused-argument
    @mock.patch.object(module_finder.ModuleFinder, '_get_build_targets',
                       return_value=uc.MODULE_BUILD_TARGETS)
    def test_find_test_by_module_name(self, _get_targ):
        """Test find_test_by_module_name."""
        self.mod_finder.module_info.is_robolectric_test.return_value = False
        self.mod_finder.module_info.has_test_config.return_value = True
        mod_info = {'installed': ['/path/to/install'],
                    'path': [uc.MODULE_DIR],
                    constants.MODULE_CLASS: [],
                    constants.MODULE_COMPATIBILITY_SUITES: []}
        self.mod_finder.module_info.get_module_info.return_value = mod_info
        t_infos = self.mod_finder.find_test_by_module_name(uc.MODULE_NAME)
        unittest_utils.assert_equal_testinfos(
            self,
            t_infos[0],
            uc.MODULE_INFO)
        self.mod_finder.module_info.get_module_info.return_value = None
        self.mod_finder.module_info.is_testable_module.return_value = False
        self.assertIsNone(self.mod_finder.find_test_by_module_name('Not_Module'))

    @mock.patch.object(test_finder_utils, 'has_method_in_file',
                       return_value=True)
    @mock.patch.object(module_finder.ModuleFinder, '_is_vts_module',
                       return_value=False)
    @mock.patch.object(module_finder.ModuleFinder, '_get_build_targets')
    @mock.patch('subprocess.check_output', return_value=uc.FIND_ONE)
    @mock.patch.object(test_finder_utils, 'get_fully_qualified_class_name',
                       return_value=uc.FULL_CLASS_NAME)
    @mock.patch('os.path.isfile', side_effect=unittest_utils.isfile_side_effect)
    @mock.patch('os.path.isdir', return_value=True)
    #pylint: disable=unused-argument
    def test_find_test_by_class_name(self, _isdir, _isfile, _fqcn,
                                     mock_checkoutput, mock_build,
                                     _vts, _has_method_in_file):
        """Test find_test_by_class_name."""
        mock_build.return_value = uc.CLASS_BUILD_TARGETS
        self.mod_finder.module_info.is_auto_gen_test_config.return_value = False
        self.mod_finder.module_info.is_robolectric_test.return_value = False
        self.mod_finder.module_info.has_test_config.return_value = True
        self.mod_finder.module_info.get_module_names.return_value = [uc.MODULE_NAME]
        self.mod_finder.module_info.get_module_info.return_value = {
            constants.MODULE_INSTALLED: DEFAULT_INSTALL_PATH,
            constants.MODULE_NAME: uc.MODULE_NAME,
            constants.MODULE_CLASS: [],
            constants.MODULE_COMPATIBILITY_SUITES: []}
        t_infos = self.mod_finder.find_test_by_class_name(uc.CLASS_NAME)
        unittest_utils.assert_equal_testinfos(
            self, t_infos[0], uc.CLASS_INFO)

        # with method
        mock_build.return_value = uc.MODULE_BUILD_TARGETS
        class_with_method = '%s#%s' % (uc.CLASS_NAME, uc.METHOD_NAME)
        t_infos = self.mod_finder.find_test_by_class_name(class_with_method)
        unittest_utils.assert_equal_testinfos(
            self, t_infos[0], uc.METHOD_INFO)
        mock_build.return_value = uc.MODULE_BUILD_TARGETS
        class_methods = '%s,%s' % (class_with_method, uc.METHOD2_NAME)
        t_infos = self.mod_finder.find_test_by_class_name(class_methods)
        unittest_utils.assert_equal_testinfos(
            self, t_infos[0],
            FLAT_METHOD_INFO)
        # module and rel_config passed in
        mock_build.return_value = uc.CLASS_BUILD_TARGETS
        t_infos = self.mod_finder.find_test_by_class_name(
            uc.CLASS_NAME, uc.MODULE_NAME, uc.CONFIG_FILE)
        unittest_utils.assert_equal_testinfos(
            self, t_infos[0], uc.CLASS_INFO)
        # find output fails to find class file
        mock_checkoutput.return_value = ''
        self.assertIsNone(self.mod_finder.find_test_by_class_name('Not class'))
        # class is outside given module path
        mock_checkoutput.side_effect = classoutside_side_effect
        t_infos = self.mod_finder.find_test_by_class_name(uc.CLASS_NAME,
                                                          uc.MODULE2_NAME,
                                                          uc.CONFIG2_FILE)
        unittest_utils.assert_equal_testinfos(
            self, t_infos[0],
            CLASS_INFO_MODULE_2)

    @mock.patch.object(test_finder_utils, 'has_method_in_file',
                       return_value=True)
    @mock.patch.object(module_finder.ModuleFinder, '_is_vts_module',
                       return_value=False)
    @mock.patch.object(module_finder.ModuleFinder, '_get_build_targets')
    @mock.patch('subprocess.check_output', return_value=uc.FIND_ONE)
    @mock.patch.object(test_finder_utils, 'get_fully_qualified_class_name',
                       return_value=uc.FULL_CLASS_NAME)
    @mock.patch('os.path.isfile', side_effect=unittest_utils.isfile_side_effect)
    #pylint: disable=unused-argument
    def test_find_test_by_module_and_class(self, _isfile, _fqcn,
                                           mock_checkoutput, mock_build,
                                           _vts, _has_method_in_file):
        """Test find_test_by_module_and_class."""
        # Native test was tested in test_find_test_by_cc_class_name().
        self.mod_finder.module_info.is_native_test.return_value = False
        self.mod_finder.module_info.is_auto_gen_test_config.return_value = False
        self.mod_finder.module_info.is_robolectric_test.return_value = False
        self.mod_finder.module_info.has_test_config.return_value = True
        mock_build.return_value = uc.CLASS_BUILD_TARGETS
        mod_info = {constants.MODULE_INSTALLED: DEFAULT_INSTALL_PATH,
                    constants.MODULE_PATH: [uc.MODULE_DIR],
                    constants.MODULE_CLASS: [],
                    constants.MODULE_COMPATIBILITY_SUITES: []}
        self.mod_finder.module_info.get_module_info.return_value = mod_info
        t_infos = self.mod_finder.find_test_by_module_and_class(MODULE_CLASS)
        unittest_utils.assert_equal_testinfos(self, t_infos[0], uc.CLASS_INFO)
        # with method
        mock_build.return_value = uc.MODULE_BUILD_TARGETS
        t_infos = self.mod_finder.find_test_by_module_and_class(MODULE_CLASS_METHOD)
        unittest_utils.assert_equal_testinfos(self, t_infos[0], uc.METHOD_INFO)
        self.mod_finder.module_info.is_testable_module.return_value = False
        # bad module, good class, returns None
        bad_module = '%s:%s' % ('BadMod', uc.CLASS_NAME)
        self.mod_finder.module_info.get_module_info.return_value = None
        self.assertIsNone(self.mod_finder.find_test_by_module_and_class(bad_module))
        # find output fails to find class file
        mock_checkoutput.return_value = ''
        bad_class = '%s:%s' % (uc.MODULE_NAME, 'Anything')
        self.mod_finder.module_info.get_module_info.return_value = mod_info
        self.assertIsNone(self.mod_finder.find_test_by_module_and_class(bad_class))

    @mock.patch.object(module_finder.ModuleFinder, 'find_test_by_kernel_class_name',
                       return_value=None)
    @mock.patch.object(module_finder.ModuleFinder, '_is_vts_module',
                       return_value=False)
    @mock.patch.object(module_finder.ModuleFinder, '_get_build_targets')
    @mock.patch('subprocess.check_output', return_value=uc.FIND_CC_ONE)
    @mock.patch.object(test_finder_utils, 'find_class_file',
                       side_effect=[None, None, '/'])
    @mock.patch('os.path.isfile', side_effect=unittest_utils.isfile_side_effect)
    #pylint: disable=unused-argument
    def test_find_test_by_module_and_class_part_2(self, _isfile, mock_fcf,
                                                  mock_checkoutput, mock_build,
                                                  _vts, _find_kernel):
        """Test find_test_by_module_and_class for MODULE:CC_CLASS."""
        # Native test was tested in test_find_test_by_cc_class_name()
        self.mod_finder.module_info.is_native_test.return_value = False
        self.mod_finder.module_info.is_auto_gen_test_config.return_value = False
        self.mod_finder.module_info.is_robolectric_test.return_value = False
        self.mod_finder.module_info.has_test_config.return_value = True
        mock_build.return_value = uc.CLASS_BUILD_TARGETS
        mod_info = {constants.MODULE_INSTALLED: DEFAULT_INSTALL_PATH,
                    constants.MODULE_PATH: [uc.CC_MODULE_DIR],
                    constants.MODULE_CLASS: [],
                    constants.MODULE_COMPATIBILITY_SUITES: []}
        self.mod_finder.module_info.get_module_info.return_value = mod_info
        t_infos = self.mod_finder.find_test_by_module_and_class(CC_MODULE_CLASS)
        unittest_utils.assert_equal_testinfos(self, t_infos[0], uc.CC_MODULE_CLASS_INFO)
        # with method
        mock_build.return_value = uc.MODULE_BUILD_TARGETS
        mock_fcf.side_effect = [None, None, '/']
        t_infos = self.mod_finder.find_test_by_module_and_class(CC_MODULE_CLASS_METHOD)
        unittest_utils.assert_equal_testinfos(self, t_infos[0], uc.CC_METHOD_INFO)
        # bad module, good class, returns None
        bad_module = '%s:%s' % ('BadMod', uc.CC_CLASS_NAME)
        self.mod_finder.module_info.get_module_info.return_value = None
        self.mod_finder.module_info.is_testable_module.return_value = False
        self.assertIsNone(self.mod_finder.find_test_by_module_and_class(bad_module))

    @mock.patch.object(module_finder.ModuleFinder, '_get_module_test_config',
                       return_value=KERNEL_CONFIG_FILE)
    @mock.patch.object(module_finder.ModuleFinder, '_is_vts_module',
                       return_value=False)
    @mock.patch.object(module_finder.ModuleFinder, '_get_build_targets')
    @mock.patch('subprocess.check_output', return_value=uc.FIND_CC_ONE)
    @mock.patch.object(test_finder_utils, 'find_class_file',
                       side_effect=[None, None, '/'])
    @mock.patch('os.path.isfile', side_effect=unittest_utils.isfile_side_effect)
    #pylint: disable=unused-argument
    def test_find_test_by_module_and_class_for_kernel_test(
            self, _isfile, mock_fcf, mock_checkoutput, mock_build, _vts,
            _test_config):
        """Test find_test_by_module_and_class for MODULE:CC_CLASS."""
        # Kernel test was tested in find_test_by_kernel_class_name()
        self.mod_finder.module_info.is_native_test.return_value = False
        self.mod_finder.module_info.is_auto_gen_test_config.return_value = False
        self.mod_finder.module_info.is_robolectric_test.return_value = False
        self.mod_finder.module_info.has_test_config.return_value = True
        mock_build.return_value = uc.CLASS_BUILD_TARGETS
        mod_info = {constants.MODULE_INSTALLED: DEFAULT_INSTALL_PATH,
                    constants.MODULE_PATH: [uc.CC_MODULE_DIR],
                    constants.MODULE_CLASS: [],
                    constants.MODULE_COMPATIBILITY_SUITES: []}
        self.mod_finder.module_info.get_module_info.return_value = mod_info
        t_infos = self.mod_finder.find_test_by_module_and_class(KERNEL_MODULE_CLASS)
        unittest_utils.assert_equal_testinfos(self, t_infos[0], KERNEL_MODULE_CLASS_INFO)

    @mock.patch.object(module_finder.ModuleFinder, '_is_vts_module',
                       return_value=False)
    @mock.patch.object(module_finder.ModuleFinder, '_get_build_targets')
    @mock.patch('subprocess.check_output', return_value=uc.FIND_PKG)
    @mock.patch('os.path.isfile', side_effect=unittest_utils.isfile_side_effect)
    @mock.patch('os.path.isdir', return_value=True)
    #pylint: disable=unused-argument
    def test_find_test_by_package_name(self, _isdir, _isfile, mock_checkoutput,
                                       mock_build, _vts):
        """Test find_test_by_package_name."""
        self.mod_finder.module_info.is_auto_gen_test_config.return_value = False
        self.mod_finder.module_info.is_robolectric_test.return_value = False
        self.mod_finder.module_info.has_test_config.return_value = True
        mock_build.return_value = uc.CLASS_BUILD_TARGETS
        self.mod_finder.module_info.get_module_names.return_value = [uc.MODULE_NAME]
        self.mod_finder.module_info.get_module_info.return_value = {
            constants.MODULE_INSTALLED: DEFAULT_INSTALL_PATH,
            constants.MODULE_NAME: uc.MODULE_NAME,
            constants.MODULE_CLASS: [],
            constants.MODULE_COMPATIBILITY_SUITES: []
            }
        t_infos = self.mod_finder.find_test_by_package_name(uc.PACKAGE)
        unittest_utils.assert_equal_testinfos(
            self, t_infos[0],
            uc.PACKAGE_INFO)
        # with method, should raise
        pkg_with_method = '%s#%s' % (uc.PACKAGE, uc.METHOD_NAME)
        self.assertRaises(atest_error.MethodWithoutClassError,
                          self.mod_finder.find_test_by_package_name,
                          pkg_with_method)
        # module and rel_config passed in
        t_infos = self.mod_finder.find_test_by_package_name(
            uc.PACKAGE, uc.MODULE_NAME, uc.CONFIG_FILE)
        unittest_utils.assert_equal_testinfos(
            self, t_infos[0], uc.PACKAGE_INFO)
        # find output fails to find class file
        mock_checkoutput.return_value = ''
        self.assertIsNone(self.mod_finder.find_test_by_package_name('Not pkg'))

    @mock.patch('os.path.isdir', return_value=False)
    @mock.patch.object(module_finder.ModuleFinder, '_is_vts_module',
                       return_value=False)
    @mock.patch.object(module_finder.ModuleFinder, '_get_build_targets')
    @mock.patch('subprocess.check_output', return_value=uc.FIND_PKG)
    @mock.patch('os.path.isfile', side_effect=unittest_utils.isfile_side_effect)
    #pylint: disable=unused-argument
    def test_find_test_by_module_and_package(self, _isfile, mock_checkoutput,
                                             mock_build, _vts, _isdir):
        """Test find_test_by_module_and_package."""
        self.mod_finder.module_info.is_auto_gen_test_config.return_value = False
        self.mod_finder.module_info.is_robolectric_test.return_value = False
        self.mod_finder.module_info.has_test_config.return_value = True
        mock_build.return_value = uc.CLASS_BUILD_TARGETS
        mod_info = {constants.MODULE_INSTALLED: DEFAULT_INSTALL_PATH,
                    constants.MODULE_PATH: [uc.MODULE_DIR],
                    constants.MODULE_CLASS: [],
                    constants.MODULE_COMPATIBILITY_SUITES: []}
        self.mod_finder.module_info.get_module_info.return_value = mod_info
        t_infos = self.mod_finder.find_test_by_module_and_package(MODULE_PACKAGE)
        self.assertEqual(t_infos, None)
        _isdir.return_value = True
        t_infos = self.mod_finder.find_test_by_module_and_package(MODULE_PACKAGE)
        unittest_utils.assert_equal_testinfos(self, t_infos[0], uc.PACKAGE_INFO)

        # with method, raises
        module_pkg_with_method = '%s:%s#%s' % (uc.MODULE2_NAME, uc.PACKAGE,
                                               uc.METHOD_NAME)
        self.assertRaises(atest_error.MethodWithoutClassError,
                          self.mod_finder.find_test_by_module_and_package,
                          module_pkg_with_method)
        # bad module, good pkg, returns None
        self.mod_finder.module_info.is_testable_module.return_value = False
        bad_module = '%s:%s' % ('BadMod', uc.PACKAGE)
        self.mod_finder.module_info.get_module_info.return_value = None
        self.assertIsNone(self.mod_finder.find_test_by_module_and_package(bad_module))
        # find output fails to find package path
        mock_checkoutput.return_value = ''
        bad_pkg = '%s:%s' % (uc.MODULE_NAME, 'Anything')
        self.mod_finder.module_info.get_module_info.return_value = mod_info
        self.assertIsNone(self.mod_finder.find_test_by_module_and_package(bad_pkg))

    @mock.patch.object(test_finder_utils, 'has_method_in_file',
                       return_value=True)
    @mock.patch.object(test_finder_utils, 'has_cc_class',
                       return_value=True)
    @mock.patch.object(module_finder.ModuleFinder, '_get_build_targets')
    @mock.patch.object(module_finder.ModuleFinder, '_is_vts_module',
                       return_value=False)
    @mock.patch.object(test_finder_utils, 'get_fully_qualified_class_name',
                       return_value=uc.FULL_CLASS_NAME)
    @mock.patch('os.path.realpath',
                side_effect=unittest_utils.realpath_side_effect)
    @mock.patch('os.path.isfile', side_effect=unittest_utils.isfile_side_effect)
    @mock.patch.object(test_finder_utils, 'find_parent_module_dir')
    @mock.patch('os.path.exists')
    #pylint: disable=unused-argument
    def test_find_test_by_path(self, mock_pathexists, mock_dir, _isfile, _real,
                               _fqcn, _vts, mock_build, _has_cc_class,
                               _has_method_in_file):
        """Test find_test_by_path."""
        self.mod_finder.module_info.is_robolectric_test.return_value = False
        self.mod_finder.module_info.has_test_config.return_value = True
        mock_build.return_value = set()
        # Check that we don't return anything with invalid test references.
        mock_pathexists.return_value = False
        unittest_utils.assert_equal_testinfos(
            self, None, self.mod_finder.find_test_by_path('bad/path'))
        mock_pathexists.return_value = True
        mock_dir.return_value = None
        unittest_utils.assert_equal_testinfos(
            self, None, self.mod_finder.find_test_by_path('no/module'))
        self.mod_finder.module_info.get_module_names.return_value = [uc.MODULE_NAME]
        self.mod_finder.module_info.get_module_info.return_value = {
            constants.MODULE_INSTALLED: DEFAULT_INSTALL_PATH,
            constants.MODULE_NAME: uc.MODULE_NAME,
            constants.MODULE_CLASS: [],
            constants.MODULE_COMPATIBILITY_SUITES: []}

        # Happy path testing.
        mock_dir.return_value = uc.MODULE_DIR

        class_path = '%s.kt' % uc.CLASS_NAME
        mock_build.return_value = uc.CLASS_BUILD_TARGETS
        t_infos = self.mod_finder.find_test_by_path(class_path)
        unittest_utils.assert_equal_testinfos(
            self, uc.CLASS_INFO, t_infos[0])

        class_path = '%s.java' % uc.CLASS_NAME
        mock_build.return_value = uc.CLASS_BUILD_TARGETS
        t_infos = self.mod_finder.find_test_by_path(class_path)
        unittest_utils.assert_equal_testinfos(
            self, uc.CLASS_INFO, t_infos[0])

        class_with_method = '%s#%s' % (class_path, uc.METHOD_NAME)
        mock_build.return_value = uc.MODULE_BUILD_TARGETS
        t_infos = self.mod_finder.find_test_by_path(class_with_method)
        unittest_utils.assert_equal_testinfos(
            self, t_infos[0], uc.METHOD_INFO)

        class_with_methods = '%s,%s' % (class_with_method, uc.METHOD2_NAME)
        mock_build.return_value = uc.MODULE_BUILD_TARGETS
        t_infos = self.mod_finder.find_test_by_path(class_with_methods)
        unittest_utils.assert_equal_testinfos(
            self, t_infos[0],
            FLAT_METHOD_INFO)

        # Cc path testing.
        self.mod_finder.module_info.get_module_names.return_value = [uc.CC_MODULE_NAME]
        self.mod_finder.module_info.get_module_info.return_value = {
            constants.MODULE_INSTALLED: DEFAULT_INSTALL_PATH,
            constants.MODULE_NAME: uc.CC_MODULE_NAME,
            constants.MODULE_CLASS: [],
            constants.MODULE_COMPATIBILITY_SUITES: []}
        mock_dir.return_value = uc.CC_MODULE_DIR
        class_path = '%s' % uc.CC_PATH
        mock_build.return_value = uc.CLASS_BUILD_TARGETS
        t_infos = self.mod_finder.find_test_by_path(class_path)
        unittest_utils.assert_equal_testinfos(
            self, uc.CC_PATH_INFO2, t_infos[0])

    @mock.patch.object(module_finder.ModuleFinder, '_get_build_targets',
                       return_value=uc.MODULE_BUILD_TARGETS)
    @mock.patch.object(module_finder.ModuleFinder, '_is_vts_module',
                       return_value=False)
    @mock.patch.object(test_finder_utils, 'find_parent_module_dir',
                       return_value=os.path.relpath(uc.TEST_DATA_DIR, uc.ROOT))
    #pylint: disable=unused-argument
    def test_find_test_by_path_part_2(self, _find_parent, _is_vts, _get_build):
        """Test find_test_by_path for directories."""
        self.mod_finder.module_info.is_auto_gen_test_config.return_value = False
        self.mod_finder.module_info.is_robolectric_test.return_value = False
        self.mod_finder.module_info.has_test_config.return_value = True
        # Dir with java files in it, should run as package
        class_dir = os.path.join(uc.TEST_DATA_DIR, 'path_testing')
        self.mod_finder.module_info.get_module_names.return_value = [uc.MODULE_NAME]
        self.mod_finder.module_info.get_module_info.return_value = {
            constants.MODULE_INSTALLED: DEFAULT_INSTALL_PATH,
            constants.MODULE_NAME: uc.MODULE_NAME,
            constants.MODULE_CLASS: [],
            constants.MODULE_COMPATIBILITY_SUITES: []}
        t_infos = self.mod_finder.find_test_by_path(class_dir)
        unittest_utils.assert_equal_testinfos(
            self, uc.PATH_INFO, t_infos[0])
        # Dir with no java files in it, should run whole module
        empty_dir = os.path.join(uc.TEST_DATA_DIR, 'path_testing_empty')
        t_infos = self.mod_finder.find_test_by_path(empty_dir)
        unittest_utils.assert_equal_testinfos(
            self, uc.EMPTY_PATH_INFO,
            t_infos[0])
        # Dir with cc files in it, should run as cc class
        class_dir = os.path.join(uc.TEST_DATA_DIR, 'cc_path_testing')
        self.mod_finder.module_info.get_module_names.return_value = [uc.CC_MODULE_NAME]
        self.mod_finder.module_info.get_module_info.return_value = {
            constants.MODULE_INSTALLED: DEFAULT_INSTALL_PATH,
            constants.MODULE_NAME: uc.CC_MODULE_NAME,
            constants.MODULE_CLASS: [],
            constants.MODULE_COMPATIBILITY_SUITES: []}
        t_infos = self.mod_finder.find_test_by_path(class_dir)
        unittest_utils.assert_equal_testinfos(
            self, uc.CC_PATH_INFO, t_infos[0])

    @mock.patch.object(test_finder_utils, 'has_method_in_file',
                       return_value=True)
    @mock.patch.object(module_finder.ModuleFinder, '_is_vts_module',
                       return_value=False)
    @mock.patch.object(module_finder.ModuleFinder, '_get_build_targets')
    @mock.patch('subprocess.check_output', return_value=uc.CC_FIND_ONE)
    @mock.patch('os.path.isfile', side_effect=unittest_utils.isfile_side_effect)
    @mock.patch('os.path.isdir', return_value=True)
    #pylint: disable=unused-argument
    def test_find_test_by_cc_class_name(self, _isdir, _isfile,
                                        mock_checkoutput, mock_build,
                                        _vts, _has_method):
        """Test find_test_by_cc_class_name."""
        mock_build.return_value = uc.CLASS_BUILD_TARGETS
        self.mod_finder.module_info.is_auto_gen_test_config.return_value = False
        self.mod_finder.module_info.is_robolectric_test.return_value = False
        self.mod_finder.module_info.has_test_config.return_value = True
        self.mod_finder.module_info.get_module_names.return_value = [uc.CC_MODULE_NAME]
        self.mod_finder.module_info.get_module_info.return_value = {
            constants.MODULE_INSTALLED: DEFAULT_INSTALL_PATH,
            constants.MODULE_NAME: uc.CC_MODULE_NAME,
            constants.MODULE_CLASS: [],
            constants.MODULE_COMPATIBILITY_SUITES: []}
        t_infos = self.mod_finder.find_test_by_cc_class_name(uc.CC_CLASS_NAME)
        unittest_utils.assert_equal_testinfos(
            self, t_infos[0], uc.CC_CLASS_INFO)

        # with method
        mock_build.return_value = uc.MODULE_BUILD_TARGETS
        class_with_method = '%s#%s' % (uc.CC_CLASS_NAME, uc.CC_METHOD_NAME)
        t_infos = self.mod_finder.find_test_by_cc_class_name(class_with_method)
        unittest_utils.assert_equal_testinfos(
            self,
            t_infos[0],
            uc.CC_METHOD_INFO)
        mock_build.return_value = uc.MODULE_BUILD_TARGETS
        class_methods = '%s,%s' % (class_with_method, uc.CC_METHOD2_NAME)
        t_infos = self.mod_finder.find_test_by_cc_class_name(class_methods)
        unittest_utils.assert_equal_testinfos(
            self, t_infos[0],
            uc.CC_METHOD2_INFO)
        # module and rel_config passed in
        mock_build.return_value = uc.CLASS_BUILD_TARGETS
        t_infos = self.mod_finder.find_test_by_cc_class_name(
            uc.CC_CLASS_NAME, uc.CC_MODULE_NAME, uc.CC_CONFIG_FILE)
        unittest_utils.assert_equal_testinfos(
            self, t_infos[0], uc.CC_CLASS_INFO)
        # find output fails to find class file
        mock_checkoutput.return_value = ''
        self.assertIsNone(self.mod_finder.find_test_by_cc_class_name(
            'Not class'))
        # class is outside given module path
        mock_checkoutput.return_value = uc.CC_FIND_ONE
        t_infos = self.mod_finder.find_test_by_cc_class_name(
            uc.CC_CLASS_NAME,
            uc.CC_MODULE2_NAME,
            uc.CC_CONFIG2_FILE)
        unittest_utils.assert_equal_testinfos(
            self, t_infos[0],
            CC_CLASS_INFO_MODULE_2)

    def test_get_testable_modules_with_ld(self):
        """Test get_testable_modules_with_ld"""
        self.mod_finder.module_info.get_testable_modules.return_value = [
            uc.MODULE_NAME, uc.MODULE2_NAME]
        # Without a misfit constraint
        ld1 = self.mod_finder.get_testable_modules_with_ld(uc.TYPO_MODULE_NAME)
        self.assertEqual([[16, uc.MODULE2_NAME], [1, uc.MODULE_NAME]], ld1)
        # With a misfit constraint
        ld2 = self.mod_finder.get_testable_modules_with_ld(uc.TYPO_MODULE_NAME, 2)
        self.assertEqual([[1, uc.MODULE_NAME]], ld2)

    def test_get_fuzzy_searching_modules(self):
        """Test get_fuzzy_searching_modules"""
        self.mod_finder.module_info.get_testable_modules.return_value = [
            uc.MODULE_NAME, uc.MODULE2_NAME]
        result = self.mod_finder.get_fuzzy_searching_results(uc.TYPO_MODULE_NAME)
        self.assertEqual(uc.MODULE_NAME, result[0])

    def test_get_build_targets_w_vts_core(self):
        """Test _get_build_targets."""
        self.mod_finder.module_info.is_auto_gen_test_config.return_value = True
        self.mod_finder.module_info.get_paths.return_value = []
        mod_info = {constants.MODULE_COMPATIBILITY_SUITES:
                        [constants.VTS_CORE_SUITE]}
        self.mod_finder.module_info.get_module_info.return_value = mod_info
        self.assertEqual(self.mod_finder._get_build_targets('', ''),
                         {constants.VTS_CORE_TF_MODULE})


if __name__ == '__main__':
    unittest.main()
