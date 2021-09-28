#!/usr/bin/env python3
#
# Copyright 2019, The Android Open Source Project
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

"""Unittests for AidegenConfig class."""

import os
import shutil
import tempfile
import unittest
from unittest import mock

from aidegen import constant

from aidegen.lib import common_util
from aidegen.lib import config


# pylint: disable=protected-access
class AidegenConfigUnittests(unittest.TestCase):
    """Unit tests for config.py"""

    _TMP_DIR = None

    def setUp(self):
        """Prepare the testdata related path."""
        AidegenConfigUnittests._TMP_DIR = tempfile.mkdtemp()
        config.AidegenConfig._CONFIG_DIR = os.path.join(
            AidegenConfigUnittests._TMP_DIR, '.config', 'asuite', 'aidegen')
        config.AidegenConfig._CONFIG_FILE_PATH = os.path.join(
            config.AidegenConfig._CONFIG_DIR,
            config.AidegenConfig._DEFAULT_CONFIG_FILE)
        config.AidegenConfig._ENABLE_DEBUG_DIR = os.path.join(
            config.AidegenConfig._CONFIG_DIR,
            config.AidegenConfig._ENABLE_DEBUG_CONFIG_DIR)
        config.AidegenConfig.DEBUG_ENABLED_FILE_PATH = os.path.join(
            config.AidegenConfig._CONFIG_DIR,
            config.AidegenConfig._ENABLE_DEBUG_CONFIG_FILE)

    def tearDown(self):
        """Clear the testdata related path."""
        shutil.rmtree(AidegenConfigUnittests._TMP_DIR)

    @mock.patch('json.load')
    @mock.patch('builtins.open')
    @mock.patch('os.path.exists')
    def test_load_aidegen_config(self, mock_file_exists, mock_file_open,
                                 mock_json_load):
        """Test loading aidegen config."""
        mock_file_exists.return_value = False
        cfg = config.AidegenConfig()
        cfg._load_aidegen_config()
        self.assertFalse(mock_file_open.called)
        self.assertFalse(mock_json_load.called)
        mock_file_exists.return_value = True
        cfg = config.AidegenConfig()
        cfg._load_aidegen_config()
        self.assertTrue(mock_file_open.called)
        self.assertTrue(mock_json_load.called)

    @mock.patch('logging.info')
    @mock.patch('logging.error')
    @mock.patch('builtins.open')
    @mock.patch('os.path.exists')
    def test_error_load_aidegen_config(self, mock_file_exists, mock_file_open,
                                       mock_error, mock_info):
        """Test loading aidegen config with errors."""
        mock_file_exists.return_value = True
        cfg = config.AidegenConfig()
        mock_file_open.side_effect = IOError()
        with self.assertRaises(IOError):
            cfg._load_aidegen_config()
            self.assertTrue(mock_error.called)
            self.assertFalse(mock_info.called)
        mock_file_open.reset()
        mock_file_open.side_effect = ValueError()
        cfg._load_aidegen_config()
        self.assertTrue(mock_info.called)

    @mock.patch('json.dump')
    @mock.patch('builtins.open')
    @mock.patch.object(config.AidegenConfig, '_is_config_modified')
    def test_aidegen_config_no_changed(self, mock_is_config_modified,
                                       mock_file_open, mock_json_dump):
        """Skip saving aidegen config when no configuration data is modified."""
        mock_is_config_modified.return_value = False
        cfg = config.AidegenConfig()
        cfg._save_aidegen_config()
        self.assertFalse(mock_file_open.called)
        self.assertFalse(mock_json_dump.called)

    @mock.patch('json.dump')
    @mock.patch('builtins.open')
    @mock.patch.object(config.AidegenConfig, '_is_config_modified')
    def test_update_aidegen_config(self, mock_is_config_modified,
                                   mock_file_open, mock_json_dump):
        """Save the aidegen config once any configuration data is modified."""
        mock_is_config_modified.return_value = True
        cfg = config.AidegenConfig()
        cfg._save_aidegen_config()
        self.assertTrue(mock_file_open.called)
        self.assertTrue(mock_json_dump.called)

    @mock.patch('logging.warning')
    @mock.patch.object(config.AidegenConfig, '_gen_enable_debugger_config')
    @mock.patch.object(config.AidegenConfig, '_gen_androidmanifest')
    @mock.patch.object(config.AidegenConfig, '_gen_enable_debug_sub_dir')
    def test_create_enable_debugger(self, mock_debug, mock_androidmanifest,
                                    mock_enable, mock_warning):
        """Test create_enable_debugger_module."""
        cfg = config.AidegenConfig()
        self.assertTrue(cfg.create_enable_debugger_module(''))
        mock_debug.side_effect = IOError()
        self.assertFalse(cfg.create_enable_debugger_module(''))
        self.assertTrue(mock_warning.called)
        mock_debug.side_effect = OSError()
        self.assertFalse(cfg.create_enable_debugger_module(''))
        self.assertTrue(mock_warning.called)
        mock_androidmanifest.side_effect = IOError()
        self.assertFalse(cfg.create_enable_debugger_module(''))
        self.assertTrue(mock_warning.called)
        mock_androidmanifest.side_effect = OSError()
        self.assertFalse(cfg.create_enable_debugger_module(''))
        self.assertTrue(mock_warning.called)
        mock_enable.side_effect = IOError()
        self.assertFalse(cfg.create_enable_debugger_module(''))
        self.assertTrue(mock_warning.called)
        mock_enable.side_effect = OSError()
        self.assertFalse(cfg.create_enable_debugger_module(''))
        self.assertTrue(mock_warning.called)

    @mock.patch.object(common_util, 'file_generate')
    @mock.patch('os.path.exists')
    def test_gen_debugger_config(self, mock_file_exists, mock_file_generate):
        """Test generating the enable debugger config."""
        cfg = config.AidegenConfig()
        android_sdk_version = ''
        mock_file_exists.return_value = False
        cfg._gen_enable_debugger_config(android_sdk_version)
        self.assertTrue(mock_file_generate.called)

    @mock.patch('os.stat')
    @mock.patch.object(common_util, 'file_generate')
    @mock.patch('os.path.exists')
    def test_androidmanifest_no_changed(self, mock_file_exists,
                                        mock_file_generate, mock_file_stat):
        """No generate the AndroidManifest.xml when it exists and size > 0."""
        cfg = config.AidegenConfig()
        mock_file_exists.return_value = True
        mock_file_stat.return_value.st_size = 1
        cfg._gen_androidmanifest()
        self.assertFalse(mock_file_generate.called)

    @mock.patch('os.stat')
    @mock.patch.object(common_util, 'file_generate')
    @mock.patch('os.path.exists')
    def test_override_androidmanifest(self, mock_file_exists,
                                      mock_file_generate, mock_file_stat):
        """Override the AndroidManifest.xml when the file size is zero."""
        cfg = config.AidegenConfig()
        mock_file_exists.return_value = True
        mock_file_stat.return_value.st_size = 0
        cfg._gen_androidmanifest()
        self.assertTrue(mock_file_generate.called)

    @mock.patch.object(common_util, 'file_generate')
    @mock.patch('os.path.exists')
    def test_gen_androidmanifest(self, mock_file_exists, mock_file_generate):
        """Generate the AndroidManifest.xml when it doesn't exist."""
        cfg = config.AidegenConfig()
        mock_file_exists.return_value = False
        cfg._gen_androidmanifest()
        self.assertTrue(mock_file_generate.called)

    @mock.patch('os.makedirs')
    @mock.patch('os.path.exists')
    def test_config_folder_exists(self, mock_folder_exists, mock_makedirs):
        """Skipping create the config folder once it exists."""
        mock_folder_exists.return_value = True
        config.AidegenConfig()
        self.assertFalse(mock_makedirs.called)

    @mock.patch('os.makedirs')
    @mock.patch('os.path.exists')
    def test_create_config_folder(self, mock_folder_exists, mock_makedirs):
        """Create the config folder when it doesn't exist."""
        mock_folder_exists.return_value = False
        config.AidegenConfig()
        self.assertTrue(mock_makedirs.called)

    @mock.patch('os.path.isfile')
    @mock.patch('builtins.open', create=True)
    def test_deprecated_intellij_version(self, mock_open, mock_isfile):
        """Test deprecated_intellij_version."""
        # Test the idea.sh file contains the deprecated string.
        cfg = config.AidegenConfig()
        expacted_data = ('#!/bin/sh\n\n'
                         'SUMMARY="This version of IntelliJ Community Edition '
                         'is no longer supported."\n')
        mock_open.side_effect = [
            mock.mock_open(read_data=expacted_data).return_value
        ]
        mock_isfile.return_value = True
        self.assertTrue(cfg.deprecated_intellij_version(0))

        # Test the idea.sh file doesn't contains the deprecated string.
        expacted_data = ('#!/bin/sh\n\n'
                         'JAVA_BIN="$JDK/bin/java"\n'
                         '"$JAVA_BIN" \\n')
        mock_open.side_effect = [
            mock.mock_open(read_data=expacted_data).return_value
        ]
        self.assertFalse(cfg.deprecated_intellij_version(0))

    @mock.patch.object(config.AidegenConfig, 'deprecated_studio_version')
    @mock.patch.object(config.AidegenConfig, 'deprecated_intellij_version')
    def test_deprecated_version(self, mock_inj, mock_studio):
        """Test deprecated_version."""
        cfg = config.AidegenConfig()
        ide_name = constant.IDE_INTELLIJ
        test_path = ''
        cfg.deprecated_version(ide_name, test_path)
        self.assertTrue(mock_inj.called)

        ide_name = constant.IDE_ANDROID_STUDIO
        cfg.deprecated_version(ide_name, test_path)
        self.assertTrue(mock_studio.called)
        test_ide = ''
        self.assertFalse(cfg.deprecated_version(test_ide, test_path))

    @mock.patch.object(os.path, 'isfile')
    def test_deprecated_studio_version(self, mock_is_file):
        """Test deprecated_studio_version."""
        test_sh_name = 'test.sh'
        # temp_dir/test
        temp_base = os.path.join(self._TMP_DIR, 'test')
        os.mkdir(temp_base)
        # temp_dir/test/studio/bin
        os.mkdir(os.path.join(temp_base, 'studio'))
        test_bin_path = os.path.join(self._TMP_DIR, 'test', 'studio', 'bin')
        os.mkdir(test_bin_path)
        # /temp_dir/test/studio/bin/test.sh
        test_sh_path = os.path.join(self._TMP_DIR, 'test', 'studio', 'bin',
                                    test_sh_name)
        # Real test.sh doesn't exist.
        cfg = config.AidegenConfig()
        self.assertTrue(cfg.deprecated_studio_version(test_sh_path))
        # The /temp_dir/test/studio/lib doesn't exist case.
        mock_is_file.return_value = True
        self.assertTrue(cfg.deprecated_studio_version(test_sh_path))
        # The /temp_dir/test/studio/lib exists.
        test_lib_path = os.path.join(self._TMP_DIR, 'test', 'studio', 'lib')
        os.mkdir(test_lib_path)
        self.assertFalse(cfg.deprecated_studio_version(test_sh_path))
        shutil.rmtree(temp_base)

    @mock.patch('os.path.isfile')
    def test_idea_path_not_file(self, mock_isfile):
        """Test deprecated_intellij_version."""
        # Test the idea_path is not a file.
        cfg = config.AidegenConfig()
        mock_isfile.return_value = False
        self.assertFalse(cfg.deprecated_intellij_version(0))

    @mock.patch.object(config.AidegenConfig, 'deprecated_version')
    @mock.patch.object(config.AidegenConfig, 'deprecated_intellij_version')
    def test_preferred_version(self, mock_deprecated_intj, mock_deprecated):
        """Test get preferred IntelliJ version."""
        cfg = config.AidegenConfig()
        cfg._config['preferred_version'] = ''
        self.assertEqual(cfg.preferred_version(), None)

        result = 'test_intellij'
        cfg._config['IntelliJ_preferred_version'] = result
        mock_deprecated.return_value = False
        self.assertEqual(cfg.preferred_version(constant.IDE_INTELLIJ), result)
        self.assertEqual(cfg.preferred_version(result), None)
        mock_deprecated.return_value = True
        self.assertEqual(cfg.preferred_version(constant.IDE_INTELLIJ), None)

        mock_deprecated_intj.return_value = False
        cfg._config['preferred_version'] = 'a'
        self.assertEqual(cfg.preferred_version(), 'a')
        mock_deprecated_intj.return_value = True
        self.assertEqual(cfg.preferred_version(), None)

    def test_set_preferred_version(self):
        """Test set_preferred_version."""
        cfg = config.AidegenConfig()
        cfg._config[config.AidegenConfig._KEY_APPEND] = 'Yes'
        cfg.set_preferred_version('test', None)
        self.assertEqual(cfg._config[config.AidegenConfig._KEY_APPEND], 'test')
        cfg.set_preferred_version('test', constant.IDE_INTELLIJ)
        self.assertEqual(cfg._config['IntelliJ_preferred_version'], 'test')

    @mock.patch('os.makedirs')
    @mock.patch('os.path.exists')
    def test_gen_enable_debug_sub_dir(self, mock_file_exists, mock_makedirs):
        """Test _gen_enable_debug_sub_dir."""
        cfg = config.AidegenConfig()
        mock_file_exists.return_value = True
        cfg._gen_enable_debug_sub_dir('a')
        self.assertFalse(mock_makedirs.called)
        mock_file_exists.return_value = False
        cfg._gen_enable_debug_sub_dir('a')
        self.assertTrue(mock_makedirs.called)


