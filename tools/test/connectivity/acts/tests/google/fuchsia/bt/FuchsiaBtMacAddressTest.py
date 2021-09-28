#!/usr/bin/env python3
#
# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""
This is a test to verify two or more Fuchsia devices don't have the same mac
address.

Setup:
This test requires at least two fuchsia devices.
"""

import time

from acts import signals
from acts.base_test import BaseTestClass
from acts.test_decorators import test_tracker_info
from acts.test_utils.bt.bt_test_utils import generate_id_by_size


class FuchsiaBtMacAddressTest(BaseTestClass):
    scan_timeout_seconds = 10

    def setup_class(self):
        super().setup_class()

        if len(self.fuchsia_devices) < 2:
            raise signals.TestAbortAll("Need at least two Fuchsia devices")
        for device in self.fuchsia_devices:
            device.btc_lib.initBluetoothControl()

    # TODO: add @test_tracker_info(uuid='')
    def test_verify_different_mac_addresses(self):
        """Verify that all connected Fuchsia devices have unique mac addresses.

        Steps:
        1. Get mac address from each device

        Expected Result:
        Verify duplicate mac addresses don't exist.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during the test.

        TAGS: BR/EDR, BT
        Priority: 1
        """
        mac_addr_list = []
        for device in self.fuchsia_devices:
            mac_addr_list.append(
                device.btc_lib.getActiveAdapterAddress().get("result"))
        if len(mac_addr_list) != len(set(mac_addr_list)):
            raise signals.TestFailure(
                "Found duplicate mac addresses {}.".format(mac_addr_list))
        raise signals.TestPass(
            "Success: All Bluetooth Mac address unique: {}".format(
                mac_addr_list))
