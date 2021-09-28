# Copyright 2019 - The Android Open Source Project
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
"""Tests for create."""

import os
import subprocess
import unittest
import mock

from acloud import errors
from acloud.create import avd_spec
from acloud.create import create
from acloud.create import gce_local_image_remote_instance
from acloud.internal import constants
from acloud.internal.lib import driver_test_lib
from acloud.internal.lib import utils
from acloud.public import config
from acloud.setup import gcp_setup_runner
from acloud.setup import host_setup_runner
from acloud.setup import setup


# pylint: disable=invalid-name,protected-access
class CreateTest(driver_test_lib.BaseDriverTest):
    """Test create functions."""

    def testGetAvdCreatorClass(self):
        """Test GetAvdCreatorClass."""
        # Checking wrong avd arg.
        avd_type = "unknown type"
        ins_type = "unknown ins"
        image_source = "unknown image"
        with self.assertRaises(errors.UnsupportedInstanceImageType):
            create.GetAvdCreatorClass(avd_type, ins_type, image_source)

        # Checking right avd arg.
        avd_creator_class = create.GetAvdCreatorClass(
            constants.TYPE_GCE,
            constants.INSTANCE_TYPE_REMOTE,
            constants.IMAGE_SRC_LOCAL)
        self.assertEqual(avd_creator_class,
                         gce_local_image_remote_instance.GceLocalImageRemoteInstance)

    # pylint: disable=protected-access
    def testCheckForAutoconnect(self):
        """Test CheckForAutoconnect."""
        args = mock.MagicMock()
        args.autoconnect = True

        self.Patch(utils, "InteractWithQuestion", return_value="Y")
        self.Patch(utils, "FindExecutable", return_value=None)

        # Checking autoconnect should be false if ANDROID_BUILD_TOP is not set.
        self.Patch(os.environ, "get", return_value=None)
        create._CheckForAutoconnect(args)
        self.assertEqual(args.autoconnect, False)

        # checking autoconnect should be True after user make adb from src.
        args.autoconnect = True
        self.Patch(subprocess, "check_call", return_value=True)
        self.Patch(os.environ, "get", return_value="/fake_dir2")
        create._CheckForAutoconnect(args)
        self.assertEqual(args.autoconnect, True)

        # checking autoconnect should be False if adb is not built.
        self.Patch(utils, "InteractWithQuestion", return_value="N")
        create._CheckForAutoconnect(args)
        self.assertEqual(args.autoconnect, False)

    # pylint: disable=protected-access,no-member
    def testCheckForSetup(self):
        """Test _CheckForSetup."""
        args = mock.MagicMock()
        args.local_instance = None
        args.args.config_file = "fake_path"
        self.Patch(gcp_setup_runner.GcpTaskRunner,
                   "ShouldRun",
                   return_value=False)
        self.Patch(host_setup_runner.HostBasePkgInstaller,
                   "ShouldRun",
                   return_value=False)
        self.Patch(config, "AcloudConfigManager")
        self.Patch(config.AcloudConfigManager, "Load")
        self.Patch(setup, "Run")
        self.Patch(utils, "InteractWithQuestion", return_value="Y")

        # Checking Setup.Run should not be called if all runner's ShouldRun func
        # return False
        create._CheckForSetup(args)
        gcp_setup_runner.GcpTaskRunner.ShouldRun.assert_called_once()
        host_setup_runner.HostBasePkgInstaller.ShouldRun.assert_called_once()
        setup.Run.assert_not_called()

        # Checking Setup.Run should be called if runner's ShouldRun func return
        # True
        self.Patch(gcp_setup_runner.GcpTaskRunner,
                   "ShouldRun",
                   return_value=True)
        create._CheckForSetup(args)
        setup.Run.assert_called_once()

    # pylint: disable=no-member
    def testRun(self):
        """Test Run."""
        args = mock.MagicMock()
        spec = mock.MagicMock()
        spec.avd_type = constants.TYPE_GCE
        spec.instance_type = constants.INSTANCE_TYPE_REMOTE
        spec.image_source = constants.IMAGE_SRC_LOCAL
        self.Patch(avd_spec, "AVDSpec", return_value=spec)
        self.Patch(config, "GetAcloudConfig")
        self.Patch(create, "PreRunCheck")
        self.Patch(gce_local_image_remote_instance.GceLocalImageRemoteInstance,
                   "Create")

        # Checking PreRunCheck func should be called if not skip_pre_run_check
        args.skip_pre_run_check = False
        create.Run(args)
        create.PreRunCheck.assert_called_once()

        # Checking PreRunCheck func should not be called if skip_pre_run_check
        args.skip_pre_run_check = True
        self.Patch(create, "PreRunCheck")
        create.Run(args)
        create.PreRunCheck.assert_not_called()


if __name__ == "__main__":
    unittest.main()
