#!/usr/bin/env python3
#
# Copyright 2020, The Android Open Source Project
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

"""Unittests for native_util."""

import unittest
from unittest import mock

from aidegen.lib import native_module_info
from aidegen.lib import native_project_info
from aidegen.lib import project_config
from aidegen.lib import project_info


# pylint: disable=protected-access
class NativeProjectInfoUnittests(unittest.TestCase):
    """Unit tests for native_project_info.py"""

    @mock.patch.object(native_module_info, 'NativeModuleInfo')
    def test_init_modules_info(self, mock_mod_info):
        """Test initializing the class attribute: modules_info."""
        native_project_info.NativeProjectInfo.modules_info = None
        native_project_info.NativeProjectInfo._init_modules_info()
        self.assertEqual(mock_mod_info.call_count, 1)
        native_project_info.NativeProjectInfo._init_modules_info()
        self.assertEqual(mock_mod_info.call_count, 1)

    @mock.patch.object(project_info, 'batch_build_dependencies')
    @mock.patch.object(native_project_info.NativeProjectInfo,
                       '_get_need_builds')
    @mock.patch.object(native_project_info.NativeProjectInfo,
                       '_init_modules_info')
    @mock.patch.object(project_config.ProjectConfig, 'get_instance')
    def test_generate_projects(self, mock_get_inst, mock_mod_info,
                               mock_get_need, mock_batch):
        """Test initializing NativeProjectInfo woth different conditions."""
        target = 'libui'
        config = mock.Mock()
        mock_get_inst.return_value = config
        config.is_skip_build = True
        native_project_info.NativeProjectInfo.generate_projects([target])
        self.assertFalse(mock_mod_info.called)

        mock_mod_info.reset_mock()
        config.is_skip_build = False
        mock_get_need.return_value = ['mod1', 'mod2']
        native_project_info.NativeProjectInfo.generate_projects([target])
        self.assertTrue(mock_mod_info.called)
        self.assertTrue(mock_get_need.called)
        self.assertTrue(mock_batch.called)

    def test_get_need_builds_without_needed_build(self):
        """Test _get_need_builds method without needed build."""
        targets = ['mod1', 'mod2']
        nativeInfo = native_project_info.NativeProjectInfo
        nativeInfo.modules_info = mock.Mock()
        nativeInfo.modules_info.is_module.return_value = [True, True]
        nativeInfo.modules_info.is_module_need_build.return_value = [True, True]
        self.assertEqual(
            set(targets),
            native_project_info.NativeProjectInfo._get_need_builds(targets))


if __name__ == '__main__':
    unittest.main()