class IdeaPropertiesUnittests(unittest.TestCase):
    """Unit tests for IdeaProperties class."""

    _CONFIG_DIR = None

    def setUp(self):
        """Prepare the testdata related path."""
        IdeaPropertiesUnittests._CONFIG_DIR = tempfile.mkdtemp()

    def tearDown(self):
        """Clear the testdata related path."""
        shutil.rmtree(IdeaPropertiesUnittests._CONFIG_DIR)

    def test_set_default_properties(self):
        """Test creating the idea.properties with default content."""
        cfg = config.IdeaProperties(IdeaPropertiesUnittests._CONFIG_DIR)
        cfg._set_default_idea_properties()
        expected_data = cfg._PROPERTIES_CONTENT.format(
            KEY_FILE_SIZE=cfg._KEY_FILESIZE,
            VALUE_FILE_SIZE=cfg._FILESIZE_LIMIT)
        generated_file = os.path.join(IdeaPropertiesUnittests._CONFIG_DIR,
                                      cfg._PROPERTIES_FILE)
        generated_content = common_util.read_file_content(generated_file)
        self.assertEqual(expected_data, generated_content)

    @mock.patch.object(common_util, 'read_file_content')
    def test_reset_max_file_size(self, mock_content):
        """Test reset the file size limit when it's smaller than 100000."""
        mock_content.return_value = ('# custom IntelliJ IDEA properties\n'
                                     'idea.max.intellisense.filesize=5000')
        expected_data = ('# custom IntelliJ IDEA properties\n'
                         'idea.max.intellisense.filesize=100000')
        cfg = config.IdeaProperties(IdeaPropertiesUnittests._CONFIG_DIR)
        cfg._reset_max_file_size()
        generated_file = os.path.join(IdeaPropertiesUnittests._CONFIG_DIR,
                                      cfg._PROPERTIES_FILE)
        with open(generated_file) as properties_file:
            generated_content = properties_file.read()
        self.assertEqual(expected_data, generated_content)

    @mock.patch.object(common_util, 'file_generate')
    @mock.patch.object(common_util, 'read_file_content')
    def test_no_reset_max_file_size(self, mock_content, mock_gen_file):
        """Test when the file size is larger than 100000."""
        mock_content.return_value = ('# custom IntelliJ IDEA properties\n'
                                     'idea.max.intellisense.filesize=110000')
        cfg = config.IdeaProperties(IdeaPropertiesUnittests._CONFIG_DIR)
        cfg._reset_max_file_size()
        self.assertFalse(mock_gen_file.called)

    @mock.patch.object(config.IdeaProperties, '_reset_max_file_size')
    @mock.patch.object(config.IdeaProperties, '_set_default_idea_properties')
    @mock.patch('os.path.exists')
    def test_set_idea_properties_called(self, mock_file_exists,
                                        mock_set_default,
                                        mock_reset_file_size):
        """Test _set_default_idea_properties() method is called."""
        mock_file_exists.return_value = False
        cfg = config.IdeaProperties(IdeaPropertiesUnittests._CONFIG_DIR)
        cfg.set_max_file_size()
        self.assertTrue(mock_set_default.called)
        self.assertFalse(mock_reset_file_size.called)

    @mock.patch.object(config.IdeaProperties, '_reset_max_file_size')
    @mock.patch.object(config.IdeaProperties, '_set_default_idea_properties')
    @mock.patch('os.path.exists')
    def test_reset_properties_called(self, mock_file_exists, mock_set_default,
                                     mock_reset_file_size):
        """Test _reset_max_file_size() method is called."""
        mock_file_exists.return_value = True
        cfg = config.IdeaProperties(IdeaPropertiesUnittests._CONFIG_DIR)
        cfg.set_max_file_size()
        self.assertFalse(mock_set_default.called)
        self.assertTrue(mock_reset_file_size.called)


if __name__ == '__main__':
    unittest.main()
