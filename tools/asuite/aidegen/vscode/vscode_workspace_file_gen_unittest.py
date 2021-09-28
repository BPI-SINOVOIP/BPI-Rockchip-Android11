#!/usr/bin/env python3
#
# Copyright 2020 - The Android Open Source Project
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


"""Unittests for ide_util."""

import os
import unittest
from unittest import mock

from aidegen.lib import common_util
from aidegen.vscode import vscode_workspace_file_gen


# pylint: disable=protected-access
class WorkspaceProjectFileGenUnittests(unittest.TestCase):
    """Unit tests for ide_util.py."""

    @mock.patch.object(vscode_workspace_file_gen, '_get_unique_project_name')
    @mock.patch.object(
        vscode_workspace_file_gen, '_create_code_workspace_file_content')
    @mock.patch.object(common_util, 'get_android_root_dir')
    def test_vdcode_apply_optional_config(
            self, mock_get_root, mock_ws_content, mock_get_unique):
        """Test IdeVSCode's apply_optional_config method."""
        mock_get_root.return_value = 'a/b'
        vscode_workspace_file_gen.generate_code_workspace_file(
            ['a/b/path_to_project1', 'a/b/path_to_project2'])
        self.assertTrue(mock_get_root.called)
        self.assertTrue(mock_ws_content.called)
        self.assertTrue(mock_get_unique.called)

    @mock.patch.object(common_util, 'dump_json_dict')
    def test_create_code_workspace_file_content(self, mock_dump):
        """Test _create_code_workspace_file_content function."""
        abs_path = 'a/b'
        folder_name = 'Settings'
        res = vscode_workspace_file_gen._create_code_workspace_file_content(
            {'folders': [{'name': folder_name, 'path': abs_path}]})
        self.assertTrue(mock_dump.called)
        file_name = ''.join(
            [folder_name, vscode_workspace_file_gen._VSCODE_WORKSPACE_EXT])
        file_path = os.path.join(abs_path, file_name)
        self.assertEqual(res, file_path)

    def test_get_unique_project_name(self):
        """Test _get_unique_project_name function with conditions."""
        abs_path = 'a/b/to/project'
        root_dir = 'a/b'
        expected = 'to.project'
        result = vscode_workspace_file_gen._get_unique_project_name(
            abs_path, root_dir)
        self.assertEqual(result, expected)
        abs_path = 'a/b'
        expected = 'b'
        result = vscode_workspace_file_gen._get_unique_project_name(
            abs_path, root_dir)
        self.assertEqual(result, expected)


if __name__ == '__main__':
    unittest.main()
