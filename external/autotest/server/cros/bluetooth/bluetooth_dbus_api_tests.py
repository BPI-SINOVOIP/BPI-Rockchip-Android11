# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Bluetooth DBus API tests."""


import logging

from autotest_lib.server.cros.bluetooth import bluetooth_adapter_tests

# Assigning local names for some frequently used long method names.
method_name = bluetooth_adapter_tests.method_name
_test_retry_and_log = bluetooth_adapter_tests._test_retry_and_log

DEFAULT_START_DELAY_SECS = 2
DEFAULT_HOLD_INTERVAL = 10
DEFAULT_HOLD_TIMEOUT = 60

# String representation of DBus exceptions
DBUS_ERRORS  = {
    'InProgress' : 'org.bluez.Error.InProgress: Operation already in progress',
    'NotReady' : 'org.bluez.Error.NotReady: Resource Not Ready',
    'Failed': {
        'discovery_start' : 'org.bluez.Error.Failed: No discovery started',
        'discovery_unpause' : 'org.bluez.Error.Failed: Discovery not paused'
              }
               }


class BluetoothDBusAPITests(bluetooth_adapter_tests.BluetoothAdapterTests):
    """Bluetooth DBus API Test

       These test verifies return values and functionality of various Bluetooth
       DBus APIs. It tests both success and failures cases of each API. It
       checks the following
       - Expected return value
       - Expected exceptions for negative cases
       - Expected change in Dbus variables
       - TODO Expected change in (hci) state of the adapter
    """

    def _reset_state(self):
        """ Reset adapter to a known state.
        These tests changes adapter state. This function resets the adapter
        to known state

        @returns True if reset was successful False otherwise

        """
        logging.debug("resetting state of the adapter")
        power_off = self._wait_till_power_off()
        power_on = self._wait_till_power_on()
        not_discovering = self._wait_till_discovery_stops()
        # unpause_discovery will fail if discovery is not paused
        self.bluetooth_facade.unpause_discovery(False)
        reset_results = {'power_off' : power_off,
                         'power_on' : power_on,
                         'not_discovering' : not_discovering}
        if not all(reset_results.values()):
            logging.error('_reset_state failed %s',reset_results)
            return False
        else:
            return True

    def _compare_error(self, expected, actual):
        """ Helper function to compare error and log. """
        if expected == actual:
            return True
        else:
            logging.debug("Expected error is %s Actual error is %s",expected,
                          actual)
            return False

    def _get_hci_state(self, msg=''):
        """ get state of bluetooth controller. """
        hci_state = self.log_flags(msg, self.get_dev_info()[3])
        logging.debug("hci_state is %s", hci_state)
        return hci_state

    def _wait_till_hci_state_inquiry(self):
        """ Wait till adapter is in INQUIRY state.

        @return: True if adapter does INQUIRY before timeout, False otherwise
        """
        return self._wait_for_condition(
            lambda: 'INQUIRY' in self._get_hci_state('Expecting INQUIRY'),
            method_name(),
            start_delay = DEFAULT_START_DELAY_SECS)

    def _wait_till_hci_state_no_inquiry_holds(self):
        """ Wait till adapter does not enter INQUIRY for a period of time

        @return : True if adapter is not in INQUIRY for a period of time before
                  timeout. Otherwise False.
        """
        return self._wait_till_condition_holds(
            lambda: 'INQUIRY' not in self._get_hci_state('Expecting NOINQUIRY'),
            method_name(),
            hold_interval = DEFAULT_HOLD_INTERVAL,
            timeout = DEFAULT_HOLD_TIMEOUT,
            start_delay = DEFAULT_START_DELAY_SECS)



    def _wait_till_discovery_stops(self, stop_discovery=True):
        """stop discovery if specified and wait for discovery to stop

        @params: stop_discovery: Specifies whether stop_discovery should be
                 executed
        @returns: True if discovery is stopped
        """
        if stop_discovery:
            self.bluetooth_facade.stop_discovery()
        is_not_discovering = self._wait_for_condition(
            lambda: not self.bluetooth_facade.is_discovering(),
            method_name())
        return is_not_discovering

    def _wait_till_discovery_starts(self, start_discovery=True):
        """start discovery if specified and wait for discovery to start

        @params: start_discovery: Specifies whether start_discovery should be
                 executed
        @returns: True if discovery is started
        """

        if start_discovery:
            self.bluetooth_facade.start_discovery()
        is_discovering = self._wait_for_condition(
            self.bluetooth_facade.is_discovering, method_name())
        return is_discovering

    def _wait_till_power_off(self):
        """power off the adapter and wait for it to be powered off

        @returns: True if adapter can be powered off
        """

        power_off = self.bluetooth_facade.set_powered(False)
        is_powered_off = self._wait_for_condition(
                lambda: not self.bluetooth_facade.is_powered_on(),
                method_name())
        return is_powered_off

    def _wait_till_power_on(self):
        """power on the adapter and wait for it to be powered on

        @returns: True if adapter can be powered on
        """
        power_on = self.bluetooth_facade.set_powered(True)
        is_powered_on = self._wait_for_condition(
            self.bluetooth_facade.is_powered_on, method_name())
        return is_powered_on


