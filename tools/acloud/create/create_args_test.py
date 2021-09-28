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
"""Tests for create."""

import unittest
import mock

from acloud import errors
from acloud.create import create_args
from acloud.internal import constants
from acloud.internal.lib import driver_test_lib


def _CreateArgs():
    """set default pass in arguments."""
    mock_args = mock.MagicMock(
        flavor=None,
        num=None,
        adb_port=None,
        hw_property=None,
        stable_cheeps_host_image_name=None,
        stable_cheeps_host_image_project=None,
        username=None,
        password=None,
        local_image="",
        local_system_image="",
        system_branch=None,
        system_build_id=None,
        system_build_target=None,
        local_instance=None,
        remote_host=None,
        host_user=constants.GCE_USER,
        host_ssh_private_key_path=None,
        avd_type=constants.TYPE_CF,
        autoconnect=constants.INS_KEY_VNC)
    return mock_args


# pylint: disable=invalid-name,protected-access
class CreateArgsTest(driver_test_lib.BaseDriverTest):
    """Test create_args functions."""

    def testVerifyArgs(self):
        """test VerifyArgs."""
        mock_args = _CreateArgs()
        # Test args default setting shouldn't raise error.
        self.assertEqual(None, create_args.VerifyArgs(mock_args))

    def testVerifyArgs_ConnectWebRTC(self):
        """test VerifyArgs args.autconnect webrtc.

        WebRTC only apply to remote cuttlefish instance

        """
        mock_args = _CreateArgs()
        mock_args.autoconnect = constants.INS_KEY_WEBRTC
        # Test remote instance and avd_type cuttlefish(default)
        # Test args.autoconnect webrtc shouldn't raise error.
        self.assertEqual(None, create_args.VerifyArgs(mock_args))

        # Test pass in none-cuttlefish avd_type should raise error.
        mock_args.avd_type = constants.TYPE_GF
        self.assertRaises(errors.UnsupportedCreateArgs,
                          create_args.VerifyArgs, mock_args)


if __name__ == "__main__":
    unittest.main()
