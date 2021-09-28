#!/usr/bin/env python3
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

"""Unittests for module_info."""

# pylint: disable=line-too-long

import os
import unittest

from unittest import mock

import constants
import module_info
import unittest_utils
import unittest_constants as uc

JSON_FILE_PATH = os.path.join(uc.TEST_DATA_DIR, uc.JSON_FILE)
EXPECTED_MOD_TARGET = 'tradefed'
EXPECTED_MOD_TARGET_PATH = ['tf/core']
UNEXPECTED_MOD_TARGET = 'this_should_not_be_in_module-info.json'
MOD_NO_PATH = 'module-no-path'
PATH_TO_MULT_MODULES = 'shared/path/to/be/used'
MULT_MOODULES_WITH_SHARED_PATH = ['module2', 'module1']
PATH_TO_MULT_MODULES_WITH_MULTI_ARCH = 'shared/path/to/be/used2'
TESTABLE_MODULES_WITH_SHARED_PATH = ['multiarch1', 'multiarch2', 'multiarch3', 'multiarch3_32']

ROBO_MOD_PATH = ['/shared/robo/path']
NON_RUN_ROBO_MOD_NAME = 'robo_mod'
RUN_ROBO_MOD_NAME = 'run_robo_mod'
NON_RUN_ROBO_MOD = {constants.MODULE_NAME: NON_RUN_ROBO_MOD_NAME,
                    constants.MODULE_PATH: ROBO_MOD_PATH,
                    constants.MODULE_CLASS: ['random_class']}
RUN_ROBO_MOD = {constants.MODULE_NAME: RUN_ROBO_MOD_NAME,
                constants.MODULE_PATH: ROBO_MOD_PATH,
                constants.MODULE_CLASS: [constants.MODULE_CLASS_ROBOLECTRIC]}
MOD_PATH_INFO_DICT = {ROBO_MOD_PATH[0]: [RUN_ROBO_MOD, NON_RUN_ROBO_MOD]}
MOD_NAME_INFO_DICT = {
    RUN_ROBO_MOD_NAME: RUN_ROBO_MOD,
    NON_RUN_ROBO_MOD_NAME: NON_RUN_ROBO_MOD}
MOD_NAME1 = 'mod1'
MOD_NAME2 = 'mod2'
MOD_NAME3 = 'mod3'
MOD_NAME4 = 'mod4'
MOD_INFO_DICT = {}
MODULE_INFO = {constants.MODULE_NAME: 'random_name',
               constants.MODULE_PATH: 'a/b/c/path',
               constants.MODULE_CLASS: ['random_class']}
NAME_TO_MODULE_INFO = {'random_name' : MODULE_INFO}

