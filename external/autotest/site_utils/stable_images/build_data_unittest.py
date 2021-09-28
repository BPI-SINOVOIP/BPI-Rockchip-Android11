# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Unit tests for functions in `build_data`.
"""


import json
import mock
import os
import sys
import unittest

import common
from autotest_lib.site_utils.stable_images import build_data


# _OMAHA_TEST_DATA - File with JSON data to be used as test input to
#   `_make_omaha_versions()`.  In the file, the various items in the
#   `omaha_data` list are selected to capture various specific test
#   cases:
#     + Board with no "beta" channel.
#     + Board with "beta" and another channel.
#     + Board with only a "beta" channel.
#     + Board with no "chrome_version" entry.
#     + Obsolete board with "is_active" set to false.
# The JSON content of the file is a subset of an actual
# `omaha_status.json` file copied when the unit test was last
# updated.
#
# _EXPECTED_OMAHA_VERSIONS - The expected output produced by the
#   contents of _OMAHA_TEST_DATA.

_OMAHA_TEST_DATA = 'test_omaha_status.json'
_EXPECTED_OMAHA_VERSIONS = {
    'auron-paine': 'R55-8872.54.0',
    'gale': 'R55-8872.40.9',
    'kevin': 'R55-8872.64.0',
    'zako-freon': 'R41-6680.52.0'
}


class OmahaDataTests(unittest.TestCase):
    """Tests for the `get_omaha_version_map()` function."""

    @mock.patch.object(build_data, '_read_gs_json_data')
    def test_make_omaha_versions(self, mock_read_gs):
        """Test `get_omaha_version_map()` against one simple input.

        This is a trivial sanity test that asserts that a single
        hard-coded input returns a correct hard-coded output.

        @param mock_read_gs  Mock created for `_read_gs_json_data()`.
        """
        module_dir = os.path.dirname(sys.modules[__name__].__file__)
        data_file_path = os.path.join(module_dir, _OMAHA_TEST_DATA)
        mock_read_gs.return_value = json.load(open(data_file_path, 'r'))
        omaha_versions = build_data.get_omaha_version_map()
        self.assertEqual(omaha_versions, _EXPECTED_OMAHA_VERSIONS)


class KeyPathTests(unittest.TestCase):
    """Tests for the `_get_by_key_path()` function."""

    DICTDICT = {'level0': 'OK', 'level1_a': {'level1_b': 'OK'}}

    def _get_by_key_path(self, keypath):
        get_by_key_path = build_data._get_by_key_path
        return get_by_key_path(self.DICTDICT, keypath)

    def _check_path_valid(self, keypath):
        self.assertEqual(self._get_by_key_path(keypath), 'OK')

    def _check_path_invalid(self, keypath):
        self.assertIsNone(self._get_by_key_path(keypath))

    def test_one_element(self):
        """Test a single-element key path with a valid key."""
        self._check_path_valid(['level0'])

    def test_two_element(self):
        """Test a two-element key path with a valid key."""
        self._check_path_valid(['level1_a', 'level1_b'])

    def test_one_element_invalid(self):
        """Test a single-element key path with an invalid key."""
        self._check_path_invalid(['absent'])

    def test_two_element_invalid(self):
        """Test a two-element key path with an invalid key."""
        self._check_path_invalid(['level1_a', 'absent'])


class GetOmahaUpgradeTests(unittest.TestCase):
    """Tests for `get_omaha_upgrade()`."""

    V0 = 'R66-10452.27.0'
    V1 = 'R66-10452.30.0'
    V2 = 'R67-10494.0.0'

    def test_choose_cros_version(self):
        """Test that the CrOS version is chosen when it is later."""
        new_version = build_data.get_omaha_upgrade(
            {'board': self.V0}, 'board', self.V1)
        self.assertEquals(new_version, self.V1)

    def test_choose_omaha_version(self):
        """Test that the Omaha version is chosen when it is later."""
        new_version = build_data.get_omaha_upgrade(
            {'board': self.V1}, 'board', self.V0)
        self.assertEquals(new_version, self.V1)

    def test_branch_version_comparison(self):
        """Test that versions on different branches compare properly."""
        new_version = build_data.get_omaha_upgrade(
            {'board': self.V1}, 'board', self.V2)
        self.assertEquals(new_version, self.V2)

    def test_identical_versions(self):
        """Test handling when both the versions are the same."""
        new_version = build_data.get_omaha_upgrade(
            {'board': self.V1}, 'board', self.V1)
        self.assertEquals(new_version, self.V1)

    def test_board_name_mapping(self):
        """Test that AFE board names are mapped to Omaha board names."""
        board_equivalents = [
            ('a-b', 'a-b'), ('c_d', 'c-d'),
            ('e_f-g', 'e-f-g'), ('hi', 'hi')
        ]
        for afe_board, omaha_board in board_equivalents:
            new_version = build_data.get_omaha_upgrade(
                {omaha_board: self.V1}, afe_board, self.V0)
            self.assertEquals(new_version, self.V1)

    def test_no_omaha_version(self):
        """Test handling when there's no Omaha version."""
        new_version = build_data.get_omaha_upgrade(
            {}, 'board', self.V1)
        self.assertEquals(new_version, self.V1)

    def test_no_afe_version(self):
        """Test handling when there's no CrOS version."""
        new_version = build_data.get_omaha_upgrade(
            {'board': self.V1}, 'board', None)
        self.assertEquals(new_version, self.V1)

    def test_no_version_whatsoever(self):
        """Test handling when both versions are `None`."""
        new_version = build_data.get_omaha_upgrade(
            {}, 'board', None)
        self.assertIsNone(new_version)


