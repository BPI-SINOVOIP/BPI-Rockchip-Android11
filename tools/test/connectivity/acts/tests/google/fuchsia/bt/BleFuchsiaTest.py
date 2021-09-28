#!/usr/bin/env python3
#
# Copyright (C) 2018 The Android Open Source Project
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
"""This scrip tests various BLE apis for Fuchsia devices.
"""

import pprint
import random
import time

from acts.base_test import BaseTestClass
from acts.test_utils.fuchsia.bt_test_utils import le_scan_for_device_by_name


class BleFuchsiaTest(BaseTestClass):
    default_timeout = 10
    active_scan_callback_list = []
    active_adv_callback_list = []
    droid = None

    def setup_class(self):
        super().setup_class()

        if (len(self.fuchsia_devices) < 2):
            self.log.error("BleFuchsiaTest Init: Not enough fuchsia devices.")
        self.log.info("Running testbed setup with two fuchsia devices")
        self.fuchsia_adv = self.fuchsia_devices[0]
        self.fuchsia_scan = self.fuchsia_devices[1]

    def test_fuchsia_publish_service(self):
        service_id = 0
        service_primary = True
        # Random uuid
        service_type = "0000180f-0000-1000-8000-00805fffffff"

        # Generate a random key for sl4f storage of proxy key
        service_proxy_key = "SProxy" + str(random.randint(0, 1000000))
        res = self.fuchsia_adv.ble_lib.blePublishService(
            service_id, service_primary, service_type, service_proxy_key)
        self.log.info("Publish result: {}".format(res))

        return True

    def test_fuchsia_scan_fuchsia_adv(self):
        # Initialize advertising on fuchsia dveice with name and interval
        fuchsia_name = "testADV1234"
        adv_data = {"name": fuchsia_name}
        interval = 1000
        res = True

        # Start advertising
        self.fuchsia_adv.ble_lib.bleStartBleAdvertising(adv_data, interval)
        self.log.info("Fuchsia advertising name: {}".format(fuchsia_name))

        # Start scan
        scan_result = le_scan_for_device_by_name(
            self.fuchsia_scan, self.log, fuchsia_name, self.default_timeout)
        if not scan_result:
            res = False

        # Stop advertising
        self.fuchsia_adv.ble_lib.bleStopBleAdvertising()

        return res

    def test_fuchsia_gatt_fuchsia_periph(self):
        # Create random service with id, primary, and uuid
        service_id = 3
        service_primary = True
        # Random uuid
        service_type = "0000180f-0000-1000-8000-00805fffffff"

        # Generate a random key for sl4f storage of proxy key
        service_proxy_key = "SProxy" + str(random.randint(0, 1000000))
        res = self.fuchsia_adv.ble_lib.blePublishService(
            service_id, service_primary, service_type, service_proxy_key)
        self.log.info("Publish result: {}".format(res))

        # Initialize advertising on fuchsia dveice with name and interval
        fuchsia_name = "testADV1234"
        adv_data = {"name": fuchsia_name}
        interval = 1000

        # Start advertising
        self.fuchsia_adv.ble_lib.bleStartBleAdvertising(adv_data, interval)
        self.log.info("Fuchsia advertising name: {}".format(fuchsia_name))

        # Start Scan
        scan_result = le_scan_for_device_by_name(
            self.fuchsia_scan, self.log, fuchsia_name, self.default_timeout)
        if not scan_result:
            self.fuchsia_adv.ble_lib.bleStopBleAdvertising()
            return False

        name, did, connectable = scan_result["name"], scan_result[
            "id"], scan_result["connectable"]

        connect = self.fuchsia_scan.gattc_lib.bleConnectToPeripheral(did)
        self.log.info("Connecting returned status: {}".format(connect))

        services = self.fuchsia_scan.gattc_lib.listServices(did)
        self.log.info("Listing services returned: {}".format(services))

        dconnect = self.fuchsia_scan.gattc_lib.bleDisconnectPeripheral(did)
        self.log.info("Disconnect status: {}".format(dconnect))

        # Stop fuchsia advertising
        self.fuchsia_adv.ble_lib.bleStopBleAdvertising()

        return True
