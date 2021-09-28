#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

# TODO: In the future to decide whether to move it to a common directory rather
# than the one specific to apollo.
# TODO: The move is contingent on understanding the functions that should be
# supported by the dut device (sec_device).

"""A generic library with bluetooth related functions. The connection is assumed
to be between and android phone with any dut (referred to as secondary device)
device that supports the following calls:
        sec_device.turn_on_bluetooth()
        sec_device.is_bt_enabled():
        sec_device.bluetooth_address
        sec_device.set_pairing_mode()
        sec_device.factory_reset()

"""
import queue
import time
from logging import Logger

from acts import asserts
from acts.controllers.buds_lib import tako_trace_logger
from acts.utils import TimeoutError
from acts.utils import wait_until

# Add connection profile for future devices in this dictionary
WEARABLE_BT_PROTOCOLS = {
    'rio': {
        'Comp. App': 'FALSE',
        'HFP (pri.)': 'FALSE',
        'HFP (sec.)': 'FALSE',
        'A2DP (pri.)': 'FALSE',
        'A2DP (sec.)': 'FALSE',
    },
    'apollo': {
        'Comp': 'FALSE',
        'HFP(pri.)': 'FALSE',
        'HFP(sec.)': 'FALSE',
        'A2DP(pri)': 'FALSE',
        'A2DP(sec)': 'FALSE',
    }
}


class BTUtilsError(Exception):
    """Generic BTUtils error"""


