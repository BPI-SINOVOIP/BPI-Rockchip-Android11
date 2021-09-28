#/usr/bin/env python3
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

import random
import statistics
import string
import time
from acts import asserts
from acts.base_test import BaseTestClass
from acts.signals import TestPass
from acts.test_decorators import test_tracker_info
from acts.test_utils.bt.BluetoothBaseTest import BluetoothBaseTest
from acts.test_utils.bt.bt_test_utils import orchestrate_rfcomm_connection
from acts.test_utils.bt.bt_test_utils import setup_multiple_devices_for_bt_test
from acts.test_utils.bt.bt_test_utils import verify_server_and_client_connected
from acts.test_utils.bt.bt_test_utils import write_read_verify_data
from acts.test_utils.bt.loggers.bluetooth_metric_logger import BluetoothMetricLogger
from acts.test_utils.bt.loggers.protos import bluetooth_metric_pb2 as proto_module
from acts.utils import set_location_service


class BluetoothLatencyTest(BaseTestClass):
    """Connects two Android phones and tests the RFCOMM latency.

        Attributes:
             client_device: An Android device object that will be sending data
             server_device: An Android device object that will be receiving data
             bt_logger: The proxy logger instance for each test case
             data_transfer_type: Data transfer protocol used for the test
        """

    def setup_class(self):
        super().setup_class()

        # Sanity check of the devices under test
        # TODO(b/119051823): Investigate using a config validator to replace this.
        if len(self.android_devices) < 2:
            raise ValueError(
                'Not enough android phones detected (need at least two)')

        # Data will be sent from the client_device to the server_device
        self.client_device = self.android_devices[0]
        self.server_device = self.android_devices[1]
        self.bt_logger = BluetoothMetricLogger.for_test_case()
        self.data_transfer_type = proto_module.BluetoothDataTestResult.RFCOMM
        self.log.info('Successfully found required devices.')

    def setup_test(self):
        setup_multiple_devices_for_bt_test(self.android_devices)
        self._connect_rfcomm()

    def teardown_test(self):
        if verify_server_and_client_connected(
                self.client_device, self.server_device, log=False):
            self.client_device.droid.bluetoothSocketConnStop()
            self.server_device.droid.bluetoothSocketConnStop()

    def _connect_rfcomm(self):
        """Establishes an RFCOMM connection between two phones.

        Connects the client device to the server device given the hardware
        address of the server device.
        """

        set_location_service(self.client_device, True)
        set_location_service(self.server_device, True)
        server_address = self.server_device.droid.bluetoothGetLocalAddress()
        self.log.info('Pairing and connecting devices')
        asserts.assert_true(self.client_device.droid
                            .bluetoothDiscoverAndBond(server_address),
                            'Failed to pair and connect devices')

        # Create RFCOMM connection
        asserts.assert_true(orchestrate_rfcomm_connection
                            (self.client_device, self.server_device),
                            'Failed to establish RFCOMM connection')

    def _measure_latency(self):
        """Measures the latency of data transfer over RFCOMM.

        Sends data from the client device that is read by the server device.
        Calculates the latency of the transfer.

        Returns:
            The latency of the transfer milliseconds.
        """

        # Generates a random message to transfer
        message = (''.join(random.choice(string.ascii_letters + string.digits)
                           for _ in range(6)))

        start_time = time.perf_counter()
        write_read_successful = write_read_verify_data(self.client_device,
                                                       self.server_device,
                                                       message,
                                                       False)
        end_time = time.perf_counter()
        asserts.assert_true(write_read_successful,
                            'Failed to send/receive message')
        return (end_time - start_time) * 1000

    @BluetoothBaseTest.bt_test_wrap
    @test_tracker_info(uuid='7748295d-204e-4ad0-adf5-7591380b940a')
    def test_bluetooth_latency(self):
        """Tests the latency for a data transfer over RFCOMM"""

        metrics = {}
        latency_list = []

        for _ in range(300):
            latency_list.append(self._measure_latency())

        metrics['data_transfer_protocol'] = self.data_transfer_type
        metrics['data_latency_min_millis'] = int(min(latency_list))
        metrics['data_latency_max_millis'] = int(max(latency_list))
        metrics['data_latency_avg_millis'] = int(statistics.mean(latency_list))
        self.log.info('Latency: {}'.format(metrics))

        proto = self.bt_logger.get_results(metrics,
                                           self.__class__.__name__,
                                           self.server_device,
                                           self.client_device)

        asserts.assert_true(metrics['data_latency_min_millis'] > 0,
                            'Minimum latency must be greater than 0!',
                            extras=proto)

        raise TestPass('Latency test completed successfully', extras=proto)
