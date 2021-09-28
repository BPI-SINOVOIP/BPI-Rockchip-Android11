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
"""Bluetooth disconnect and reconnect verification."""
# Quick way to get the Apollo serial number:
# python3.5 -c "from acts.controllers.buds_lib.apollo_lib import get_devices; [print(d['serial_number']) for d in get_devices()]"

import statistics
import time
from acts import asserts
from acts.base_test import BaseTestClass
from acts.controllers.buds_lib.test_actions.apollo_acts import ApolloTestActions
from acts.signals import TestFailure
from acts.signals import TestPass
from acts.test_decorators import test_tracker_info
from acts.test_utils.bt.BluetoothBaseTest import BluetoothBaseTest
from acts.test_utils.bt.bt_test_utils import enable_bluetooth
from acts.test_utils.bt.bt_test_utils import setup_multiple_devices_for_bt_test
from acts.test_utils.bt.loggers.bluetooth_metric_logger import BluetoothMetricLogger
from acts.utils import set_location_service

# The number of reconnections to be attempted during the test
RECONNECTION_ATTEMPTS = 200


class BluetoothReconnectError(TestFailure):
    pass


class BluetoothReconnectTest(BaseTestClass):
    """Connects a phone to Apollo earbuds to test Bluetooth reconnection.

   Attributes:
       phone: An Android phone object
       apollo: An Apollo earbuds object
       apollo_act: An Apollo test action object
       dut_bt_addr: The Bluetooth address of the Apollo earbuds
    """

    def setup_class(self):
        super().setup_class()
        # sanity check of the dut devices.
        # TODO(b/119051823): Investigate using a config validator to replace this.
        if not self.android_devices:
            raise ValueError(
                'Cannot find android phone (need at least one).')
        self.phone = self.android_devices[0]

        if not self.buds_devices:
            raise ValueError(
                'Cannot find apollo device (need at least one).')
        self.apollo = self.buds_devices[0]
        self.log.info('Successfully found needed devices.')

        # Staging the test, create result object, etc.
        self.apollo_act = ApolloTestActions(self.apollo, self.log)
        self.dut_bt_addr = self.apollo.bluetooth_address
        self.bt_logger = BluetoothMetricLogger.for_test_case()

    def setup_test(self):
        setup_multiple_devices_for_bt_test(self.android_devices)
        # Make sure Bluetooth is on
        enable_bluetooth(self.phone.droid, self.phone.ed)
        set_location_service(self.phone, True)
        self.apollo_act.factory_reset()

        # Initial pairing and connection of devices
        self.phone.droid.bluetoothDiscoverAndBond(self.dut_bt_addr)
        paired_and_connected = self.apollo_act.wait_for_bluetooth_a2dp_hfp()
        asserts.assert_true(paired_and_connected,
                            'Failed to pair and connect devices')
        time.sleep(20)
        self.log.info('===== START BLUETOOTH RECONNECT TEST  =====')

    def teardown_test(self):
        self.log.info('Teardown test, shutting down all services...')
        self.apollo_act.factory_reset()
        self.apollo.close()

    def _reconnect_bluetooth_from_phone(self):
        """Reconnects Bluetooth from the phone.

        Disables and then re-enables Bluetooth from the phone when Bluetooth
        disconnection has been verified. Measures the reconnection time.

        Returns:
            The time it takes to connect Bluetooth in milliseconds.

        Raises:
            BluetoothReconnectError
        """

        # Disconnect Bluetooth from the phone side
        self.log.info('Disconnecting Bluetooth from phone')
        self.phone.droid.bluetoothDisconnectConnected(self.dut_bt_addr)
        if not self.apollo_act.wait_for_bluetooth_disconnection():
            raise BluetoothReconnectError('Failed to disconnect Bluetooth')
        self.log.info('Bluetooth disconnected successfully')

        # Buffer between disconnect and reconnect
        time.sleep(3)

        # Reconnect Bluetooth from the phone side
        self.log.info('Connecting Bluetooth from phone')
        start_time = time.perf_counter()
        self.phone.droid.bluetoothConnectBonded(self.dut_bt_addr)
        self.log.info('Bluetooth connected successfully')
        if not self.apollo_act.wait_for_bluetooth_a2dp_hfp():
            raise BluetoothReconnectError('Failed to connect Bluetooth')
        end_time = time.perf_counter()
        return (end_time - start_time) * 1000

    @BluetoothBaseTest.bt_test_wrap
    @test_tracker_info(uuid='da921903-92d0-471d-ae01-456058cc1297')
    def test_bluetooth_reconnect(self):
        """Reconnects Bluetooth between a phone and Apollo device a specified
        number of times and reports connection time statistics."""

        # Store metrics
        metrics = {}
        connection_success = 0
        connection_times = []
        reconnection_failures = []
        first_connection_failure = None

        for attempt in range(RECONNECTION_ATTEMPTS):
            self.log.info("Reconnection attempt {}".format(attempt + 1))
            reconnect_timestamp = time.strftime('%Y-%m-%d %H:%M:%S',
                                                time.localtime())
            try:
                connection_time = self._reconnect_bluetooth_from_phone()
            except BluetoothReconnectError as err:
                self.log.error(err)
                failure_data = {'timestamp': reconnect_timestamp,
                                'error': str(err),
                                'reconnect_attempt': attempt + 1}
                reconnection_failures.append(failure_data)
                if not first_connection_failure:
                    first_connection_failure = err
            else:
                connection_times.append(connection_time)
                connection_success += 1

            # Buffer between reconnection attempts
            time.sleep(3)

        metrics['connection_attempt_count'] = RECONNECTION_ATTEMPTS
        metrics['connection_successful_count'] = connection_success
        metrics['connection_failed_count'] = (RECONNECTION_ATTEMPTS
                                              - connection_success)
        if len(connection_times) > 0:
            metrics['connection_max_time_millis'] = int(max(connection_times))
            metrics['connection_min_time_millis'] = int(min(connection_times))
            metrics['connection_avg_time_millis'] = int(statistics.mean(
                connection_times))

        if reconnection_failures:
            metrics['connection_failure_info'] = reconnection_failures

        proto = self.bt_logger.get_results(metrics,
                                           self.__class__.__name__,
                                           self.phone,
                                           self.apollo)

        self.log.info('Metrics: {}'.format(metrics))

        if RECONNECTION_ATTEMPTS != connection_success:
            raise TestFailure(str(first_connection_failure), extras=proto)
        else:
            raise TestPass('Bluetooth reconnect test passed', extras=proto)
