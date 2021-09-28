#!/usr/bin/env python
#
# Copyright 2016 - The Android Open Source Project
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

"""Tests for acloud.public.report."""

import unittest
from acloud.public import report


class ReportTest(unittest.TestCase):
    """Test Report class."""

    def testAddData(self):
        """test AddData."""
        test_report = report.Report("create")
        test_report.AddData("devices", {"instance_name": "instance_1"})
        test_report.AddData("devices", {"instance_name": "instance_2"})
        expected = {
            "devices": [{
                "instance_name": "instance_1"
            }, {
                "instance_name": "instance_2"
            }]
        }
        self.assertEqual(test_report.data, expected)

    def testAddError(self):
        """test AddError."""
        test_report = report.Report("create")
        test_report.errors.append("some errors")
        test_report.errors.append("some errors")
        self.assertEqual(test_report.errors, ["some errors", "some errors"])

    def testSetStatus(self):
        """test SetStatus."""
        test_report = report.Report("create")
        test_report.SetStatus(report.Status.SUCCESS)
        self.assertEqual(test_report.status, "SUCCESS")

        test_report.SetStatus(report.Status.FAIL)
        self.assertEqual(test_report.status, "FAIL")

        test_report.SetStatus(report.Status.BOOT_FAIL)
        self.assertEqual(test_report.status, "BOOT_FAIL")

        # Test that more severe status won't get overriden.
        test_report.SetStatus(report.Status.FAIL)
        self.assertEqual(test_report.status, "BOOT_FAIL")

    def testAddDevice(self):
        """test AddDevice."""
        test_report = report.Report("create")
        test_report.AddDevice("instance_1", "127.0.0.1", 6520, 6444)
        expected = {
            "devices": [{
                "instance_name": "instance_1",
                "ip": "127.0.0.1:6520",
                "adb_port": 6520,
                "vnc_port": 6444
            }]
        }
        self.assertEqual(test_report.data, expected)

    def testAddDeviceBootFailure(self):
        """test AddDeviceBootFailure."""
        test_report = report.Report("create")
        test_report.AddDeviceBootFailure("instance_1", "127.0.0.1", 6520, 6444,
                                         "some errors")
        expected = {
            "devices_failing_boot": [{
                "instance_name": "instance_1",
                "ip": "127.0.0.1:6520",
                "adb_port": 6520,
                "vnc_port": 6444
            }]
        }
        self.assertEqual(test_report.data, expected)
        self.assertEqual(test_report.errors, ["some errors"])


if __name__ == "__main__":
    unittest.main()
