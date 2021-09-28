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

"""Unittests for vscode_native_project_file_gen."""

import os
import unittest
from unittest import mock

from aidegen.vscode import vscode_native_project_file_gen

_NATIVE_INCLUDES1 = [
    'out/soong/.intermediates/android.frameworks.bufhub@1.0_genc++_headers/gen'
]
_NATIVE_INCLUDES2 = [
    'frameworks/native/include',
    'frameworks/native/libs/ui',
]


# pylint: disable=protected-access
class VSCodeNativeProjectFileGenUnittests(unittest.TestCase):
    """Unit tests for vscode_native_project_file_gen.py"""

    @mock.patch.object(os, 'mkdir')
    @mock.patch.object(os.path, 'isdir')
    def test_init(self, mock_isdir, mock_mkdir):
        """Test initializing VSCodeNativeProjectFileGenerator."""
        mod_dir = 'a/b/packages/apps/Settings'
        mock_isdir.return_value = True
        vscode_native_project_file_gen.VSCodeNativeProjectFileGenerator(mod_dir)
        self.assertFalse(mock_mkdir.called)
        mock_mkdir.mock_reset()
        mock_isdir.return_value = False
        vscode_native_project_file_gen.VSCodeNativeProjectFileGenerator(mod_dir)
        self.assertTrue(mock_mkdir.called)

    @mock.patch.object(os.path, 'isdir')
    @mock.patch.object(os.path, 'isfile')
    def test_create_c_cpp_properties_dict(self, mock_isfile, mock_isdir):
        """Test _create_c_cpp_properties_dict with conditions."""
        mock_isfile.return_value = True
        mock_isdir.return_value = True
        includes = set(_NATIVE_INCLUDES1)
        includes.update(set(_NATIVE_INCLUDES2))
        mod_dir = 'a/b/shared/path/to/be/used2/multiarch'
        ccgen = vscode_native_project_file_gen.VSCodeNativeProjectFileGenerator(
            mod_dir)
        cc_mod_info = mock.Mock()
        cc_mod_info.get_module_includes.return_value = includes
        data = ccgen._create_c_cpp_properties_dict(cc_mod_info, ['multiarch'])
        config = vscode_native_project_file_gen._CONFIG
        include = vscode_native_project_file_gen._INC_PATH
        compp = vscode_native_project_file_gen._COMPILE_PATH
        compiler = vscode_native_project_file_gen._COMPILER_PATH
        res = vscode_native_project_file_gen._make_header_file_paths(includes)
        self.assertEqual(set(res), set(data[config][0][include]))
        self.assertEqual(compiler, data[config][0][compp])

        mock_isfile.return_value = False
        data = ccgen._create_c_cpp_properties_dict(cc_mod_info, ['multiarch'])
        self.assertEqual(set(res), set(data[config][0][include]))
        self.assertEqual('', data[config][0][compp])


if __name__ == '__main__':
    unittest.main()