#pylint: disable=protected-access
class ModuleInfoUnittests(unittest.TestCase):
    """Unit tests for module_info.py"""

    @mock.patch('json.load', return_value={})
    @mock.patch('builtins.open', new_callable=mock.mock_open)
    @mock.patch('os.path.isfile', return_value=True)
    def test_load_mode_info_file_out_dir_handling(self, _isfile, _open, _json):
        """Test _load_module_info_file out dir handling."""
        # Test out default out dir is used.
        build_top = '/path/to/top'
        default_out_dir = os.path.join(build_top, 'out/dir/here')
        os_environ_mock = {'ANDROID_PRODUCT_OUT': default_out_dir,
                           constants.ANDROID_BUILD_TOP: build_top}
        default_out_dir_mod_targ = 'out/dir/here/module-info.json'
        # Make sure module_info_target is what we think it is.
        with mock.patch.dict('os.environ', os_environ_mock, clear=True):
            mod_info = module_info.ModuleInfo()
            self.assertEqual(default_out_dir_mod_targ,
                             mod_info.module_info_target)

        # Test out custom out dir is used (OUT_DIR=dir2).
        custom_out_dir = os.path.join(build_top, 'out2/dir/here')
        os_environ_mock = {'ANDROID_PRODUCT_OUT': custom_out_dir,
                           constants.ANDROID_BUILD_TOP: build_top}
        custom_out_dir_mod_targ = 'out2/dir/here/module-info.json'
        # Make sure module_info_target is what we think it is.
        with mock.patch.dict('os.environ', os_environ_mock, clear=True):
            mod_info = module_info.ModuleInfo()
            self.assertEqual(custom_out_dir_mod_targ,
                             mod_info.module_info_target)

        # Test out custom abs out dir is used (OUT_DIR=/tmp/out/dir2).
        abs_custom_out_dir = '/tmp/out/dir'
        os_environ_mock = {'ANDROID_PRODUCT_OUT': abs_custom_out_dir,
                           constants.ANDROID_BUILD_TOP: build_top}
        custom_abs_out_dir_mod_targ = '/tmp/out/dir/module-info.json'
        # Make sure module_info_target is what we think it is.
        with mock.patch.dict('os.environ', os_environ_mock, clear=True):
            mod_info = module_info.ModuleInfo()
            self.assertEqual(custom_abs_out_dir_mod_targ,
                             mod_info.module_info_target)

    @mock.patch.object(module_info.ModuleInfo, '_load_module_info_file',)
    def test_get_path_to_module_info(self, mock_load_module):
        """Test that we correctly create the path to module info dict."""
        mod_one = 'mod1'
        mod_two = 'mod2'
        mod_path_one = '/path/to/mod1'
        mod_path_two = '/path/to/mod2'
        mod_info_dict = {mod_one: {constants.MODULE_PATH: [mod_path_one],
                                   constants.MODULE_NAME: mod_one},
                         mod_two: {constants.MODULE_PATH: [mod_path_two],
                                   constants.MODULE_NAME: mod_two}}
        mock_load_module.return_value = ('mod_target', mod_info_dict)
        path_to_mod_info = {mod_path_one: [{constants.MODULE_NAME: mod_one,
                                            constants.MODULE_PATH: [mod_path_one]}],
                            mod_path_two: [{constants.MODULE_NAME: mod_two,
                                            constants.MODULE_PATH: [mod_path_two]}]}
        mod_info = module_info.ModuleInfo()
        self.assertDictEqual(path_to_mod_info,
                             mod_info._get_path_to_module_info(mod_info_dict))

    def test_is_module(self):
        """Test that we get the module when it's properly loaded."""
        # Load up the test json file and check that module is in it
        mod_info = module_info.ModuleInfo(module_file=JSON_FILE_PATH)
        self.assertTrue(mod_info.is_module(EXPECTED_MOD_TARGET))
        self.assertFalse(mod_info.is_module(UNEXPECTED_MOD_TARGET))

    def test_get_path(self):
        """Test that we get the module path when it's properly loaded."""
        # Load up the test json file and check that module is in it
        mod_info = module_info.ModuleInfo(module_file=JSON_FILE_PATH)
        self.assertEqual(mod_info.get_paths(EXPECTED_MOD_TARGET),
                         EXPECTED_MOD_TARGET_PATH)
        self.assertEqual(mod_info.get_paths(MOD_NO_PATH), [])

    def test_get_module_names(self):
        """test that we get the module name properly."""
        mod_info = module_info.ModuleInfo(module_file=JSON_FILE_PATH)
        self.assertEqual(mod_info.get_module_names(EXPECTED_MOD_TARGET_PATH[0]),
                         [EXPECTED_MOD_TARGET])
        unittest_utils.assert_strict_equal(
            self, mod_info.get_module_names(PATH_TO_MULT_MODULES),
            MULT_MOODULES_WITH_SHARED_PATH)

    def test_path_to_mod_info(self):
        """test that we get the module name properly."""
        mod_info = module_info.ModuleInfo(module_file=JSON_FILE_PATH)
        module_list = []
        for path_to_mod_info in mod_info.path_to_module_info[PATH_TO_MULT_MODULES_WITH_MULTI_ARCH]:
            module_list.append(path_to_mod_info.get(constants.MODULE_NAME))
        module_list.sort()
        TESTABLE_MODULES_WITH_SHARED_PATH.sort()
        self.assertEqual(module_list, TESTABLE_MODULES_WITH_SHARED_PATH)

    def test_is_suite_in_compatibility_suites(self):
        """Test is_suite_in_compatibility_suites."""
        mod_info = module_info.ModuleInfo(module_file=JSON_FILE_PATH)
        info = {'compatibility_suites': []}
        self.assertFalse(mod_info.is_suite_in_compatibility_suites("cts", info))
        info2 = {'compatibility_suites': ["cts"]}
        self.assertTrue(mod_info.is_suite_in_compatibility_suites("cts", info2))
        self.assertFalse(mod_info.is_suite_in_compatibility_suites("vts10", info2))
        info3 = {'compatibility_suites': ["cts", "vts10"]}
        self.assertTrue(mod_info.is_suite_in_compatibility_suites("cts", info3))
        self.assertTrue(mod_info.is_suite_in_compatibility_suites("vts10", info3))
        self.assertFalse(mod_info.is_suite_in_compatibility_suites("ats", info3))

    @mock.patch.object(module_info.ModuleInfo, 'is_testable_module')
    @mock.patch.object(module_info.ModuleInfo, 'is_suite_in_compatibility_suites')
    def test_get_testable_modules(self, mock_is_suite_exist, mock_is_testable):
        """Test get_testable_modules."""
        mod_info = module_info.ModuleInfo(module_file=JSON_FILE_PATH)
        mock_is_testable.return_value = False
        self.assertEqual(mod_info.get_testable_modules(), set())
        mod_info.name_to_module_info = NAME_TO_MODULE_INFO
        mock_is_testable.return_value = True
        mock_is_suite_exist.return_value = True
        self.assertEqual(1, len(mod_info.get_testable_modules('test_suite')))
        mock_is_suite_exist.return_value = False
        self.assertEqual(0, len(mod_info.get_testable_modules('test_suite')))
        self.assertEqual(1, len(mod_info.get_testable_modules()))

    @mock.patch.object(module_info.ModuleInfo, 'has_test_config')
    @mock.patch.object(module_info.ModuleInfo, 'is_robolectric_test')
    def test_is_testable_module(self, mock_is_robo_test, mock_has_test_config):
        """Test is_testable_module."""
        mod_info = module_info.ModuleInfo(module_file=JSON_FILE_PATH)
        mock_is_robo_test.return_value = False
        mock_has_test_config.return_value = True
        installed_module_info = {constants.MODULE_INSTALLED:
                                 uc.DEFAULT_INSTALL_PATH}
        non_installed_module_info = {constants.MODULE_NAME: 'rand_name'}
        # Empty mod_info or a non-installed module.
        self.assertFalse(mod_info.is_testable_module(non_installed_module_info))
        self.assertFalse(mod_info.is_testable_module({}))
        # Testable Module or is a robo module for non-installed module.
        self.assertTrue(mod_info.is_testable_module(installed_module_info))
        mock_has_test_config.return_value = False
        self.assertFalse(mod_info.is_testable_module(installed_module_info))
        mock_is_robo_test.return_value = True
        self.assertTrue(mod_info.is_testable_module(non_installed_module_info))

    @mock.patch.dict('os.environ', {constants.ANDROID_BUILD_TOP:'/'})
    @mock.patch.object(module_info.ModuleInfo, 'is_auto_gen_test_config')
    def test_has_test_config(self, mock_is_auto_gen):
        """Test has_test_config."""
        mod_info = module_info.ModuleInfo(module_file=JSON_FILE_PATH)
        info = {constants.MODULE_PATH:[uc.TEST_DATA_DIR]}
        mock_is_auto_gen.return_value = True
        # Validate we see the config when it's auto-generated.
        self.assertTrue(mod_info.has_test_config(info))
        self.assertTrue(mod_info.has_test_config({}))
        # Validate when actual config exists and there's no auto-generated config.
        mock_is_auto_gen.return_value = False
        self.assertTrue(mod_info.has_test_config(info))
        self.assertFalse(mod_info.has_test_config({}))
        # Validate the case mod_info MODULE_TEST_CONFIG be set
        info2 = {constants.MODULE_PATH:[uc.TEST_CONFIG_DATA_DIR],
                 constants.MODULE_TEST_CONFIG:[os.path.join(uc.TEST_CONFIG_DATA_DIR, "a.xml")]}
        self.assertTrue(mod_info.has_test_config(info2))

    @mock.patch.object(module_info.ModuleInfo, 'get_module_names')
    def test_get_robolectric_test_name(self, mock_get_module_names):
        """Test get_robolectric_test_name."""
        # Happy path testing, make sure we get the run robo target.
        mod_info = module_info.ModuleInfo(module_file=JSON_FILE_PATH)
        mod_info.name_to_module_info = MOD_NAME_INFO_DICT
        mod_info.path_to_module_info = MOD_PATH_INFO_DICT
        mock_get_module_names.return_value = [RUN_ROBO_MOD_NAME, NON_RUN_ROBO_MOD_NAME]
        self.assertEqual(mod_info.get_robolectric_test_name(
            NON_RUN_ROBO_MOD_NAME), RUN_ROBO_MOD_NAME)
        # Let's also make sure we don't return anything when we're not supposed
        # to.
        mock_get_module_names.return_value = [NON_RUN_ROBO_MOD_NAME]
        self.assertEqual(mod_info.get_robolectric_test_name(
            NON_RUN_ROBO_MOD_NAME), None)

    @mock.patch.object(module_info.ModuleInfo, 'get_module_info')
    @mock.patch.object(module_info.ModuleInfo, 'get_module_names')
    def test_is_robolectric_test(self, mock_get_module_names, mock_get_module_info):
        """Test is_robolectric_test."""
        # Happy path testing, make sure we get the run robo target.
        mod_info = module_info.ModuleInfo(module_file=JSON_FILE_PATH)
        mod_info.name_to_module_info = MOD_NAME_INFO_DICT
        mod_info.path_to_module_info = MOD_PATH_INFO_DICT
        mock_get_module_names.return_value = [RUN_ROBO_MOD_NAME, NON_RUN_ROBO_MOD_NAME]
        mock_get_module_info.return_value = RUN_ROBO_MOD
        # Test on a run robo module.
        self.assertTrue(mod_info.is_robolectric_test(RUN_ROBO_MOD_NAME))
        # Test on a non-run robo module but shares with a run robo module.
        self.assertTrue(mod_info.is_robolectric_test(NON_RUN_ROBO_MOD_NAME))
        # Make sure we don't find robo tests where they don't exist.
        mock_get_module_info.return_value = None
        self.assertFalse(mod_info.is_robolectric_test('rand_mod'))

    @mock.patch.object(module_info.ModuleInfo, 'is_module')
    def test_is_auto_gen_test_config(self, mock_is_module):
        """Test is_auto_gen_test_config correctly detects the module."""
        mod_info = module_info.ModuleInfo(module_file=JSON_FILE_PATH)
        mock_is_module.return_value = True
        is_auto_test_config = {'auto_test_config': [True]}
        is_not_auto_test_config = {'auto_test_config': [False]}
        is_not_auto_test_config_again = {'auto_test_config': []}
        MOD_INFO_DICT[MOD_NAME1] = is_auto_test_config
        MOD_INFO_DICT[MOD_NAME2] = is_not_auto_test_config
        MOD_INFO_DICT[MOD_NAME3] = is_not_auto_test_config_again
        MOD_INFO_DICT[MOD_NAME4] = {}
        mod_info.name_to_module_info = MOD_INFO_DICT
        self.assertTrue(mod_info.is_auto_gen_test_config(MOD_NAME1))
        self.assertFalse(mod_info.is_auto_gen_test_config(MOD_NAME2))
        self.assertFalse(mod_info.is_auto_gen_test_config(MOD_NAME3))
        self.assertFalse(mod_info.is_auto_gen_test_config(MOD_NAME4))

    def test_is_robolectric_module(self):
        """Test is_robolectric_module correctly detects the module."""
        mod_info = module_info.ModuleInfo(module_file=JSON_FILE_PATH)
        is_robolectric_module = {'class': ['ROBOLECTRIC']}
        is_not_robolectric_module = {'class': ['OTHERS']}
        MOD_INFO_DICT[MOD_NAME1] = is_robolectric_module
        MOD_INFO_DICT[MOD_NAME2] = is_not_robolectric_module
        mod_info.name_to_module_info = MOD_INFO_DICT
        self.assertTrue(mod_info.is_robolectric_module(MOD_INFO_DICT[MOD_NAME1]))
        self.assertFalse(mod_info.is_robolectric_module(MOD_INFO_DICT[MOD_NAME2]))


if __name__ == '__main__':
    unittest.main()
