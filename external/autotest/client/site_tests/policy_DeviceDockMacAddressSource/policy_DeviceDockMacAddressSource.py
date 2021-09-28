# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_DeviceDockMacAddressSource(
        enterprise_policy_base.EnterprisePolicyTest):
    """Test for setting the DeviceDockMacAddressSource policy.

    This test checks which MAC address will be used when a
    dock is connected to the device.

    """
    version = 1
    _POLICY = 'DeviceDockMacAddressSource'


    def _get_device_name(self):
        """Figure out which ethernet port is the dut.

        Since dut is the one plugged into the internet it's the one
        that has 'BROADCAST' and 'state UP' in 'ip link'.
        Example: "2: eth1: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500"
        " qdisc mq state UP".
        """
        active_ethernet = utils.run("ip link | grep 'BROADCAST.*state UP'")
        device_name = active_ethernet.stdout.split(":")
        device_name = device_name[1].lstrip()
        return device_name


    def _get_dock_mac(self, device_name):
        """Determine the dock's mac address.

        This is done via looking for an "eth" interface in /sys/class/net,
        that is NOT the interface currently in use by the device. E.g. if
        the "device_name" is "eth0", we are looking for an interface that
        has the name "eth" but not "eth0" (such as eth1").
        """
        dock_ethernet = utils.run(
            "ls /sys/class/net/ | grep -v {} | grep 'eth'".format(
                device_name))
        dock_ethernet = dock_ethernet.stdout.rstrip()
        dock_mac = utils.run(
            'cat /sys/class/net/{}/address'.format(dock_ethernet))
        dock_mac = dock_mac.stdout.lower().rstrip()
        return dock_mac


    def _get_dut_mac(self, device_name):
        """Grab duts's mac address."""
        dut_mac = utils.run(
            'cat /sys/class/net/{}/address'.format(device_name))
        dut_mac = dut_mac.stdout.lower().rstrip()
        return dut_mac


    def _get_designated_mac(self):
        """Device's designated dock MAC address."""
        desig_mac = utils.run('vpd -g dock_mac')
        desig_mac = desig_mac.stdout.lower().rstrip()
        return desig_mac


    def run_once(self, case, enroll=True, check_mac=False):
        """
        Entry point of this test.

        @param case: True, False, or None for the value of the policy.

        """

        TEST_CASES = {
            'designated_mac': 1,
            'device_mac': 2,
            'dock_mac': 3
        }

        if enroll:
            case_value = TEST_CASES[case]
            self.setup_case(
                device_policies={self._POLICY: case_value}, enroll=enroll)

        if check_mac:
            device_name = self._get_device_name()
            dock_mac = self._get_dock_mac(device_name)
            dut_mac = self._get_dut_mac(device_name)
            desig_mac = self._get_designated_mac()

            if case is 'designated_mac':
                if dock_mac != desig_mac:
                    raise error.TestFail(
                        'Dock MAC address should match the designated MAC '
                        'address and it does not.')
            elif case is 'device_mac':
                if dut_mac != dock_mac:
                    raise error.TestFail(
                        'Dock MAC address should match the device MAC '
                        'address and it does not.')
            else:
                if dock_mac == dut_mac or dock_mac == desig_mac:
                    raise error.TestFail(
                        'Dock MAC should not match any other MAC addresses '
                        'but it does.')
