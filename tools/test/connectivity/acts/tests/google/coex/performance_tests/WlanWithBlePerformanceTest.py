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

import itertools
import time

from acts.test_utils.bt.bt_gatt_utils import close_gatt_client
from acts.test_utils.bt.bt_gatt_utils import disconnect_gatt_connection
from acts.test_utils.bt.bt_gatt_utils import GattTestUtilsError
from acts.test_utils.bt.bt_gatt_utils import orchestrate_gatt_connection
from acts.test_utils.bt.bt_test_utils import generate_ble_scan_objects
from acts.test_utils.coex.CoexPerformanceBaseTest import CoexPerformanceBaseTest


class WlanWithBlePerformanceTest(CoexPerformanceBaseTest):
    default_timeout = 10
    adv_instances = []
    bluetooth_gatt_list = []
    gatt_server_list = []

    def __init__(self, controllers):
        super().__init__(controllers)
        req_params = [
            # A dict containing:
            #     protocol: A list containing TCP/UDP. Ex: protocol: ['tcp'].
            #     stream: A list containing ul/dl. Ex: stream: ['ul']
            'standalone_params'
        ]
        self.unpack_userparams(req_params)
        self.tests = self.generate_test_cases(['start_stop_ble_scan',
                                               'ble_gatt_connection'])

    def setup_test(self):
        super().setup_test()
        self.pri_ad.droid.bluetoothDisableBLE()
        self.gatt_server_list = []
        self.adv_instances = []

    def teardown_test(self):
        super().teardown_test()
        for bluetooth_gatt in self.bluetooth_gatt_list:
            self.pri_ad.droid.gattClientClose(bluetooth_gatt)
        for gatt_server in self.gatt_server_list:
            self.sec_ad.droid.gattServerClose(gatt_server)
        for adv in self.adv_instances:
            self.sec_ad.droid.bleStopBleAdvertising(adv)
        return True

    def _orchestrate_gatt_disconnection(self, bluetooth_gatt, gatt_callback):
        """Disconnect gatt connection between two devices.

        Args:
            bluetooth_gatt: Index of the BluetoothGatt object
            gatt_callback: Index of gatt callback object.

        Steps:
        1. Disconnect gatt connection.
        2. Close bluetooth gatt object.

        Returns:
            True if successful, False otherwise.
        """
        self.log.info("Disconnecting from peripheral device.")
        try:
            disconnect_gatt_connection(self.pri_ad, bluetooth_gatt,
                                       gatt_callback)
            close_gatt_client(self.pri_ad, bluetooth_gatt)
            if bluetooth_gatt in self.bluetooth_gatt_list:
                self.bluetooth_gatt_list.remove(bluetooth_gatt)
        except GattTestUtilsError as err:
            self.log.error(err)
            return False
        return True

    def ble_start_stop_scan(self):
        """Convenience method to start BLE scan and stop BLE scan.

        Steps:
        1. Enable ble.
        2. Create LE scan objects.
        3. Start scan.
        4. Stop scan.

        Returns:
            True if successful, False otherwise.
        """
        for i in self.attenuation_range:
            self.pri_ad.droid.bluetoothEnableBLE()
            filter_list, scan_settings, scan_callback = generate_ble_scan_objects(
                self.pri_ad.droid)
            self.pri_ad.droid.bleStartBleScan(filter_list, scan_settings,
                                          scan_callback)
            time.sleep(self.iperf["duration"])
            try:
                self.pri_ad.droid.bleStopBleScan(scan_callback)
            except Exception as err:
                self.log.error(str(err))
                return False
        return True

    def initiate_ble_gatt_connection(self):
        """Creates gatt connection and disconnect gatt connection.

        Steps:
        1. Initializes gatt objects.
        2. Start a generic advertisement.
        3. Start a generic scanner.
        4. Find the advertisement and extract the mac address.
        5. Stop the first scanner.
        6. Create a GATT connection between the scanner and advertiser.
        7. Disconnect the GATT connection.

        Returns:
            True if successful, False otherwise.
        """
        start_time = time.time()
        while(time.time()) < (start_time + self.iperf["duration"]):
            self.pri_ad.droid.bluetoothEnableBLE()
            gatt_server_cb = (
                self.sec_ad.droid.gattServerCreateGattServerCallback())
            gatt_server = (
                self.sec_ad.droid.gattServerOpenGattServer(gatt_server_cb))
            self.gatt_server_list.append(gatt_server)
            try:
                bluetooth_gatt, gatt_callback, adv_callback = (
                    orchestrate_gatt_connection(self.pri_ad, self.sec_ad))
                self.bluetooth_gatt_list.append(bluetooth_gatt)
                time.sleep(self.default_timeout)
            except GattTestUtilsError as err:
                self.log.error(err)
                return False
            self.adv_instances.append(adv_callback)
            return self._orchestrate_gatt_disconnection(bluetooth_gatt,
                                                        gatt_callback)

    def start_stop_ble_scan(self):
        tasks = [(self.ble_start_stop_scan, ()),
                 (self.run_iperf_and_get_result, ())]
        return self.set_attenuation_and_run_iperf(tasks)

    def ble_gatt_connection(self):
        tasks = [(self.initiate_ble_gatt_connection, ()),
                 (self.run_iperf_and_get_result, ())]
        return self.set_attenuation_and_run_iperf(tasks)

    def generate_test_cases(self, test_types):
        test_cases = []
        for protocol, stream, test_type in itertools.product(
                self.standalone_params['protocol'],
                self.standalone_params['stream'], test_types):
            test_name = 'test_performance_with_{}_{}_{}'.format(
                test_type, protocol, stream)

            test_function = getattr(self, test_type)
            setattr(self, test_name, test_function)
            test_cases.append(test_name)
        return test_cases
