# Copyright 2018 - The Android Open Source Project
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
"""Tests for host_setup_runner."""
import subprocess
import unittest

from acloud import errors
from acloud.internal.lib import driver_test_lib
from acloud.setup import setup_common


class SetupCommonTest(driver_test_lib.BaseDriverTest):
    """Test HostPkgTaskRunner."""
    PKG_INFO_INSTALLED = """fake_pkg:
  Installed: 0.7
  Candidate: 0.7
  Version table:
"""
    PKG_INFO_NONE_INSTALL = """fake_pkg:
  Installed: (none)
  Candidate: 0.7
  Version table:
"""
    PKG_INFO_OLD_VERSION = """fake_pkg:
  Installed: 0.2
  Candidate: 0.7
  Version table:
"""

    # pylint: disable=invalid-name
    def testPackageNotInstalled(self):
        """"Test PackageInstalled return False when Installed status is (None). """
        self.Patch(
            setup_common,
            "CheckCmdOutput",
            return_value=self.PKG_INFO_NONE_INSTALL)

        self.assertFalse(
            setup_common.PackageInstalled("fake_package"))

    def testUnableToLocatePackage(self):
        """"Test PackageInstalled return False if unable to locate package."""
        self.Patch(
            setup_common,
            "CheckCmdOutput",
            side_effect=subprocess.CalledProcessError(
                None, "This error means unable to locate package on repository."))

        with self.assertRaises(errors.UnableToLocatePkgOnRepositoryError):
            setup_common.PackageInstalled("fake_package")

    # pylint: disable=invalid-name
    def testPackageInstalledForOldVersion(self):
        """Test PackageInstalled should return True when pkg is out-of-date."""
        self.Patch(
            setup_common,
            "CheckCmdOutput",
            return_value=self.PKG_INFO_OLD_VERSION)

        self.assertTrue(setup_common.PackageInstalled("fake_package",
                                                      compare_version=True))

    def testPackageInstalled(self):
        """Test PackageInstalled should return True when pkg is installed."""
        self.Patch(
            setup_common,
            "CheckCmdOutput",
            return_value=self.PKG_INFO_INSTALLED)

        self.assertTrue(setup_common.PackageInstalled("fake_package"))


if __name__ == "__main__":
    unittest.main()
