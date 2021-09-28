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
"""This script shows simple examples of how to get started with bluetooth
   low energy testing in acts.
"""

import pprint
import random
import time

from acts.controllers import android_device
from acts.test_utils.fuchsia.bt_test_utils import le_scan_for_device_by_name
from acts.test_utils.bt.BluetoothBaseTest import BluetoothBaseTest
from acts.test_utils.bt.bt_constants import ble_advertise_settings_modes
from acts.test_utils.bt.bt_constants import adv_succ
from acts.test_utils.bt.bt_constants import ble_scan_settings_modes
from acts.test_utils.bt.bt_constants import scan_result
from acts.test_utils.bt.bt_test_utils import cleanup_scanners_and_advertisers
from acts.test_utils.bt.bt_test_utils import reset_bluetooth


class BleFuchsiaAndroidTest(BluetoothBaseTest):
    default_timeout = 10
    active_adv_callback_list = []
    droid = None

    def setup_class(self):
        super().setup_class()

        # Android device under test
        self.ad = self.android_devices[0]
        # Fuchsia device under test
        self.fd = self.fuchsia_devices[0]
        self.log.info("There are: {} fuchsia and {} android devices.".format(
            len(self.fuchsia_devices), len(self.android_devices)))

    def _start_generic_advertisement_include_device_name(self):
        self.ad.droid.bleSetAdvertiseDataIncludeDeviceName(True)
        self.ad.droid.bleSetAdvertiseSettingsAdvertiseMode(
            ble_advertise_settings_modes['low_latency'])
        advertise_data = self.ad.droid.bleBuildAdvertiseData()
        advertise_settings = self.ad.droid.bleBuildAdvertiseSettings()
        advertise_callback = self.ad.droid.bleGenBleAdvertiseCallback()
        self.ad.droid.bleStartBleAdvertising(advertise_callback,
                                             advertise_data,
                                             advertise_settings)
        self.ad.ed.pop_event(adv_succ.format(advertise_callback),
                             self.default_timeout)
        self.active_adv_callback_list.append(advertise_callback)
        return advertise_callback

    # Basic test for android device as advertiser and fuchsia device as scanner
    # Returns True if scan result has an entry corresponding to sample_android_name
    @BluetoothBaseTest.bt_test_wrap
    def test_fuchsia_scan_android_adv(self):
        sample_android_name = "Pixel1234"
        self.ad.droid.bluetoothSetLocalName(sample_android_name)
        adv_callback = self._start_generic_advertisement_include_device_name()
        droid_name = self.ad.droid.bluetoothGetLocalName()
        self.log.info("Android device name: {}".format(droid_name))
        res = True
        if not le_scan_for_device_by_name(
                self.fd, self.log, sample_android_name, self.default_timeout):
            res = False

        #Print clients to validate results are saved
        self.fd.print_clients()

        #Stop android advertising
        self.ad.droid.bleStopBleAdvertising(adv_callback)

        return res

    # Test for fuchsia device attempting to connect to android device (peripheral)
    # Also tests the list_services and discconect to a peripheral
    @BluetoothBaseTest.bt_test_wrap
    def test_fuchsia_connect_android_periph(self):
        sample_android_name = "Pixel1234"
        self.ad.droid.bluetoothStartPairingHelper()
        self.ad.droid.bluetoothSetLocalName(sample_android_name)
        adv_callback = self._start_generic_advertisement_include_device_name()
        droid_name = self.ad.droid.bluetoothGetLocalName()
        self.log.info("Android device name: {}".format(droid_name))

        scan_result = le_scan_for_device_by_name(self.fd, self.log,
                                                 sample_android_name,
                                                 self.default_timeout)
        if not scan_result:
            return False

        name, did, connectable = scan_result["name"], scan_result[
            "id"], scan_result["connectable"]

        connect = self.fd.gattc_lib.bleConnectToPeripheral(did)
        self.log.info("Connecting returned status: {}".format(connect))

        services = self.fd.gattc_lib.listServices(did)
        self.log.info("Listing services returned: {}".format(services))

        dconnect = self.fd.gattc_lib.bleDisconnectPeripheral(did)
        self.log.info("Disconnect status: {}".format(dconnect))

        #Print clients to validate results are saved
        self.fd.print_clients()

        #Stop android advertising + cleanup sl4f
        self.ad.droid.bleStopBleAdvertising(adv_callback)

        return True

    # Currently, this test doesn't work. The android device does not scan
    # TODO(): Debug android scan
    @BluetoothBaseTest.bt_test_wrap
    def test_fuchsia_adv_android_scan(self):
        #Initialize advertising on fuchsia device with name and interval
        fuchsia_name = "testADV123"
        adv_data = {"name": fuchsia_name}
        interval = 1000

        #Start advertising
        self.fd.ble_lib.bleStartBleAdvertising(adv_data, interval)

        # Initialize scan on android device which scan settings + callback
        filter_list = self.ad.droid.bleGenFilterList()
        self.ad.droid.bleSetScanFilterDeviceName(fuchsia_name)
        self.ad.droid.bleSetScanSettingsScanMode(
            ble_scan_settings_modes['low_latency'])
        scan_settings = self.ad.droid.bleBuildScanSetting()
        scan_callback = self.ad.droid.bleGenScanCallback()
        self.ad.droid.bleBuildScanFilter(filter_list)
        self.ad.droid.bleStartBleScan(filter_list, scan_settings,
                                      scan_callback)
        event_name = scan_result.format(scan_callback)
        try:
            event = self.ad.ed.pop_event(event_name, self.default_timeout)
            self.log.info("Found scan result: {}".format(
                pprint.pformat(event)))
        except Exception:
            self.log.error("Didn't find any scan results.")
            return False
        finally:
            self.fd.ble_lib.bleStopBleAdvertising()
            self.ad.droid.bleStopBleScan(scan_callback)
        # TODO(): Validate result
        return True
