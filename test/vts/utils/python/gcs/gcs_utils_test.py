#!/usr/bin/env python
#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import mock
import unittest

from vts.utils.python.gcs import gcs_utils


def simple_GetGcloudPath():
    """mock function created for _GetGcloudPath"""
    return "gcloud"


def simple_ExecuteOneShellCommand(input_string):
    """mock function created for ExecuteOneShellCommand"""
    std_out = "this is standard output"
    std_err = "this is standard error"
    return_code = 0
    return std_out, std_err, return_code


class GcsUtilsTest(unittest.TestCase):
    """Unit tests for gcs_utils module"""

    def SetUp(self):
        """Setup tasks"""
        self.category = "category_default"
        self.name = "name_default"

    def testInitialization(self):
        """Tests the initilization of a GcsUtils object"""
        user_params = {"service_key_json_path": "key.json"}
        _gcs_utils = gcs_utils.GcsUtils(user_params)
        self.assertEqual(_gcs_utils.service_key_json_path, "key.json")

    @mock.patch(
        'vts.utils.python.gcs.gcs_utils.GcsUtils.GetGcloudPath',
        side_effect=simple_GetGcloudPath)
    @mock.patch(
        'vts.utils.python.common.cmd_utils.ExecuteOneShellCommand',
        side_effect=simple_ExecuteOneShellCommand)
    def testGetGcloudAuth(self, simple_ExecuteOneShellCommand,
                          simeple_GetGCloudPath):
        """Tests the GetGcloudAuth function"""
        user_params = {"service_key_json_path": "key.json"}
        _gcs_utils = gcs_utils.GcsUtils(user_params)
        _gcs_utils.GetGcloudAuth()
        simple_ExecuteOneShellCommand.assert_called_with(
            "gcloud auth activate-service-account --key-file key.json")

    @mock.patch(
        'vts.utils.python.common.cmd_utils.ExecuteOneShellCommand',
        side_effect=simple_ExecuteOneShellCommand)
    def testGetGcloudPath(self, simple_ExecuteOneShellCommand):
        """Tests the GetGcloudPath static function"""
        result = gcs_utils.GcsUtils.GetGcloudPath()
        simple_ExecuteOneShellCommand.assert_called_with("which gcloud")
        self.assertEqual(result, "this is standard output")

    @mock.patch(
        'vts.utils.python.common.cmd_utils.ExecuteOneShellCommand',
        side_effect=simple_ExecuteOneShellCommand)
    def testGetGsutilPath(self, simple_ExecuteOneShellCommand):
        """Tests the GetGsutilPath static function"""
        result = gcs_utils.GcsUtils.GetGsutilPath()
        simple_ExecuteOneShellCommand.assert_called_with("which gsutil")
        self.assertEqual(result, "this is standard output")


if __name__ == "__main__":
    unittest.main()