class GetFirmwareVersionsTests(unittest.TestCase):
    """Tests for get_firmware_versions."""

    def setUp(self):
        self.cros_version = 'R64-10176.65.0'

    @mock.patch.object(build_data, '_read_gs_json_data')
    def test_get_firmware_versions_on_normal_build(self, mock_read_gs):
        """Test get_firmware_versions on normal build."""
        metadata_json = """
{
    "unibuild": false,
    "board-metadata":{
        "auron_paine":{
             "main-firmware-version":"Google_Auron_paine.6301.58.98"
        }
   }
}
        """
        mock_read_gs.return_value = json.loads(metadata_json)
        board = 'auron_paine'

        fw_version = build_data.get_firmware_versions(
                board, self.cros_version)
        expected_version = {board: "Google_Auron_paine.6301.58.98"}
        self.assertEqual(fw_version, expected_version)

    @mock.patch.object(build_data, '_read_gs_json_data',
                       side_effect = Exception('GS ERROR'))
    def test_get_firmware_versions_with_exceptions(self, mock_read_gs):
        """Test get_firmware_versions on normal build with exceptions."""
        afe_mock = mock.Mock()
        fw_version = build_data.get_firmware_versions(
                'auron_paine', self.cros_version)
        self.assertEqual(fw_version, {'auron_paine': None})

    @mock.patch.object(build_data, '_read_gs_json_data')
    def test_get_firmware_versions_on_unibuild(self, mock_read_gs):
        """Test get_firmware_version on uni-build."""
        metadata_json = """
{
    "unibuild": true,
    "board-metadata":{
        "coral":{
            "kernel-version":"4.4.114-r1354",
            "models":{
                "blue":{
                    "main-readonly-firmware-version":"Google_Coral.10068.37.0",
                    "main-readwrite-firmware-version":"Google_Coral.10068.39.0"
                },
                "robo360":{
                    "main-readonly-firmware-version":"Google_Coral.10068.34.0",
                    "main-readwrite-firmware-version":null
                },
                "porbeagle":{
                    "main-readonly-firmware-version":null,
                    "main-readwrite-firmware-version":null
                }
            }
        }
    }
}
"""
        mock_read_gs.return_value = json.loads(metadata_json)
        fw_version = build_data.get_firmware_versions(
            'coral', self.cros_version)
        expected_version = {
            'blue': 'Google_Coral.10068.39.0',
            'robo360': 'Google_Coral.10068.34.0',
            'porbeagle': None
        }
        self.assertEqual(fw_version, expected_version)

    @mock.patch.object(build_data, '_read_gs_json_data')
    def test_get_firmware_versions_on_unibuild_no_models(self, mock_read_gs):
        """Test get_firmware_versions on uni-build without models dict."""
        metadata_json = """
{
    "unibuild": true,
    "board-metadata":{
        "coral":{
            "kernel-version":"4.4.114-r1354"
        }
    }
}
"""
        mock_read_gs.return_value = json.loads(metadata_json)
        fw_version = build_data.get_firmware_versions(
                'coral', self.cros_version)
        self.assertEqual(fw_version, {'coral': None})


if __name__ == '__main__':
    unittest.main()
