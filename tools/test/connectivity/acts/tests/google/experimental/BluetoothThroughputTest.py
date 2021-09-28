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

import statistics
from acts import asserts
from acts.base_test import BaseTestClass
from acts.signals import TestPass
from acts.test_decorators import test_tracker_info
from acts.test_utils.bt.BluetoothBaseTest import BluetoothBaseTest
from acts.test_utils.bt.bt_test_utils import orchestrate_rfcomm_connection
from acts.test_utils.bt.bt_test_utils import setup_multiple_devices_for_bt_test
from acts.test_utils.bt.bt_test_utils import verify_server_and_client_connected
from acts.test_utils.bt.loggers.bluetooth_metric_logger import BluetoothMetricLogger
from acts.test_utils.bt.loggers.protos import bluetooth_metric_pb2 as proto_module
from acts.utils import set_location_service


class BluetoothThroughputTest(BaseTestClass):
    """Connects two Android phones and tests the throughput between them.

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

    def _measure_throughput(self, num_of_buffers, buffer_size):
        """Measures the throughput of a data transfer.

        Sends data from the client device that is read by the server device.
        Calculates the throughput for the transfer.

        Args:
            num_of_buffers: An integer value designating the number of buffers
                          to be sent.
            buffer_size: An integer value designating the size of each buffer,
                       in bytes.

        Returns:
            The throughput of the transfer in bytes per second.
        """

        # TODO(b/119638242): Need to fix throughput send/receive methods
        (self.client_device.droid
         .bluetoothConnectionThroughputSend(num_of_buffers, buffer_size))

        throughput = (self.server_device.droid
                      .bluetoothConnectionThroughputRead(num_of_buffers,
                                                         buffer_size))
        return throughput

    @BluetoothBaseTest.bt_test_wrap
    @test_tracker_info(uuid='23afba5b-5801-42c2-8d7a-41510e91a605')
    def test_bluetooth_throughput_large_buffer(self):
        """Tests the throughput over a series of data transfers with large
        buffer size.
        """

        metrics = {}
        throughput_list = []

        for transfer in range(300):
            throughput = self._measure_throughput(1, 300)
            self.log.info('Throughput: {} bytes-per-sec'.format(throughput))
            throughput_list.append(throughput)

        metrics['data_transfer_protocol'] = self.data_transfer_type
        metrics['data_packet_size'] = 300
        metrics['data_throughput_min_bytes_per_second'] = int(
            min(throughput_list))
        metrics['data_throughput_max_bytes_per_second'] = int(
            max(throughput_list))
        metrics['data_throughput_avg_bytes_per_second'] = int(statistics.mean(
            throughput_list))

        proto = self.bt_logger.get_results(metrics,
                                           self.__class__.__name__,
                                           self.server_device,
                                           self.client_device)

        asserts.assert_true(metrics['data_throughput_min_bytes_per_second'] > 0,
                            'Minimum throughput must be greater than 0!',
                            extras=proto)
        raise TestPass('Throughput test (large buffer) completed successfully',
                       extras=proto)

    @BluetoothBaseTest.bt_test_wrap
    @test_tracker_info(uuid='5472fe33-891e-4fa1-ba84-78fc6f6a2327')
    def test_bluetooth_throughput_medium_buffer(self):
        """Tests the throughput over a series of data transfers with medium
        buffer size.
        """

        metrics = {}
        throughput_list = []

        for transfer in range(300):
            throughput = self._measure_throughput(1, 100)
            self.log.info('Throughput: {} bytes-per-sec'.format(throughput))
            throughput_list.append(throughput)

        metrics['data_transfer_protocol'] = self.data_transfer_type
        metrics['data_packet_size'] = 100
        metrics['data_throughput_min_bytes_per_second'] = int(
            min(throughput_list))
        metrics['data_throughput_max_bytes_per_second'] = int(
            max(throughput_list))
        metrics['data_throughput_avg_bytes_per_second'] = int(statistics.mean(
            throughput_list))

        proto = self.bt_logger.get_results(metrics,
                                           self.__class__.__name__,
                                           self.server_device,
                                           self.client_device)

        asserts.assert_true(metrics['data_throughput_min_bytes_per_second'] > 0,
                            'Minimum throughput must be greater than 0!',
                            extras=proto)
        raise TestPass('Throughput test (medium buffer) completed successfully',
                       extras=proto)

    @BluetoothBaseTest.bt_test_wrap
    @test_tracker_info(uuid='97589280-cefa-4ae4-b3fd-94ec9c1f4104')
    def test_bluetooth_throughput_small_buffer(self):
        """Tests the throughput over a series of data transfers with small
        buffer size.
        """

        metrics = {}
        throughput_list = []

        for transfer in range(300):
            throughput = self._measure_throughput(1, 10)
            self.log.info('Throughput: {} bytes-per-sec'.format(throughput))
            throughput_list.append(throughput)

        metrics['data_transfer_protocol'] = self.data_transfer_type
        metrics['data_packet_size'] = 10
        metrics['data_throughput_min_bytes_per_second'] = int(
            min(throughput_list))
        metrics['data_throughput_max_bytes_per_second'] = int(
            max(throughput_list))
        metrics['data_throughput_avg_bytes_per_second'] = int(statistics.mean(
            throughput_list))

        proto = self.bt_logger.get_results(metrics,
                                           self.__class__.__name__,
                                           self.server_device,
                                           self.client_device)

        asserts.assert_true(metrics['data_throughput_min_bytes_per_second'] > 0,
                            'Minimum throughput must be greater than 0!',
                            extras=proto)
        raise TestPass('Throughput test (small buffer) completed successfully',
                       extras=proto)

    @BluetoothBaseTest.bt_test_wrap
    def test_maximum_buffer_size(self):
        """Calculates the maximum allowed buffer size for one packet."""

        current_buffer_size = 1
        while True:
            self.log.info('Trying buffer size {}'.format(current_buffer_size))
            try:
                throughput = self._measure_throughput(1, current_buffer_size)
            except Exception:
                buffer_msg = ('Max buffer size: {} bytes'.
                              format(current_buffer_size - 1))
                throughput_msg = ('Max throughput: {} bytes-per-second'.
                                  format(throughput))
                self.log.info(buffer_msg)
                self.log.info(throughput_msg)
                return True
            current_buffer_size += 1
