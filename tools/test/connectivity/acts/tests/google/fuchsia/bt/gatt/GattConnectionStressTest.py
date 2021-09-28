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

Custom Params:
gatt_connect_stress_test_iterations

    Example:
    "gatt_connect_stress_test_iterations": 10

Setup:
This test only requires two fuchsia devices as the purpose is to test
the robusntess of GATT connections.
"""

from acts import signals
from acts.base_test import BaseTestClass
from acts.test_decorators import test_tracker_info
from acts.test_utils.bt.bt_test_utils import generate_id_by_size
from acts.test_utils.fuchsia.bt_test_utils import le_scan_for_device_by_name
import time


class GattConnectionStressTest(BaseTestClass):
    gatt_connect_err_message = "Gatt connection failed with: {}"
    gatt_disconnect_err_message = "Gatt disconnection failed with: {}"
    ble_advertise_interval = 50
    scan_timeout_seconds = 10
    default_iterations = 1000

    def setup_class(self):
        super().setup_class()
        self.fuchsia_client_dut = self.fuchsia_devices[0]
        self.fuchsia_server_dut = self.fuchsia_devices[1]
        self.default_iterations = self.user_params.get(
            "gatt_connect_stress_test_iterations", self.default_iterations)

    def _orchestrate_single_connect_disconnect(self):
        adv_name = generate_id_by_size(10)
        adv_data = {"name": adv_name}
        self.fuchsia_server_dut.ble_lib.bleStartBleAdvertising(
            adv_data, self.ble_advertise_interval)
        device = le_scan_for_device_by_name(self.fuchsia_client_dut, self.log,
                                            adv_name,
                                            self.scan_timeout_seconds)
        if device is None:
            raise signals.TestFailure("Scanner unable to find advertisement.")
        connect_result = self.fuchsia_client_dut.gattc_lib.bleConnectToPeripheral(
            device["id"])
        if connect_result.get("error") is not None:
            raise signals.TestFailure(
                self.gatt_connect_err_message.format(
                    connect_result.get("error")))
        self.log.info("Connection Successful...")
        disconnect_result = self.fuchsia_client_dut.gattc_lib.bleDisconnectPeripheral(
            device["id"])
        if disconnect_result.get("error") is not None:
            raise signals.TestFailure(
                self.gatt_disconnect_err_message.format(
                    connect_result.get("error")))
        self.log.info("Disconnection Successful...")
        self.fuchsia_server_dut.ble_lib.bleStopBleAdvertising()

    # TODO: add @test_tracker_info(uuid='')
    def test_connect_reconnect_n_iterations_over_le(self):
        """Test GATT reconnection n times.

        Verify that the GATT client device can discover and connect to
        a perpheral n times. Default value is 1000.

        Steps:
        1. Setup Ble advertisement on peripheral with unique advertisement
            name.
        2. GATT client scans for peripheral advertisement.
        3. Upon find the advertisement, send a connection request to
            peripheral.

        Expected Result:
        Verify that there are no errors after each GATT connection.

        Returns:
          signals.TestPass if no errors
          signals.TestFailure if there are any errors during the test.

        TAGS: GATT
        Priority: 1
        """
        for i in range(self.default_iterations):
            self.log.info("Starting iteration {}".format(i + 1))
            self._orchestrate_single_connect_disconnect()
            self.log.info("Iteration {} successful".format(i + 1))
        raise signals.TestPass("Success")
