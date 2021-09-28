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
This is a stress test for Fuchsia GATT connections.

Setup:
This test only requires two fuchsia devices as the purpose is to test
the robusntess of GATT connections.
"""

import time

from acts import signals
from acts.base_test import BaseTestClass
from acts.test_decorators import test_tracker_info
from acts.test_utils.bt.bt_test_utils import generate_id_by_size


class FuchsiaBtScanTest(BaseTestClass):
    scan_timeout_seconds = 30

    def setup_class(self):
        super().setup_class()
        self.pri_dut = self.fuchsia_devices[0]
        self.sec_dut = self.fuchsia_devices[1]

        self.pri_dut.btc_lib.initBluetoothControl()
        self.sec_dut.btc_lib.initBluetoothControl()

    # TODO: add @test_tracker_info(uuid='')
    def test_scan_with_peer_set_non_discoverable(self):
        """Test Bluetooth scan with peer set to non discoverable.

        Steps:
        1. Set peer device to a unique device name.
        2. Set peer device to be non-discoverable.
        3. Perform a BT Scan with primary dut with enough time to
        gather results.

        Expected Result:
        Verify there are no results that match the unique device
        name in step 1.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during the test.

        TAGS: BR/EDR, BT
        Priority: 1
        """
        local_name = generate_id_by_size(10)
        self.sec_dut.btc_lib.setName(local_name)
        self.sec_dut.btc_lib.setDiscoverable(False)

        self.pri_dut.btc_lib.requestDiscovery(True)
        time.sleep(self.scan_timeout_seconds)
        self.pri_dut.btc_lib.requestDiscovery(False)
        discovered_devices = self.pri_dut.btc_lib.getKnownRemoteDevices()
        for device in discovered_devices.get("result").values():
            discoverd_name = device.get("name")
            if discoverd_name is not None and discoverd_name is local_name:
                raise signals.TestFailure(
                    "Found peer unexpectedly: {}.".format(device))
        raise signals.TestPass("Successfully didn't find peer device.")

    # TODO: add @test_tracker_info(uuid='')
    def test_scan_with_peer_set_discoverable(self):
        """Test Bluetooth scan with peer set to discoverable.

        Steps:
        1. Set peer device to a unique device name.
        2. Set peer device to be discoverable.
        3. Perform a BT Scan with primary dut with enough time to
        gather results.

        Expected Result:
        Verify there is a result that match the unique device
        name in step 1.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during the test.

        TAGS: BR/EDR, BT
        Priority: 1
        """
        local_name = generate_id_by_size(10)
        self.log.info("Setting local peer name to: {}".format(local_name))
        self.sec_dut.btc_lib.setName(local_name)
        self.sec_dut.btc_lib.setDiscoverable(True)

        self.pri_dut.btc_lib.requestDiscovery(True)
        end_time = time.time() + self.scan_timeout_seconds
        poll_timeout = 10
        while time.time() < end_time:
            discovered_devices = self.pri_dut.btc_lib.getKnownRemoteDevices()
            for device in discovered_devices.get("result").values():
                self.log.info(device)
                discoverd_name = device.get("name")
                if discoverd_name is not None and discoverd_name in local_name:
                    self.pri_dut.btc_lib.requestDiscovery(False)
                    raise signals.TestPass("Successfully found peer device.")
            time.sleep(poll_timeout)
        self.pri_dut.btc_lib.requestDiscovery(False)
        raise signals.TestFailure("Unable to find peer device.")
