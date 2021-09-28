# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the `stable_version` module and CLI."""

import mock
import unittest

import common
from autotest_lib.server import frontend
from autotest_lib.site_utils.stable_images import stable_version


class ParseArgsTestCase(unittest.TestCase):
    """Unit tests for `_parse_args()`."""

    def test_default_options(self):
        """Test for an empty command line."""
        arguments = stable_version._parse_args(['command'])
        self.assertFalse(arguments.dry_run)
        self.assertIsNone(arguments.type)
        self.assertIsNone(arguments.web)
        self.assertFalse(arguments.delete)
        self.assertIsNone(arguments.key)
        self.assertIsNone(arguments.version)

    def test_web_option(self):
        """Test for the `--web` option."""
        for option in ['-w', '--web']:
            argv = ['command', option, 'server']
            arguments = stable_version._parse_args(argv)
            self.assertEqual(arguments.web, argv[2])

    def test_dry_run_option(self):
        """Test for the `--dry-run` option."""
        for option in ['-n', '--dry-run']:
            argv = ['command', option]
            arguments = stable_version._parse_args(argv)
            self.assertTrue(arguments.dry_run)

    def test_image_type_option(self):
        """Test for the `--type` option."""
        for image_type in stable_version._ALL_IMAGE_TYPES:
            for option in ['-t', '--type']:
                argv = ['command', option, image_type]
                arguments = stable_version._parse_args(argv)

    def test_delete_option(self):
        """Test for the `--delete` option."""
        for option in ['-d', '--delete']:
            argv = ['command', option]
            arguments = stable_version._parse_args(argv)
            self.assertTrue(arguments.delete)

    def test_key_argument(self):
        """Test for the BOARD_OR_MODEL argument."""
        argv = ['command', 'key']
        arguments = stable_version._parse_args(argv)
        self.assertEqual(arguments.key, argv[1])

    def test_version_argument(self):
        """Test for the VERSION argument."""
        argv = ['command', 'key', 'version']
        arguments = stable_version._parse_args(argv)
        self.assertEqual(arguments.key, argv[1])
        self.assertEqual(arguments.version, argv[2])


class ProcessCommandTestCase(unittest.TestCase):
    """Unit tests for `_dispatch_command()`."""

    def _dispatch_command_success(self, afe, argv, name_called):
        arguments = stable_version._parse_args(argv)
        patches = mock.patch.multiple(stable_version,
                                      list_all_mappings=mock.DEFAULT,
                                      list_mapping_by_key=mock.DEFAULT,
                                      set_mapping=mock.DEFAULT,
                                      delete_mapping=mock.DEFAULT)
        with patches as mocks:
            stable_version._dispatch_command(afe, arguments)
        for not_called in set(mocks) - set([name_called]):
            mocks[not_called].assert_not_called()
        return mocks[name_called]

    def _assert_command_error(self, argv):
        afe = object()
        arguments = stable_version._parse_args(argv)
        with self.assertRaises(stable_version._CommandError):
            stable_version._dispatch_command(afe, arguments)

    def test_list_all(self):
        """Test that `list_all_mappings` is called when required."""
        argv = ['command']
        afe = object()
        called_mock = self._dispatch_command_success(
                afe, argv, 'list_all_mappings')
        called_mock.assert_called_once_with(afe, None)

    def test_list_all_with_type(self):
        """Test that `list_all_mappings` is called with a supplied type."""
        argv = ['command', '-t', frontend.AFE.CROS_IMAGE_TYPE]
        afe = object()
        called_mock = self._dispatch_command_success(
                afe, argv, 'list_all_mappings')
        called_mock.assert_called_once_with(afe, argv[2])

    def test_list_mapping_by_key(self):
        """Test that `list_mapping_by_key` is called when required."""
        argv = ['command', 'board']
        afe = object()
        called_mock = self._dispatch_command_success(
                afe, argv, 'list_mapping_by_key')
        called_mock.assert_called_once_with(afe, None, argv[1])

    def test_list_mapping_by_key_with_type(self):
        """Test that `list_mapping_by_key` is called with a supplied type."""
        argv = ['command', '-t', frontend.AFE.CROS_IMAGE_TYPE, 'board']
        afe = object()
        called_mock = self._dispatch_command_success(
                afe, argv, 'list_mapping_by_key')
        called_mock.assert_called_once_with(afe, argv[2], argv[3])

    def test_set_mapping(self):
        """Test that `set_mapping` is called when required."""
        argv = ['command', '-t', frontend.AFE.CROS_IMAGE_TYPE, 'board', 'V0.0']
        afe = object()
        called_mock = self._dispatch_command_success(
                afe, argv, 'set_mapping')
        called_mock.assert_called_once_with(afe, argv[2], argv[3],
                                            argv[4], False)

    def test_set_mapping_firmware(self):
        """Test error when `set_mapping` is called for firmware."""
        argv = ['command', '-t', frontend.AFE.FIRMWARE_IMAGE_TYPE,
                'board', 'V0.0']
        self._assert_command_error(argv)

    def test_set_mapping_no_type(self):
        """Test error when `set_mapping` is called without a type."""
        argv = ['command', 'board', 'V0.0']
        self._assert_command_error(argv)

    def test_delete_mapping(self):
        """Test that `delete_mapping` is called when required."""
        argv = ['command', '-t', frontend.AFE.CROS_IMAGE_TYPE, '-d', 'board']
        afe = object()
        called_mock = self._dispatch_command_success(
                afe, argv, 'delete_mapping')
        called_mock.assert_called_once_with(afe, argv[2], argv[4], False)

    def test_delete_mapping_no_key(self):
        """Test error when `delete_mapping` is called without a key."""
        argv = ['command', '-t', frontend.AFE.CROS_IMAGE_TYPE, '-d']
        self._assert_command_error(argv)

    def test_delete_mapping_no_type(self):
        """Test error when `delete_mapping` is called without a type."""
        argv = ['command', '-d', 'board']
        self._assert_command_error(argv)

    def test_delete_mapping_with_version(self):
        """Test error when `delete_mapping` is called with extra arguments."""
        argv = ['command', '-t', frontend.AFE.CROS_IMAGE_TYPE, '-d',
                'board', 'V0.0']
        self._assert_command_error(argv)


if __name__ == '__main__':
    unittest.main()
