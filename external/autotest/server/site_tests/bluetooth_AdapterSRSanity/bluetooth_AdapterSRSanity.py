# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Server-side bluetooth adapter tests that involve suspend/resume with peers

paired and/or connected.

Single chameleon tests:
  - Reconnect on resume test
    - Classic HID
    - LE HID
    - A2DP
  - Wake from suspend test
    - Classic HID
    - LE HID
    - A2DP shouldn't wake from suspend
  - Suspend while discovering (discovering should pause and unpause)
  - Suspend while advertising (advertising should pause and unpause)

Multiple chameleon tests:
  - Reconnect on resume test
    - One classic HID, One LE HID
    - Two classic HID
    - Two LE HID
  - Wake from suspend test
    - Two classic HID
    - Two classic LE
"""

from datetime import datetime, timedelta
import logging
import multiprocessing
import threading
import time

from autotest_lib.server.cros.bluetooth import bluetooth_adapter_tests
from autotest_lib.server.cros.bluetooth.bluetooth_adapter_quick_tests import \
     BluetoothAdapterQuickTests

test_wrapper = BluetoothAdapterQuickTests.quick_test_test_decorator
batch_wrapper = BluetoothAdapterQuickTests.quick_test_batch_decorator
test_retry_and_log = bluetooth_adapter_tests._test_retry_and_log

SHORT_SUSPEND = 10
MED_SUSPEND = 20
LONG_SUSPEND = 30

RESUME_DELTA = 5

class bluetooth_AdapterSRSanity(
        BluetoothAdapterQuickTests,
        bluetooth_adapter_tests.BluetoothAdapterTests):
    """Server side bluetooth adapter suspend resume test with peer."""

    def _suspend_async(self,
                       suspend_time=SHORT_SUSPEND,
                       allow_early_resume=False):
        """ Suspend asynchronously and return process for joining

        @param suspend_time: how long to stay in suspend
        @param allow_early_resume: are we expecting to wake up earlier
        @returns multiprocessing.Process object with suspend task
        """

        def _action_suspend():
            self.host.suspend(
                suspend_time=suspend_time,
                allow_early_resume=allow_early_resume)

        proc = multiprocessing.Process(target=_action_suspend)
        proc.daemon = True
        return proc

    def _device_connect_async(self, device_type, device, adapter_address):
        """ Connects peer device asynchronously with DUT.

        This function uses a thread instead of a subprocess so that the test
        result is stored for the test. Otherwise, the test connection was
        sometimes failing but the test itself was passing.

        @param device_type: The device type (used to check if it's LE)
        @param device: the meta device with the peer device
        @param adapter_address: the address of the adapter

        @returns threading.Thread object with device connect task
        """

        def _action_device_connect():
            time.sleep(1)
            if 'BLE' in device_type:
                # LE reconnects by advertising (dut controller will create LE
                # connection, not the peer device)
                self.test_device_set_discoverable(device, True)
            else:
                # Classic requires peer to initiate a connection to wake up the
                # dut
                self.test_connection_by_device_only(device, adapter_address)

        thread = threading.Thread(target=_action_device_connect)
        return thread

    @test_retry_and_log(False)
    def suspend_and_wait_for_sleep(self, suspend):
        """ Suspend the device and wait until it is sleeping.

        @param suspend: Sub-process that does the actual suspend call.

        @return True if host is asleep within a short timeout, False otherwise.
        """
        suspend.start()
        try:
            self.host.test_wait_for_sleep(sleep_timeout=SHORT_SUSPEND)
        except:
            suspend.join()
            return False

        return True

    @test_retry_and_log(False)
    def wait_for_resume(self, boot_id, suspend, resume_timeout=SHORT_SUSPEND):
        """ Wait for device to resume from suspend.

        @param boot_id: Current boot id
        @param suspend: Sub-process that does actual suspend call.
        @param resume_timeout: Expect device to resume in given timeout.

        @raises if timeout triggers during resume

        @return True if suspend sub-process completed without error.
        """
        success = True

        # Sometimes it takes longer to resume from suspend; give some leeway
        resume_timeout = resume_timeout + RESUME_DELTA
        try:
            start = datetime.now()
            self.host.test_wait_for_resume(
                boot_id, resume_timeout=resume_timeout)

            delta = datetime.now() - start

            # TODO(abhishekpandit) - Figure out why test_wait_for_resume isn't
            #                        timing out
            if delta > timedelta(seconds=resume_timeout):
                success = False
        except Exception as e:
            success = False
            logging.error("wait_for_resume: %s", e)
        finally:
            suspend.join()
            self.results = {
                "resume_success": success,
                "suspend_result": suspend.exitcode == 0
            }

            return all(self.results.values())

    def test_discover_and_pair(self, device):
        """ Discovers and pairs given device. Automatically connects too."""
        self.test_device_set_discoverable(device, True)
        self.test_discover_device(device.address)
        self.bluetooth_facade.stop_discovery()
        self.test_pairing(device.address, device.pin, trusted=True)

    def _test_keyboard_with_string(self, device):
        self.test_keyboard_input_from_trace(device, "simple_text")

    # ---------------------------------------------------------------
    # Reconnect after suspend tests
    # ---------------------------------------------------------------

    def run_reconnect_device(self, devtuples):
        """ Reconnects a device after suspend/resume.

        @param devtuples: array of tuples consisting of the following
                            * device_type: MOUSE, BLE_MOUSE, etc.
                            * device: meta object for peer device
                            * device_test: Optional; test function to run w/
                                           device (for example, mouse click)
        """
        boot_id = self.host.get_boot_id()
        suspend = self._suspend_async()

        try:
            for _, device, device_test in devtuples:
                self.test_discover_and_pair(device)
                self.test_device_set_discoverable(device, False)
                self.test_connection_by_adapter(device.address)

            # Trigger suspend, wait for regular resume, verify we can reconnect
            # and run device specific test
            self.suspend_and_wait_for_sleep(suspend)
            self.wait_for_resume(boot_id, suspend, resume_timeout=SHORT_SUSPEND)

            for device_type, device, device_test in devtuples:
                if 'BLE' in device_type:
                    # LE can't reconnect without advertising/discoverable
                    self.test_device_set_discoverable(device, True)

                # Test that host sees connection complete
                self.test_connection_by_device(device)
                if device_test is not None:
                    device_test(device)

        finally:
            for _, device, __ in devtuples:
                self.test_remove_pairing(device.address)

    @test_wrapper('Reconnect Classic HID', devices={'MOUSE': 1})
    def sr_reconnect_classic_hid(self):
        """ Reconnects a classic HID device after suspend/resume. """
        device_type = 'MOUSE'
        device = self.devices[device_type][0]
        self.run_reconnect_device([(device_type, device,
                                    self.test_mouse_left_click)])

    @test_wrapper('Reconnect LE HID', devices={'BLE_MOUSE': 1})
    def sr_reconnect_le_hid(self):
        """ Reconnects a LE HID device after suspend/resume. """
        device_type = 'BLE_MOUSE'
        device = self.devices[device_type][0]
        self.run_reconnect_device([(device_type, device,
                                    self.test_mouse_left_click)])

    @test_wrapper('Reconnect A2DP', devices={})
    def sr_reconnect_a2dp(self):
        """ Reconnects an A2DP device after suspend/resume. """
        raise NotImplementedError()

    @test_wrapper('Reconnect Multiple Classic HID',
                   devices={'MOUSE': 1, 'KEYBOARD': 1})
    def sr_reconnect_multiple_classic_hid(self):
        """ Reconnects multiple classic HID devices after suspend/resume. """
        devices = [
                ('MOUSE', self.devices['MOUSE'][0], self.test_mouse_left_click),
                ('KEYBOARD', self.devices['KEYBOARD'][0],
                    self._test_keyboard_with_string)
        ]
        self.run_reconnect_device(devices)

    @test_wrapper('Reconnect Multiple LE HID',
                  devices={'BLE_MOUSE': 1, 'BLE_KEYBOARD': 1})
    def sr_reconnect_multiple_le_hid(self):
        """ Reconnects multiple LE HID devices after suspend/resume. """
        devices = [
                ('BLE_MOUSE', self.devices['BLE_MOUSE'][0],
                 self.test_mouse_left_click),
                ('BLE_KEYBOARD', self.devices['BLE_KEYBOARD'][0],
                 self._test_keyboard_with_string)
        ]
        self.run_reconnect_device(devices)

    @test_wrapper(
        'Reconnect one of each classic+LE HID',
        devices={
            'BLE_MOUSE': 1,
            'KEYBOARD': 1
        })
    def sr_reconnect_multiple_classic_le_hid(self):
        """ Reconnects one of each classic and LE HID devices after
            suspend/resume.
        """
        devices = [
                ('BLE_MOUSE', self.devices['BLE_MOUSE'][0],
                 self.test_mouse_left_click),
                ('KEYBOARD', self.devices['KEYBOARD'][0],
                 self._test_keyboard_with_string)
        ]
        self.run_reconnect_device(devices)

    # ---------------------------------------------------------------
    # Wake from suspend tests
    # ---------------------------------------------------------------

    def run_peer_wakeup_device(self, device_type, device, device_test=None):
        """ Uses paired peer device to wake the device from suspend.

        @param device_type: the device type (used to determine if it's LE)
        @param device: the meta device with the paired device
        @param device_test: What to test to run after waking and connecting the
                            adapter/host
        """
        boot_id = self.host.get_boot_id()
        suspend = self._suspend_async(
            suspend_time=LONG_SUSPEND, allow_early_resume=True)

        try:
            self.test_discover_and_pair(device)
            self.test_device_set_discoverable(device, False)

            adapter_address = self.bluetooth_facade.address

            # TODO(abhishekpandit) - Wait for powerd event that marks the
            # power/wakeup of hci0's parent to "enabled"; otherwise, we won't be
            # able to wake
            time.sleep(3)

            # Trigger suspend, asynchronously trigger wake and wait for resume
            self.suspend_and_wait_for_sleep(suspend)

            # Trigger peer wakeup
            peer_wake = self._device_connect_async(device_type, device,
                                                   adapter_address)
            peer_wake.start()

            # Expect a quick resume
            self.wait_for_resume(boot_id, suspend, resume_timeout=SHORT_SUSPEND)

            # Finish peer wake process
            peer_wake.join()

            if device_test is not None:
                device_test(device)

        finally:
            self.test_remove_pairing(device.address)

    @test_wrapper('Peer wakeup Classic HID', devices={'MOUSE': 1})
    def sr_peer_wake_classic_hid(self):
        """ Use classic HID device to wake from suspend. """
        device = self.devices['MOUSE'][0]
        self.run_peer_wakeup_device(
            'MOUSE', device, device_test=self.test_mouse_left_click)

    @test_wrapper('Peer wakeup LE HID', devices={'BLE_MOUSE': 1})
    def sr_peer_wake_le_hid(self):
        """ Use LE HID device to wake from suspend. """
        device = self.devices['BLE_MOUSE'][0]
        self.run_peer_wakeup_device(
            'BLE_MOUSE', device, device_test=self.test_mouse_left_click)

    @test_wrapper('Peer wakeup with A2DP should fail')
    def sr_peer_wake_a2dp_should_fail(self):
        """ Use A2DP device to wake from suspend and fail. """
        raise NotImplementedError()

    # ---------------------------------------------------------------
    # Suspend while discovering and advertising
    # ---------------------------------------------------------------

    @test_wrapper('Suspend while discovering', devices={'BLE_MOUSE': 1})
    def sr_while_discovering(self):
        """ Suspend while discovering. """
        device = self.devices['BLE_MOUSE'][0]
        boot_id = self.host.get_boot_id()
        suspend = self._suspend_async(
            suspend_time=SHORT_SUSPEND, allow_early_resume=False)

        # We don't pair to the peer device because we don't want it in the
        # whitelist. However, we want an advertising peer in this test
        # responding to the discovery requests.
        self.test_device_set_discoverable(device, True)

        self.test_start_discovery()
        self.suspend_and_wait_for_sleep(suspend)

        # If discovery events wake us early, we will raise and suspend.exitcode
        # will be non-zero
        self.wait_for_resume(boot_id, suspend, resume_timeout=SHORT_SUSPEND)

        # Discovering should restore after suspend
        self.test_is_discovering()

        self.test_stop_discovery()

    @test_wrapper('Suspend while advertising', devices={'MOUSE': 1})
    def sr_while_advertising(self):
        """ Suspend while advertising. """
        device = self.devices['MOUSE'][0]
        boot_id = self.host.get_boot_id()
        suspend = self._suspend_async(
            suspend_time=MED_SUSPEND, allow_early_resume=False)

        self.test_discoverable()
        self.suspend_and_wait_for_sleep(suspend)

        # Peer device should not be able to discover us in suspend
        self.test_discover_by_device_fails(device)

        self.wait_for_resume(boot_id, suspend, resume_timeout=MED_SUSPEND)

        # Test that we are properly discoverable again
        self.test_is_discoverable()
        self.test_discover_by_device(device)

        self.test_nondiscoverable()

    @batch_wrapper('SR with Peer Sanity')
    def sr_sanity_batch_run(self, num_iterations=1, test_name=None):
        """ Batch of suspend/resume peer sanity tests. """
        self.sr_reconnect_classic_hid()
        self.sr_reconnect_le_hid()
        self.sr_peer_wake_classic_hid()
        self.sr_peer_wake_le_hid()
        self.sr_while_discovering()
        self.sr_while_advertising()

    def run_once(self,
                 host,
                 num_iterations=1,
                 test_name=None,
                 flag='Quick Sanity'):
        """Running Bluetooth adapter suspend resume with peer autotest.

        @param host: the DUT, usually a chromebook
        @param num_iterations: the number of times to execute the test
        @param test_name: the test to run or None for all tests
        @param flag: run tests with this flag (default: Quick Sanity)

        """

        # Initialize and run the test batch or the requested specific test
        self.quick_test_init(host, use_chameleon=True, flag=flag)
        self.sr_sanity_batch_run(num_iterations, test_name)
        self.quick_test_cleanup()