class BTUtils(object):
    """A utility that provides access to bluetooth controls.

    This class to be maintained as a generic class such that it is compatible
    with any devices that pair with a phone.
    """

    def __init__(self):
        self.default_timeout = 60
        self.logger = tako_trace_logger.TakoTraceLogger(Logger(__file__))

    def bt_pair_and_connect(self, pri_device, sec_device):
        """Pair and connect a pri_device to a sec_device.

        Args:
        pri_device: an android device with sl4a installed.
        sec_device: a wearable device.

        Returns:
        (Tuple)True if pair and connect successful. False Otherwise.
        Time in ms to execute the flow.
        """

        pair_time = self.bt_pair(pri_device, sec_device)
        connect_result, connect_time = self.bt_connect(pri_device, sec_device)
        return connect_result, pair_time + connect_time

    def bt_pair(self, pri_device, sec_device):
        """Pair a pri_device to a sec_device.

        Args:
        pri_device: an android device with sl4a installed.
        sec_device: a wearable device.

        Returns:
            (Tuple)True if pair successful. False Otherwise.
            Time in ms to execute the flow.
         """
        start_time = time.time()
        # Enable BT on the primary device if it's not currently ON.
        if not pri_device.droid.bluetoothCheckState():
            pri_device.droid.bluetoothToggleState(True)
            try:
                pri_device.ed.pop_event(event_name='BluetoothStateChangedOn',
                                        timeout=10)
            except queue.Empty:
                raise BTUtilsError(
                    'Failed to toggle Bluetooth on the primary device.')
        sec_device.turn_on_bluetooth()
        if not sec_device.is_bt_enabled():
            raise BTUtilsError('Could not turn on Bluetooth on secondary '
                               'devices')
        target_addr = sec_device.bluetooth_address
        sec_device.set_pairing_mode()

        pri_device.droid.bluetoothDiscoverAndBond(target_addr)
        # Loop until we have bonded successfully or timeout.
        self.logger.info('Verifying devices are bonded')
        try:
            wait_until(lambda: self.android_device_in_paired_state(pri_device,
                                                                   target_addr),
                       self.default_timeout)
        except TimeoutError as err:
            raise BTUtilsError('bt_pair failed: {}'.format(err))
        end_time = time.time()
        return end_time - start_time

    def bt_connect(self, pri_device, sec_device):
        """Connect a previously paired sec_device to a pri_device.

        Args:
          pri_device: an android device with sl4a installed.
          sec_device: a wearable device.

        Returns:
          (Tuple)True if connect successful. False otherwise.
          Time in ms to execute the flow.
        """
        start_time = end_time = time.time()
        target_addr = sec_device.bluetooth_address
        # First check that devices are bonded.
        paired = False
        for paired_device in pri_device.droid.bluetoothGetBondedDevices():
            if paired_device['address'] == target_addr:
                paired = True
                break
        if not paired:
            self.logger.error('Not paired to %s', sec_device.device_name)
            return False, 0

        self.logger.info('Attempting to connect.')
        pri_device.droid.bluetoothConnectBonded(target_addr)

        self.logger.info('Verifying devices are connected')
        wait_until(
            lambda: self.android_device_in_connected_state(pri_device,
                                                           target_addr),
            self.default_timeout)
        end_time = time.time()
        return True, end_time - start_time

    def android_device_in_paired_state(self, device, mac_address):
        """Check device in paired list."""
        bonded_devices = device.droid.bluetoothGetBondedDevices()
        for d in bonded_devices:
            if d['address'] == mac_address:
                self.logger.info('Successfully bonded to device')
                return True
        return False

    def android_device_in_connected_state(self, device, mac_address):
        """Check device in connected list."""
        connected_devices = device.droid.bluetoothGetConnectedDevices()
        for d in connected_devices:
            if d['address'] == mac_address:
                self.logger.info('Successfully connected to device')
                return True
        return False

    def bt_unpair(self, pri_device, sec_device, factory_reset_dut=True):
        """Unpairs two Android devices using bluetooth.

        Args:
          pri_device: an android device with sl4a installed.
          sec_device: a wearable device.

        Returns:
          (Tuple)True: if the devices successfully unpaired.
          Time in ms to execute the flow.
        Raises:
          Error: When devices fail to unpair.
        """
        target_address = sec_device.bluetooth_address
        if not self.android_device_in_paired_state(pri_device, target_address):
            self.logger.debug('Already unpaired.')
            return True, 0
        self.logger.debug('Unpairing from %s' % target_address)
        start_time = end_time = time.time()
        asserts.assert_true(
            pri_device.droid.bluetoothUnbond(target_address),
            'Failed to request device unpairing.')

        # Check that devices have unpaired successfully.
        self.logger.debug('Verifying devices are unpaired')

        # Loop until we have unbonded successfully or timeout.
        wait_until(
            lambda: self.android_device_in_paired_state(pri_device,
                                                        target_address),
            self.default_timeout,
            condition=False)

        self.logger.info('Successfully unpaired from %s' % target_address)
        if factory_reset_dut:
            self.logger.info('Factory reset DUT')
            sec_device.factory_reset()
        end_time = time.time()
        return True, end_time - start_time

    def check_device_bt(self, device, **kwargs):
        """Check the Bluetooth connection status from device.

        Args:
          device: a wearable device.
          **kwargs: additional parameters

        Returns:
          True: if bt status check success, False otherwise.
        """
        if device.dut_type in ['rio', 'apollo']:
            profiles = kwargs.get('profiles')
            return self.check_dut_status(device, profiles)

    def check_dut_status(self, device, profiles=None):
        """Check the Bluetooth connection status from rio/apollo device.

        Args:
          device: rio/apollo device
          profiles: A dict of profiles, eg. {'HFP (pri.)': 'TRUE', 'Comp. App':
            'TRUE', 'A2DP (pri.)': 'TRUE'}

        Returns:
          True: if bt status check success, False otherwise.
        """
        expected = WEARABLE_BT_PROTOCOLS
        self.logger.info(profiles)
        for key in profiles:
            expected[device.dut_type][key] = profiles[key]
        try:
            wait_until(lambda: self._compare_profile(device,
                                                     expected[device.dut_type]),
                       self.default_timeout)
        except TimeoutError:
            status = device.get_bt_status()
            msg_fmt = self._get_formatted_output(expected[device.dut_type],
                                                 status)
            self.logger.error(msg_fmt)
            return False
        return True

    def _get_formatted_output(self, expected, actual):
        """On BT status mismatch generate formatted output string.

        Args:
          expected: Expected BT status hash.
          actual: Actual BT status hash from Rio.

        Returns:
          Formatted mismatch string.

        Raises:
          Error: When unexpcted parameter encounterd.
        """
        msg = ''
        mismatch_format = '{}: Expected {} Actual {}. '
        if actual is None:
            raise BTUtilsError('None is not expected.')
        for key in expected.keys():
            if expected[key] != actual[key]:
                msg += mismatch_format.format(key, expected[key], actual[key])
        return msg

    def _compare_profile(self, device, expected):
        """Compare input expected profile with actual."""
        actual = device.get_bt_status()
        if actual is None:
            raise BTUtilsError('None is not expected.')
        for key in expected.keys():
            if expected[key] != actual[key]:
                return False
        return True
