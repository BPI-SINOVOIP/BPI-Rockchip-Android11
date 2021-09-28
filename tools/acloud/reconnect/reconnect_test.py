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
"""Tests for reconnect."""

import collections
import unittest
import subprocess

import mock

from acloud import errors
from acloud.internal import constants
from acloud.internal.lib import driver_test_lib
from acloud.internal.lib import utils
from acloud.internal.lib.adb_tools import AdbTools
from acloud.reconnect import reconnect


ForwardedPorts = collections.namedtuple("ForwardedPorts",
                                        [constants.VNC_PORT, constants.ADB_PORT])


class ReconnectTest(driver_test_lib.BaseDriverTest):
    """Test reconnect functions."""

    # pylint: disable=no-member
    def testReconnectInstance(self):
        """Test Reconnect Instances."""
        ssh_private_key_path = "/fake/acloud_rea"
        fake_report = mock.MagicMock()
        instance_object = mock.MagicMock()
        instance_object.ip = "1.1.1.1"
        instance_object.islocal = False
        instance_object.adb_port = "8686"
        instance_object.avd_type = "cuttlefish"
        self.Patch(subprocess, "check_call", return_value=True)
        self.Patch(utils, "LaunchVncClient")
        self.Patch(utils, "AutoConnect")
        self.Patch(AdbTools, "IsAdbConnected", return_value=False)
        self.Patch(AdbTools, "IsAdbConnectionAlive", return_value=False)
        self.Patch(utils, "IsCommandRunning", return_value=False)

        #test ssh tunnel not connected, remote instance.
        instance_object.vnc_port = 6666
        instance_object.display = ""
        utils.AutoConnect.call_count = 0
        reconnect.ReconnectInstance(ssh_private_key_path, instance_object, fake_report)
        utils.AutoConnect.assert_not_called()
        utils.LaunchVncClient.assert_called_with(6666)

        instance_object.display = "888x777 (99)"
        utils.AutoConnect.call_count = 0
        reconnect.ReconnectInstance(ssh_private_key_path, instance_object, fake_report)
        utils.AutoConnect.assert_not_called()
        utils.LaunchVncClient.assert_called_with(6666, "888", "777")

        #test ssh tunnel connected , remote instance.
        instance_object.ssh_tunnel_is_connected = False
        instance_object.display = ""
        utils.AutoConnect.call_count = 0
        instance_object.vnc_port = 5555
        extra_args_ssh_tunnel = None
        self.Patch(utils, "AutoConnect",
                   return_value=ForwardedPorts(vnc_port=11111, adb_port=22222))
        reconnect.ReconnectInstance(ssh_private_key_path, instance_object, fake_report)
        utils.AutoConnect.assert_called_with(ip_addr=instance_object.ip,
                                             rsa_key_file=ssh_private_key_path,
                                             target_vnc_port=constants.CF_VNC_PORT,
                                             target_adb_port=constants.CF_ADB_PORT,
                                             ssh_user=constants.GCE_USER,
                                             extra_args_ssh_tunnel=extra_args_ssh_tunnel)
        utils.LaunchVncClient.assert_called_with(11111)

        instance_object.display = "999x777 (99)"
        extra_args_ssh_tunnel = "fake_extra_args_ssh_tunnel"
        utils.AutoConnect.call_count = 0
        reconnect.ReconnectInstance(ssh_private_key_path,
                                    instance_object,
                                    fake_report,
                                    extra_args_ssh_tunnel)
        utils.AutoConnect.assert_called_with(ip_addr=instance_object.ip,
                                             rsa_key_file=ssh_private_key_path,
                                             target_vnc_port=constants.CF_VNC_PORT,
                                             target_adb_port=constants.CF_ADB_PORT,
                                             ssh_user=constants.GCE_USER,
                                             extra_args_ssh_tunnel=extra_args_ssh_tunnel)
        utils.LaunchVncClient.assert_called_with(11111, "999", "777")

        #test reconnect local instance.
        instance_object.islocal = True
        instance_object.display = ""
        instance_object.vnc_port = 5555
        instance_object.ssh_tunnel_is_connected = False
        utils.AutoConnect.call_count = 0
        reconnect.ReconnectInstance(ssh_private_key_path,
                                    instance_object,
                                    fake_report)
        utils.AutoConnect.assert_not_called()
        utils.LaunchVncClient.assert_called_with(5555)

    def testReconnectInstanceAvdtype(self):
        """Test Reconnect Instances of avd_type."""
        ssh_private_key_path = "/fake/acloud_rea"
        fake_report = mock.MagicMock()
        instance_object = mock.MagicMock()
        instance_object.ip = "1.1.1.1"
        instance_object.vnc_port = 9999
        instance_object.adb_port = "9999"
        instance_object.islocal = False
        instance_object.ssh_tunnel_is_connected = False
        self.Patch(utils, "AutoConnect")
        self.Patch(reconnect, "StartVnc")
        #test reconnect remote instance when avd_type as gce.
        instance_object.avd_type = "gce"
        reconnect.ReconnectInstance(ssh_private_key_path, instance_object, fake_report)
        utils.AutoConnect.assert_called_with(ip_addr=instance_object.ip,
                                             rsa_key_file=ssh_private_key_path,
                                             target_vnc_port=constants.GCE_VNC_PORT,
                                             target_adb_port=constants.GCE_ADB_PORT,
                                             ssh_user=constants.GCE_USER,
                                             extra_args_ssh_tunnel=None)

        #test reconnect remote instance when avd_type as cuttlefish.
        instance_object.avd_type = "cuttlefish"
        reconnect.ReconnectInstance(ssh_private_key_path, instance_object, fake_report)
        utils.AutoConnect.assert_called_with(ip_addr=instance_object.ip,
                                             rsa_key_file=ssh_private_key_path,
                                             target_vnc_port=constants.CF_VNC_PORT,
                                             target_adb_port=constants.CF_ADB_PORT,
                                             ssh_user=constants.GCE_USER,
                                             extra_args_ssh_tunnel=None)


    def testReconnectInstanceUnknownAvdType(self):
        """Test reconnect instances of unknown avd type."""
        ssh_private_key_path = "/fake/acloud_rea"
        fake_report = mock.MagicMock()
        instance_object = mock.MagicMock()
        instance_object.avd_type = "unknown"
        self.assertRaises(errors.UnknownAvdType,
                          reconnect.ReconnectInstance,
                          ssh_private_key_path,
                          instance_object,
                          fake_report)


    def testReconnectInstanceNoAvdType(self):
        """Test reconnect instances with no avd type."""
        ssh_private_key_path = "/fake/acloud_rea"
        fake_report = mock.MagicMock()
        instance_object = mock.MagicMock()
        self.assertRaises(errors.UnknownAvdType,
                          reconnect.ReconnectInstance,
                          ssh_private_key_path,
                          instance_object,
                          fake_report)


    def testStartVnc(self):
        """Test start Vnc."""
        self.Patch(subprocess, "check_call", return_value=True)
        self.Patch(utils, "IsCommandRunning", return_value=False)
        self.Patch(utils, "LaunchVncClient")
        vnc_port = 5555
        display = ""
        reconnect.StartVnc(vnc_port, display)
        utils.LaunchVncClient.assert_called_with(5555)

        display = "888x777 (99)"
        utils.AutoConnect.call_count = 0
        reconnect.StartVnc(vnc_port, display)
        utils.LaunchVncClient.assert_called_with(5555, "888", "777")


if __name__ == "__main__":
    unittest.main()