########################################################################
# dbus call : start_discovery
#
#####################################################
# Positive cases
# Case 1
# preconditions: Adapter powered on AND
#                Currently not discovering
# result: Success
######################################################
# Negative cases
#
# Case 1
# preconditions: Adapter powered off
# result: Failure
# error : NotReady
#
# Case 2
# precondition: Adapter power on AND
#               Currently discovering
# result: Failure
# error: Inprogress
#########################################################################

    @_test_retry_and_log(False)
    def test_dbus_start_discovery_success(self):
        """ Test success case of start_discovery call. """
        reset = self._reset_state()
        is_power_on = self._wait_till_power_on()
        is_not_discovering = self._wait_till_discovery_stops()

        start_discovery, error =  self.bluetooth_facade.start_discovery()

        is_discovering = self._wait_till_discovery_starts(start_discovery=False)
        inquiry_state = self._wait_till_hci_state_inquiry()

        self.results = {'reset' : reset,
                        'is_power_on' : is_power_on,
                        'is_not_discovering': is_not_discovering,
                        'start_discovery' : start_discovery,
                        'is_discovering': is_discovering,
                        'inquiry_state' : inquiry_state
                        }
        return all(self.results.values())

    @_test_retry_and_log(False)
    def test_dbus_start_discovery_fail_discovery_in_progress(self):
        """ Test Failure case of start_discovery call.

        start discovery when discovery is in progress and confirm it fails with
        'org.bluez.Error.InProgress: Operation already in progress'.
        """
        reset = self._reset_state()
        is_discovering = self._wait_till_discovery_starts()

        start_discovery, error =  self.bluetooth_facade.start_discovery()


        self.results = {'reset' : reset,
                        'is_discovering' : is_discovering,
                        'start_discovery_failed' : not start_discovery,
                        'error_matches' : self._compare_error(error,
                                                    DBUS_ERRORS['InProgress'])
        }
        return all(self.results.values())

    @_test_retry_and_log(False)
    def test_dbus_start_discovery_fail_power_off(self):
        """ Test Failure case of start_discovery call.

        start discovery when adapter is turned off and confirm it fails with
        'NotReady' : 'org.bluez.Error.NotReady: Resource Not Ready'.
        """
        reset = self._reset_state()
        is_power_off = self._wait_till_power_off()

        start_discovery, error =  self.bluetooth_facade.start_discovery()

        is_power_on = self._wait_till_power_on()
        self.results = {'reset' : reset,
                        'power_off' : is_power_off,
                        'start_discovery_failed' : not start_discovery,
                        'error_matches' : self._compare_error(error,
                                                    DBUS_ERRORS['NotReady']),
                        'power_on' : is_power_on}
        return all(self.results.values())


