# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros.bluetooth import bluetooth_socket
from autotest_lib.server.cros.bluetooth import bluetooth_adapter_tests

DEVICE_ADDRESS = '01:02:03:04:05:06'
ADDRESS_TYPE = 0

class bluetooth_Sanity_DefaultStateTest(
        bluetooth_adapter_tests.BluetoothAdapterTests):
    """
    This class implements the default state test
    and all the helper functions it needs.
    """
    version = 1



    def compare_property(self, bluez_property, mgmt_setting, current_settings):
        """ Compare bluez property value and Kernel property

            @param bluez_property : Bluez property to be compared
            @param mgmt_setting   : Bit mask of management setting
            @param current_settings : Current kernel settings
            @return : True if bluez property and the current settings agree """

        cur_kernel_value = 1 if mgmt_setting & current_settings else 0
        return bluez_property == cur_kernel_value

    def default_state_test(self):
        """Test Default state of Bluetooth adapter after power cycling."""

        # Reset the adapter to the powered off state.
        self.test_reset_off_adapter()

        # Kernel default state depends on whether the kernel supports the
        # BR/EDR Whitelist. When this is supported the 'connectable' setting
        # remains unset and instead page scan is managed by the kernel based
        # on whether or not a BR/EDR device is in the whitelist.
        ( commands, events ) = self.read_supported_commands()
        supports_add_device = bluetooth_socket.MGMT_OP_ADD_DEVICE in commands

        # Read the initial state of the adapter. Verify that it is powered down.
        ( address, bluetooth_version, manufacturer_id,
                supported_settings, current_settings, class_of_device,
                name, short_name ) = self.read_info()
        self.log_settings('Initial state', current_settings)

        if current_settings & bluetooth_socket.MGMT_SETTING_POWERED:
            raise error.TestFail('Bluetooth adapter is powered')

        # The other kernel settings (connectable, pairable, etc.) reflect the
        # initial state before the bluetooth daemon adjusts them - we're ok
        # with them being on or off during that brief period.
        #

        # Verify that the Bluetooth Daemon sees that it is also powered down,
        # non-discoverable and not discovering devices.
        bluez_properties = self.get_adapter_properties()

        if bluez_properties['Powered']:
            raise error.TestFail('Bluetooth daemon Powered property does not '
                                 'match kernel while powered off')
        if not self.compare_property(bluez_properties['Discoverable'],
                                     bluetooth_socket.MGMT_SETTING_DISCOVERABLE,
                                     current_settings):
            raise error.TestFail('Bluetooth daemon Discoverable property '
                                 'does not match kernel while powered off')
        if bluez_properties['Discovering']:
            raise error.TestFail('Bluetooth daemon believes adapter is '
                                 'discovering while powered off')

        # Compare with the raw HCI state of the adapter as well, this should
        # be just not "UP", otherwise something deeply screwy is happening.
        flags = self.get_dev_info()[3]
        self.log_flags('Initial state', flags)

        if flags & bluetooth_socket.HCI_UP:
            raise error.TestFail('HCI UP flag does not match kernel while '
                                 'powered off')

        # Power on the adapter, then read the state again. Verify that it is
        # powered up, pairable, but not discoverable.
        self.test_power_on_adapter()
        current_settings = self.read_info()[4]
        self.log_settings("Powered up", current_settings)

        if not current_settings & bluetooth_socket.MGMT_SETTING_POWERED:
            raise error.TestFail('Bluetooth adapter is not powered')

        # If the kernel does not supports the BR/EDR whitelist, the adapter
        # should be generically connectable;
        # if it doesn't, then it depends on previous settings.
        if not supports_add_device:
            if not current_settings & bluetooth_socket.MGMT_SETTING_CONNECTABLE:
                raise error.TestFail('Bluetooth adapter is not connectable '
                                     'though kernel does not support '
                                     'BR/EDR whitelist')

        # Verify that the Bluetooth Daemon sees the same state as the kernel
        # and that it's not discovering.
        bluez_properties = self.get_adapter_properties()

        if not bluez_properties['Powered']:
            raise error.TestFail('Bluetooth daemon Powered property does not '
                                 'match kernel while powered on')
        if not self.compare_property(bluez_properties['Pairable'],
                                     bluetooth_socket.MGMT_SETTING_PAIRABLE,
                                     current_settings):
            raise error.TestFail('Bluetooth daemon Pairable property does not '
                                 'match kernel while powered on')
        if not self.compare_property(bluez_properties['Discoverable'],
                                     bluetooth_socket.MGMT_SETTING_DISCOVERABLE,
                                     current_settings):
            raise error.TestFail('Bluetooth daemon Discoverable property '
                                 'does not match kernel while powered on')
        if bluez_properties['Discovering']:
            raise error.TestFail('Bluetooth daemon believes adapter is '
                                 'discovering while powered on')

        # Compare with the raw HCI state of the adapter while powered up as
        # well.
        flags = self.get_dev_info()[3]
        self.log_flags('Powered up', flags)

        if not flags & bluetooth_socket.HCI_UP:
            raise error.TestFail('HCI UP flag does not match kernel while '
                                 'powered on')
        if not flags & bluetooth_socket.HCI_RUNNING:
            raise error.TestFail('HCI RUNNING flag does not match kernel while '
                                 'powered on')
        if bool(flags & bluetooth_socket.HCI_ISCAN) != \
            bool(bluez_properties['Discoverable']):
            raise error.TestFail('HCI ISCAN flag does not match kernel while '
                                 'powered on')
        if flags & bluetooth_socket.HCI_INQUIRY:
            raise error.TestFail('HCI INQUIRY flag does not match kernel while '
                                 'powered on')

        # If the kernel does not supports the BR/EDR whitelist, the adapter
        # should generically connectable, so should it should be in PSCAN
        # mode. This matches the management API "connectable" setting so far.
        if not supports_add_device:
            if not flags & bluetooth_socket.HCI_PSCAN:
                raise error.TestFail('HCI PSCAN flag not set though kernel'
                                     'does not supports BR/EDR whitelist')

        # Now we can examine the differences. Try adding and removing a device
        # from the kernel BR/EDR whitelist. The management API "connectable"
        # setting should remain off, but we should be able to see the PSCAN
        # flag come and go.
        if supports_add_device:
            # If PSCAN is currently on then device is CONNECTABLE
            # or a previous add device which was not removed.
            # Turn on and off DISCOVERABLE to turn off CONNECTABLE and
            # PSCAN
            if flags & bluetooth_socket.HCI_PSCAN:
                if not (current_settings &
                        bluetooth_socket.MGMT_SETTING_CONNECTABLE):
                    raise error.TestFail('PSCAN on but device not CONNECTABLE')
                logging.debug('Toggle Discoverable to turn off CONNECTABLE')
                self.test_discoverable()
                self.test_nondiscoverable()
                current_settings = self.read_info()[4]
                flags = self.get_dev_info()[3]
                self.log_flags('Discoverability Toggled', flags)
                if flags & bluetooth_socket.HCI_PSCAN:
                    raise error.TestFail('PSCAN on after toggling DISCOVERABLE')

            previous_settings = current_settings
            previous_flags = flags

            self.add_device(DEVICE_ADDRESS, ADDRESS_TYPE, 1)

            # Wait for a few seconds before reading the settings
            time.sleep(3)
            current_settings = self.read_info()[4]
            self.log_settings("After add device",
                                              current_settings)

            flags = self.get_dev_info()[3]
            self.log_flags('After add device', flags)

            if current_settings != previous_settings:
                self.log_settings("previous settings", previous_settings)
                self.log_settings("current settings", current_settings)
                raise error.TestFail(
                    'Bluetooth adapter settings changed after add device')
            if not flags & bluetooth_socket.HCI_PSCAN:
                raise error.TestFail('HCI PSCAN flag not set after add device')

            # Remove the device again, and make sure the PSCAN flag goes away.
            self.remove_device(DEVICE_ADDRESS, ADDRESS_TYPE)

            # PSCAN is still enabled for a few seconds after remove device
            # on older devices. Wait for few second before reading the settigs
            time.sleep(3)
            current_settings = self.read_info()[4]
            self.log_settings("After remove device", current_settings)

            flags = self.get_dev_info()[3]
            self.log_flags('After remove device', flags)

            if current_settings != previous_settings:
                raise error.TestFail(
                    'Bluetooth adapter settings changed after remove device')
            if flags & bluetooth_socket.HCI_PSCAN:
                raise error.TestFail('HCI PSCAN flag set after remove device')

        # Finally power off the adapter again, and verify that the adapter has
        # returned to powered down.
        self.test_power_off_adapter()
        current_settings = self.read_info()[4]
        self.log_settings("After power down", current_settings)

        if current_settings & bluetooth_socket.MGMT_SETTING_POWERED:
            raise error.TestFail('Bluetooth adapter is powered after power off')

        # Verify that the Bluetooth Daemon sees the same state as the kernel.
        bluez_properties = self.get_adapter_properties()

        if bluez_properties['Powered']:
            raise error.TestFail('Bluetooth daemon Powered property does not '
                                 'match kernel after power off')
        if not self.compare_property(bluez_properties['Discoverable'],
                                     bluetooth_socket.MGMT_SETTING_DISCOVERABLE,
                                     current_settings):
            raise error.TestFail('Bluetooth daemon Discoverable property '
                                 'does not match kernel after off')
        if bluez_properties['Discovering']:
            raise error.TestFail('Bluetooth daemon believes adapter is '
                                 'discovering after power off')

        # And one last comparison with the raw HCI state of the adapter.
        flags = self.get_dev_info()[3]
        self.log_flags('After power down', flags)

        if flags & bluetooth_socket.HCI_UP:
            raise error.TestFail('HCI UP flag does not match kernel after '
                                 'power off')
