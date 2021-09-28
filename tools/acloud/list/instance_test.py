#!/usr/bin/env python
#
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
"""Tests for instance class."""

import collections
import datetime
import subprocess

import unittest
import mock

# pylint: disable=import-error
import dateutil.parser
import dateutil.tz

from acloud.internal import constants
from acloud.internal.lib import cvd_runtime_config
from acloud.internal.lib import driver_test_lib
from acloud.internal.lib.adb_tools import AdbTools
from acloud.list import instance


class InstanceTest(driver_test_lib.BaseDriverTest):
    """Test instance."""
    PS_SSH_TUNNEL = ("/fake_ps_1 --fake arg \n"
                     "/fake_ps_2 --fake arg \n"
                     "/usr/bin/ssh -i ~/.ssh/acloud_rsa "
                     "-o UserKnownHostsFile=/dev/null "
                     "-o StrictHostKeyChecking=no -L 12345:127.0.0.1:6444 "
                     "-L 54321:127.0.0.1:6520 -N -f -l user 1.1.1.1")
    PS_LAUNCH_CVD = ("Sat Nov 10 21:55:10 2018 /fake_path/bin/run_cvd ")
    PS_RUNTIME_CF_CONFIG = {"x_res": "1080", "y_res": "1920", "dpi": "480"}
    GCE_INSTANCE = {
        constants.INS_KEY_NAME: "fake_ins_name",
        constants.INS_KEY_CREATETIME: "fake_create_time",
        constants.INS_KEY_STATUS: "fake_status",
        constants.INS_KEY_ZONE: "test/zones/fake_zone",
        "networkInterfaces": [{"accessConfigs": [{"natIP": "1.1.1.1"}]}],
        "labels": {constants.INS_KEY_AVD_TYPE: "fake_type",
                   constants.INS_KEY_AVD_FLAVOR: "fake_flavor"},
        "metadata": {
            "items":[{"key":constants.INS_KEY_AVD_TYPE,
                      "value":"fake_type"},
                     {"key":constants.INS_KEY_AVD_FLAVOR,
                      "value":"fake_flavor"}]}
    }

    # pylint: disable=protected-access
    def testCreateLocalInstance(self):
        """"Test get local instance info from launch_cvd process."""
        self.Patch(subprocess, "check_output", return_value=self.PS_LAUNCH_CVD)
        cf_config = mock.MagicMock(
            instance_id=2,
            x_res=1080,
            y_res=1920,
            dpi=480,
            instance_dir="fake_instance_dir",
            adb_port=6521,
            vnc_port=6445,
            adb_ip_port="127.0.0.1:6521",
        )
        self.Patch(cvd_runtime_config, "CvdRuntimeConfig",
                   return_value=cf_config)
        local_instance = instance.LocalInstance(cf_config)

        self.assertEqual(constants.LOCAL_INS_NAME + "-2", local_instance.name)
        self.assertEqual(True, local_instance.islocal)
        self.assertEqual("1080x1920 (480)", local_instance.display)
        expected_full_name = ("device serial: 127.0.0.1:%s (%s) elapsed time: %s"
                              % ("6521",
                                 constants.LOCAL_INS_NAME + "-2",
                                 "None"))
        self.assertEqual(expected_full_name, local_instance.fullname)
        self.assertEqual(6521, local_instance.adb_port)
        self.assertEqual(6445, local_instance.vnc_port)

    @mock.patch("acloud.list.instance.tempfile")
    @mock.patch("acloud.list.instance.AdbTools")
    def testCreateLocalGoldfishInstance(self, mock_adb_tools, mock_tempfile):
        """"Test the attributes of LocalGoldfishInstance."""
        mock_tempfile.gettempdir.return_value = "/unit/test"
        mock_adb_tools.return_value = mock.Mock(device_information={})

        inst = instance.LocalGoldfishInstance(1)

        self.assertEqual(inst.name, "local-goldfish-instance-1")
        self.assertEqual(inst.avd_type, constants.TYPE_GF)
        self.assertEqual(inst.adb_port, 5555)
        self.assertTrue(inst.islocal)
        self.assertEqual(inst.console_port, 5554)
        self.assertEqual(inst.device_serial, "emulator-5554")
        self.assertEqual(inst.instance_dir,
                         "/unit/test/acloud_gf_temp/local-goldfish-instance-1")

    @mock.patch("acloud.list.instance.open",
                mock.mock_open(read_data="test createtime"))
    @mock.patch("acloud.list.instance.os.path.isfile")
    @mock.patch("acloud.list.instance.os.listdir")
    @mock.patch("acloud.list.instance.os.path.isdir")
    @mock.patch("acloud.list.instance.tempfile")
    @mock.patch("acloud.list.instance.AdbTools")
    @mock.patch("acloud.list.instance._GetElapsedTime")
    def testGetLocalGoldfishInstances(self, mock_get_elapsed_time,
                                      mock_adb_tools, mock_tempfile,
                                      mock_isdir, mock_listdir, mock_isfile):
        """Test LocalGoldfishInstance.GetExistingInstances."""
        mock_get_elapsed_time.return_value = datetime.timedelta(hours=10)
        mock_adb_tools.return_value = mock.Mock(device_information={})
        mock_tempfile.gettempdir.return_value = "/unit/test"
        acloud_gf_temp_path = "/unit/test/acloud_gf_temp"
        subdir_names = (
            "local-goldfish-instance-1",
            "local-goldfish-instance-2",
            "local-goldfish-instance-3")
        timestamp_paths = (
            "/unit/test/acloud_gf_temp/local-goldfish-instance-1/"
            "creation_timestamp.txt",
            "/unit/test/acloud_gf_temp/local-goldfish-instance-2/"
            "creation_timestamp.txt",
            "/unit/test/acloud_gf_temp/local-goldfish-instance-3/"
            "creation_timestamp.txt")
        mock_isdir.side_effect = lambda path: path == acloud_gf_temp_path
        mock_listdir.side_effect = lambda path: (
            subdir_names if path == acloud_gf_temp_path else [])
        mock_isfile.side_effect = lambda path: (
            path in (timestamp_paths[0], timestamp_paths[2]))

        instances = instance.LocalGoldfishInstance.GetExistingInstances()

        mock_isdir.assert_called_with(acloud_gf_temp_path)
        mock_listdir.assert_called_with(acloud_gf_temp_path)
        for timestamp_path in timestamp_paths:
            mock_isfile.assert_any_call(timestamp_path)
        self.assertEqual(len(instances), 2)
        self.assertEqual(instances[0].console_port, 5554)
        self.assertEqual(instances[0].createtime, "test createtime")
        self.assertEqual(instances[0].fullname,
                         "device serial: emulator-5554 "
                         "(local-goldfish-instance-1) "
                         "elapsed time: 10:00:00")
        self.assertEqual(instances[1].console_port, 5558)
        self.assertEqual(instances[1].createtime, "test createtime")
        self.assertEqual(instances[1].fullname,
                         "device serial: emulator-5558 "
                         "(local-goldfish-instance-3) "
                         "elapsed time: 10:00:00")

    def testGetElapsedTime(self):
        """Test _GetElapsedTime"""
        # Instance time can't parse
        start_time = "error time"
        self.assertEqual(instance._MSG_UNABLE_TO_CALCULATE,
                         instance._GetElapsedTime(start_time))

        # Remote instance elapsed time
        now = "2019-01-14T13:00:00.000-07:00"
        start_time = "2019-01-14T03:00:00.000-07:00"
        self.Patch(instance, "datetime")
        instance.datetime.datetime.now.return_value = dateutil.parser.parse(now)
        self.assertEqual(
            datetime.timedelta(hours=10), instance._GetElapsedTime(start_time))

        # Local instance elapsed time
        now = "Mon Jan 14 10:10:10 2019"
        start_time = "Mon Jan 14 08:10:10 2019"
        instance.datetime.datetime.now.return_value = dateutil.parser.parse(
            now).replace(tzinfo=dateutil.tz.tzlocal())
        self.assertEqual(
            datetime.timedelta(hours=2), instance._GetElapsedTime(start_time))

    # pylint: disable=protected-access
    def testGetAdbVncPortFromSSHTunnel(self):
        """"Test Get forwarding adb and vnc port from ssh tunnel."""
        self.Patch(subprocess, "check_output", return_value=self.PS_SSH_TUNNEL)
        self.Patch(instance, "_GetElapsedTime", return_value="fake_time")
        self.Patch(instance.RemoteInstance, "_GetZoneName", return_value="fake_zone")
        forwarded_ports = instance.RemoteInstance(
            mock.MagicMock()).GetAdbVncPortFromSSHTunnel(
                "1.1.1.1", constants.TYPE_CF)
        self.assertEqual(54321, forwarded_ports.adb_port)
        self.assertEqual(12345, forwarded_ports.vnc_port)

        # If avd_type is undefined in utils.AVD_PORT_DICT.
        forwarded_ports = instance.RemoteInstance(
            mock.MagicMock()).GetAdbVncPortFromSSHTunnel(
                "1.1.1.1", "undefined_avd_type")
        self.assertEqual(None, forwarded_ports.adb_port)
        self.assertEqual(None, forwarded_ports.vnc_port)

    # pylint: disable=protected-access
    def testProcessGceInstance(self):
        """"Test process instance detail."""
        fake_adb = 123456
        fake_vnc = 654321
        forwarded_ports = collections.namedtuple("ForwardedPorts",
                                                 [constants.VNC_PORT,
                                                  constants.ADB_PORT])
        self.Patch(
            instance.RemoteInstance,
            "GetAdbVncPortFromSSHTunnel",
            return_value=forwarded_ports(vnc_port=fake_vnc, adb_port=fake_adb))
        self.Patch(instance, "_GetElapsedTime", return_value="fake_time")
        self.Patch(AdbTools, "IsAdbConnected", return_value=True)

        # test ssh_tunnel_is_connected will be true if ssh tunnel connection is found
        instance_info = instance.RemoteInstance(self.GCE_INSTANCE)
        self.assertTrue(instance_info.ssh_tunnel_is_connected)
        self.assertEqual(instance_info.adb_port, fake_adb)
        self.assertEqual(instance_info.vnc_port, fake_vnc)
        self.assertEqual("1.1.1.1", instance_info.ip)
        self.assertEqual("fake_status", instance_info.status)
        self.assertEqual("fake_type", instance_info.avd_type)
        self.assertEqual("fake_flavor", instance_info.avd_flavor)
        expected_full_name = "device serial: 127.0.0.1:%s (%s) elapsed time: %s" % (
            fake_adb, self.GCE_INSTANCE[constants.INS_KEY_NAME], "fake_time")
        self.assertEqual(expected_full_name, instance_info.fullname)

        # test ssh tunnel is connected but adb is disconnected
        self.Patch(AdbTools, "IsAdbConnected", return_value=False)
        instance_info = instance.RemoteInstance(self.GCE_INSTANCE)
        self.assertTrue(instance_info.ssh_tunnel_is_connected)
        expected_full_name = "device serial: not connected (%s) elapsed time: %s" % (
            instance_info.name, "fake_time")
        self.assertEqual(expected_full_name, instance_info.fullname)

        # test ssh_tunnel_is_connected will be false if ssh tunnel connection is not found
        self.Patch(
            instance.RemoteInstance,
            "GetAdbVncPortFromSSHTunnel",
            return_value=forwarded_ports(vnc_port=None, adb_port=None))
        instance_info = instance.RemoteInstance(self.GCE_INSTANCE)
        self.assertFalse(instance_info.ssh_tunnel_is_connected)
        expected_full_name = "device serial: not connected (%s) elapsed time: %s" % (
            self.GCE_INSTANCE[constants.INS_KEY_NAME], "fake_time")
        self.assertEqual(expected_full_name, instance_info.fullname)

    def testInstanceSummary(self):
        """Test instance summary."""
        fake_adb = 123456
        fake_vnc = 654321
        forwarded_ports = collections.namedtuple("ForwardedPorts",
                                                 [constants.VNC_PORT,
                                                  constants.ADB_PORT])
        self.Patch(
            instance.RemoteInstance,
            "GetAdbVncPortFromSSHTunnel",
            return_value=forwarded_ports(vnc_port=fake_vnc, adb_port=fake_adb))
        self.Patch(instance, "_GetElapsedTime", return_value="fake_time")
        self.Patch(AdbTools, "IsAdbConnected", return_value=True)
        remote_instance = instance.RemoteInstance(self.GCE_INSTANCE)
        result_summary = (" name: fake_ins_name\n "
                          "   IP: 1.1.1.1\n "
                          "   create time: fake_create_time\n "
                          "   elapse time: fake_time\n "
                          "   status: fake_status\n "
                          "   avd type: fake_type\n "
                          "   display: None\n "
                          "   vnc: 127.0.0.1:654321\n "
                          "   zone: fake_zone\n "
                          "   adb serial: 127.0.0.1:123456\n "
                          "   product: None\n "
                          "   model: None\n "
                          "   device: None\n "
                          "   transport_id: None")
        self.assertEqual(remote_instance.Summary(), result_summary)

        self.Patch(
            instance.RemoteInstance,
            "GetAdbVncPortFromSSHTunnel",
            return_value=forwarded_ports(vnc_port=None, adb_port=None))
        self.Patch(instance, "_GetElapsedTime", return_value="fake_time")
        self.Patch(AdbTools, "IsAdbConnected", return_value=False)
        remote_instance = instance.RemoteInstance(self.GCE_INSTANCE)
        result_summary = (" name: fake_ins_name\n "
                          "   IP: 1.1.1.1\n "
                          "   create time: fake_create_time\n "
                          "   elapse time: fake_time\n "
                          "   status: fake_status\n "
                          "   avd type: fake_type\n "
                          "   display: None\n "
                          "   vnc: 127.0.0.1:None\n "
                          "   zone: fake_zone\n "
                          "   adb serial: disconnected")
        self.assertEqual(remote_instance.Summary(), result_summary)

    def testGetZoneName(self):
        """Test GetZoneName."""
        zone_info = "v1/projects/project/zones/us-central1-c"
        expected_result = "us-central1-c"
        self.assertEqual(instance.RemoteInstance._GetZoneName(zone_info),
                         expected_result)
        # Test can't get zone name from zone info.
        zone_info = "v1/projects/project/us-central1-c"
        self.assertEqual(instance.RemoteInstance._GetZoneName(zone_info), None)


if __name__ == "__main__":
    unittest.main()