########################################################################
# dbus call : stop_discovery
#
#####################################################
# Positive cases
# Case 1
# preconditions: Adapter powered on AND
#                Currently discovering
# result: Success
#####################################################
# Negative cases
#
# Case 1
# preconditions: Adapter powered off
# result: Failure
# error : NotReady
#
# Case 2
# precondition: Adapter power on AND
#               Currently not discovering
# result: Failure
# error: Failed
#
#TODO
#Case 3  org.bluez.Error.NotAuthorized
#########################################################################

    @_test_retry_and_log(False)
    def test_dbus_stop_discovery_success(self):
        """ Test success case of stop_discovery call. """
        reset = self._reset_state()
        is_power_on = self._wait_till_power_on()
        is_discovering = self._wait_till_discovery_starts()

        stop_discovery, error =  self.bluetooth_facade.stop_discovery()
        is_not_discovering = self._wait_till_discovery_stops(
            stop_discovery=False)
        self._wait_till_hci_state_no_inquiry_holds()
        self.results = {'reset' : reset,
                        'is_power_on' : is_power_on,
                        'is_discovering': is_discovering,
                        'stop_discovery' : stop_discovery,
                        'is_not_discovering' : is_not_discovering}
        return all(self.results.values())

    @_test_retry_and_log(False)
    def test_dbus_stop_discovery_fail_discovery_not_in_progress(self):
        """ Test Failure case of stop_discovery call.

        stop discovery when discovery is not in progress and confirm it fails
        with 'org.bluez.Error.Failed: No discovery started'.
        """
        reset = self._reset_state()
        is_not_discovering = self._wait_till_discovery_stops()

        stop_discovery, error =  self.bluetooth_facade.stop_discovery()

        still_not_discovering = self._wait_till_discovery_stops(
            stop_discovery=False)

        self.results = {
            'reset' : reset,
            'is_not_discovering' : is_not_discovering,
            'stop_discovery_failed' : not stop_discovery,
            'error_matches' : self._compare_error(error,
                                DBUS_ERRORS['Failed']['discovery_start']),
            'still_not_discovering': still_not_discovering}
        return all(self.results.values())

    @_test_retry_and_log(False)
    def test_dbus_stop_discovery_fail_power_off(self):
        """ Test Failure case of stop_discovery call.

        stop discovery when adapter is turned off and confirm it fails with
        'NotReady' : 'org.bluez.Error.NotReady: Resource Not Ready'.
        """
        reset = self._reset_state()
        is_power_off = self._wait_till_power_off()

        stop_discovery, error =  self.bluetooth_facade.stop_discovery()

        is_power_on = self._wait_till_power_on()
        self.results = {'reset' : reset,
                        'is_power_off' : is_power_off,
                        'stop_discovery_failed' : not stop_discovery,
                        'error_matches' : self._compare_error(error,
                                                    DBUS_ERRORS['NotReady']),
                        'is_power_on' : is_power_on}
        return all(self.results.values())


########################################################################
# dbus call: pause_discovery
# arguments: boolean system_suspend_resume
# returns : True/False
# Notes: 1: argument system_suspend_resume is ignored in the code
#        2: pause/unpause state is not reflected in Discovering state
#####################################################
# Positive cases
# Case 1
# preconditions: Adapter powered on AND
#                Currently discovering
# Argument: [True|False]
# result: Success
# Case 2
# preconditions: Adapter powered on AND
#                Currently not discovering
# Argument: [True|False]
# result: Success
######################################################
# Negative cases
#
# Case 1
# preconditions: Adapter powered off
# result: Failure
# error : NotReady
# postconditions: Discovery can be started after power on
#
# Case 2
# precondition: Adapter powered on AND
#               Discovery paused
# result: Failure
# error: Busy
#########################################################################

    @_test_retry_and_log(False)
    def test_dbus_pause_discovery_success(self):
        """ Test success case of pause_discovery call. """
        reset = self._reset_state()
        is_discovering = self._wait_till_discovery_starts()
        self._wait_till_hci_state_inquiry()

        pause_discovery, error = self.bluetooth_facade.pause_discovery(False)

        no_inquiry = self._wait_till_hci_state_no_inquiry_holds()
        self.results = {'reset' : reset,
                        'is_discovering': is_discovering,
                        'pause_discovery' : pause_discovery,
                        'no_inquiry' : no_inquiry
                        }
        return all(self.results.values())

    @_test_retry_and_log(False)
    def test_dbus_pause_discovery_success_no_discovery_in_progress(self):
        """ Test success case of pause_discovery call. """
        reset = self._reset_state()
        is_power_on = self._wait_till_power_on()
        is_not_discovering = self._wait_till_discovery_stops()

        pause_discovery, error = self.bluetooth_facade.pause_discovery(False)

        no_inquiry = self._wait_till_hci_state_no_inquiry_holds()
        self.results = {'reset' : reset,
                        'is_power_on' : is_power_on,
                        'is_not_discovering': is_not_discovering,
                        'pause_discovery' : pause_discovery,
                        'no_inquiry' : no_inquiry
                        }
        return all(self.results.values())

    @_test_retry_and_log(False)
    def test_dbus_pause_discovery_fail_power_off(self):
        """ Test Failure case of pause_discovery call.

        pause discovery when adapter is turned off and confirm it fails with
        'NotReady' : 'org.bluez.Error.NotReady: Resource Not Ready'.
        Also check we are able to start discovery after power on
        """
        reset = self._reset_state()
        is_power_off = self._wait_till_power_off()

        pause_discovery, error = self.bluetooth_facade.pause_discovery()

        is_power_on = self._wait_till_power_on()
        discovery_started = self._wait_till_discovery_starts()

        self.results = {'reset' : reset,
                        'is_power_off' : is_power_off,
                        'pause_discovery_failed' : not pause_discovery,
                        'error_matches' : self._compare_error(error,
                                                    DBUS_ERRORS['NotReady']),
                        'is_power_on' : is_power_on,
                        'discovery_started' : discovery_started
                       }
        return all(self.results.values())

    @_test_retry_and_log(False)
    def test_dbus_pause_discovery_fail_already_paused(self):
        """ Test failure case of pause_discovery call.

        Call pause discovery twice and make sure second call fails
        with 'org.bluez.Error.InProgress: Operation already in progress'.
        """
        reset = self._reset_state()
        is_power_on = self._wait_till_power_on()
        is_discovering = self._wait_till_discovery_starts()
        pause_discovery, _ = self.bluetooth_facade.pause_discovery()

        pause_discovery_again, error = self.bluetooth_facade.pause_discovery()

        no_inquiry = self._wait_till_hci_state_no_inquiry_holds()

        self.results = {'reset' : reset,
                        'is_power_on' : is_power_on,
                        'is_discovering': is_discovering,
                        'pause_discovery' : pause_discovery,
                        'pause_discovery_failed' : not pause_discovery_again,
                        'error_matches' : self._compare_error(error,
                                                    DBUS_ERRORS['InProgress']),
                        'no_inquiry' : no_inquiry,
                        }
        return all(self.results.values())

########################################################################
# dbus call: unpause_discovery
# arguments: boolean system_suspend_resume
# returns : True/False
# Notes: 1: argument system_suspend_resume is ignored in the code
#        2: pause/unpause state is not reflected in Discovering state
#####################################################
# Positive cases
# Case 1
# preconditions: Adapter powered on AND
#                Discovery started and Discovery currently paused
# Argument: [True|False]
######################################################
# Negative cases
#
# result: Success
# Case 1
# preconditions: Adapter powered on AND
#                Discovery currently not paused
# Argument: [True|False]
# result: Failed
#
# Case 2
# preconditions: Adapter powered off
# result: Failure
# error : NotReady
#
# Case 3
# precondition: Adapter powered on AND
#               Discovery paused
# result: Failure
# error: Busy
#########################################################################
    @_test_retry_and_log(False)
    def test_dbus_unpause_discovery_success(self):
        """ Test success case of unpause_discovery call. """
        reset = self._reset_state()
        is_discovering = self._wait_till_discovery_starts()
        pause_discovery, _ = self.bluetooth_facade.pause_discovery()
        no_inquiry_after_pause = self._wait_till_hci_state_no_inquiry_holds()

        unpause_discovery, error = self.bluetooth_facade.unpause_discovery()

        inquiry_after_unpause = self._wait_till_hci_state_inquiry()
        self.results = {'reset' : reset,
                        'is_discovering': is_discovering,
                        'pause_discovery' : pause_discovery,
                        'no_inquiry_after_pause' : no_inquiry_after_pause,
                        'unpause_discovery' : unpause_discovery,
                        'error' : error is None,
                        'inquiry_after_unpause' : inquiry_after_unpause
                        }
        return all(self.results.values())

    @_test_retry_and_log(False)
    def test_dbus_unpause_discovery_fail_without_pause(self):
        """ Test failure case of unpause_discovery call.

        Call unpause_discovery without calling pause_discovery and check it will
        fail with  org.bluez.Error.Failed: Discovery not paused'
        """
        reset = self._reset_state()
        is_discovering = self._wait_till_discovery_starts()

        unpause_discovery, error = self.bluetooth_facade.unpause_discovery()

        inquiry_after_unpause = self._wait_till_hci_state_inquiry()
        self.results = {'reset' : reset,
                        'is_discovering': is_discovering,
                        'unpause_discovery_fails' : not unpause_discovery,
                        'error' : self._compare_error(error,
                                    DBUS_ERRORS['Failed']['discovery_unpause']),
                        'inquiry_after_unpause' : inquiry_after_unpause
                        }
        return all(self.results.values())

    @_test_retry_and_log(False)
    def test_dbus_unpause_discovery_fail_power_off(self):
        """ Test Failure case of unpause_discovery call.

        unpause discovery when adapter is turned off and confirm it fails with
         'org.bluez.Error.Failed: Discovery not paused'

        """
        reset = self._reset_state()
        is_power_off = self._wait_till_power_off()

        unpause_discovery, error = self.bluetooth_facade.unpause_discovery()

        self.results = {'reset' : reset,
                        'is_power_off' : is_power_off,
                        'unpause_discovery_failed' : not unpause_discovery,
                        'error_matches' : self._compare_error(error,
                                    DBUS_ERRORS['Failed']['discovery_unpause']),

                       }
        return all(self.results.values())


    @_test_retry_and_log(False)
    def test_dbus_unpause_discovery_fail_already_unpaused(self):
        """ Test Failure case of unpause_discovery call.

        Call unpause discovery twice and make sure second call fails
        with 'org.bluez.Error.InProgress: Operation already in progress'.
        """
        reset = self._reset_state()
        is_discovering = self._wait_till_discovery_starts()
        pause_discovery, error = self.bluetooth_facade.pause_discovery()
        unpause_discovery, _ = self.bluetooth_facade.unpause_discovery()

        unpause_again, error = self.bluetooth_facade.unpause_discovery()

        inquiry_after_unpause = self._wait_till_hci_state_inquiry()

        self.results = {
            'reset' : reset,
            'is_discovering': is_discovering,
            'pause_discovery' : pause_discovery,
            'unpause_discovery' : unpause_discovery,
            'unpause_again_failed': not unpause_again,
            'error_matches' : self._compare_error(error,
                                    DBUS_ERRORS['Failed']['discovery_unpause']),
            'inquiry_after_unpause':inquiry_after_unpause
        }
        return all(self.results.values())

########################################################################
# dbus call: get_suppported_capabilities
# arguments: None
# returns : The dictionary is following the format
#           {capability : value}, where:
#
#           string capability:  The supported capability under
#                       discussion.
#           variant value:      A more detailed description of
#                       the capability.
#####################################################
# Positive cases
# Case 1
# Precondition: Adapter Powered on
# results: Result dictionary returned
#
# Case 2
# Precondition: Adapter Powered Off
# result : Result dictionary returned
################################################################################

    @_test_retry_and_log(False)
    def test_dbus_get_supported_capabilities_success(self):
        """ Test success case of get_supported_capabilities call. """
        reset = self._reset_state()
        is_power_on = self._wait_till_power_on()

        capabilities, error = self.bluetooth_facade.get_supported_capabilities()
        logging.debug('supported capabilities is %s', capabilities)

        self.results = {'reset' : reset,
                        'is_power_on' : is_power_on,
                        'get_supported_capabilities': error is None
                        }
        return all(self.results.values())

    @_test_retry_and_log(False)
    def test_dbus_get_supported_capabilities_success_power_off(self):
        """ Test success case of get_supported_capabilities call.
        Call get_supported_capabilities call with adapter powered off and
        confirm that it succeeds
        """

        reset = self._reset_state()
        is_power_off = self._wait_till_power_off()

        capabilities, error = self.bluetooth_facade.get_supported_capabilities()
        logging.debug('supported capabilities is %s', capabilities)

        self.results = {'reset' : reset,
                        'is_power_off' : is_power_off,
                        'get_supported_capabilities': error is None,
                        }
        return all(self.results.values())
