#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
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

import inspect
import logging

import acts.test_utils.wifi.wifi_test_utils as awutils
import acts.test_utils.abstract_devices.utils_lib.wlan_utils as fwutils
from acts.utils import get_interface_ip_addresses
from acts.utils import adb_shell_ping

from acts import asserts
from acts.controllers.fuchsia_device import FuchsiaDevice
from acts.controllers.android_device import AndroidDevice


def create_wlan_device(hardware_device):
    """Creates a generic WLAN device based on type of device that is sent to
    the functions.

    Args:
        hardware_device: A WLAN hardware device that is supported by ACTS.
    """
    if isinstance(hardware_device, FuchsiaDevice):
        return FuchsiaWlanDevice(hardware_device)
    elif isinstance(hardware_device, AndroidDevice):
        return AndroidWlanDevice(hardware_device)
    else:
        raise ValueError('Unable to create WlanDevice for type %s' %
                         type(hardware_device))


class WlanDevice(object):
    """Class representing a generic WLAN device.

    Each object of this class represents a generic WLAN device.
    Android device and Fuchsia devices are the currently supported devices/

    Attributes:
        device: A generic WLAN device.
    """
    def __init__(self, device):
        self.device = device
        self.log = logging

    def wifi_toggle_state(self, state):
        """Base generic WLAN interface.  Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def reset_wifi(self):
        """Base generic WLAN interface.  Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def take_bug_report(self, test_name, begin_time):
        """Base generic WLAN interface.  Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def get_log(self, test_name, begin_time):
        """Base generic WLAN interface.  Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def turn_location_off_and_scan_toggle_off(self):
        """Base generic WLAN interface.  Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def associate(self,
                  target_ssid,
                  target_pwd=None,
                  check_connectivity=True,
                  hidden=False):
        """Base generic WLAN interface.  Only called if not overriden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def disconnect(self):
        """Base generic WLAN interface.  Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def get_wlan_interface_id_list(self):
        """Base generic WLAN interface.  Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def destroy_wlan_interface(self, iface_id):
        """Base generic WLAN interface.  Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def send_command(self, command):
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def get_interface_ip_addresses(self, interface):
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def is_connected(self):
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def ping(self, dest_ip, count=3, interval=1000, timeout=1000, size=25):
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))


class AndroidWlanDevice(WlanDevice):
    """Class wrapper for an Android WLAN device.

    Each object of this class represents a generic WLAN device.
    Android device and Fuchsia devices are the currently supported devices/

    Attributes:
        android_device: An Android WLAN device.
    """
    def __init__(self, android_device):
        super().__init__(android_device)

    def wifi_toggle_state(self, state):
        awutils.wifi_toggle_state(self.device, state)

    def reset_wifi(self):
        awutils.reset_wifi(self.device)

    def take_bug_report(self, test_name, begin_time):
        self.device.take_bug_report(test_name, begin_time)

    def get_log(self, test_name, begin_time):
        self.device.cat_adb_log(test_name, begin_time)

    def turn_location_off_and_scan_toggle_off(self):
        awutils.turn_location_off_and_scan_toggle_off(self.device)

    def associate(self,
                  target_ssid,
                  target_pwd=None,
                  key_mgmt=None,
                  check_connectivity=True,
                  hidden=False):
        """Function to associate an Android WLAN device.

        Args:
            target_ssid: SSID to associate to.
            target_pwd: Password for the SSID, if necessary.
            key_mgmt: The hostapd wpa_key_mgmt value, distinguishes wpa3 from
                wpa2 for android tests.
            check_connectivity: Whether to check for internet connectivity.
            hidden: Whether the network is hidden.
        Returns:
            True if successfully connected to WLAN, False if not.
        """
        network = {'SSID': target_ssid, 'hiddenSSID': hidden}
        if target_pwd:
            network['password'] = target_pwd
        if key_mgmt:
            network['security'] = key_mgmt
        try:
            awutils.connect_to_wifi_network(
                self.device,
                network,
                check_connectivity=check_connectivity,
                hidden=hidden)
            return True
        except Exception as e:
            self.device.log.info('Failed to associated (%s)' % e)
            return False

    def disconnect(self):
        awutils.turn_location_off_and_scan_toggle_off(self.device)

    def get_wlan_interface_id_list(self):
        pass

    def destroy_wlan_interface(self, iface_id):
        pass

    def send_command(self, command):
        return self.device.adb.shell(str(command))

    def get_interface_ip_addresses(self, interface):
        return get_interface_ip_addresses(self.device, interface)

    def is_connected(self):
        return 'BSSID' in self.device.droid.wifiGetConnectionInfo()

    def ping(self, dest_ip, count=3, interval=1000, timeout=1000, size=25):
        return adb_shell_ping(self.device,
                              dest_ip=dest_ip,
                              count=count,
                              timeout=timeout)


class FuchsiaWlanDevice(WlanDevice):
    """Class wrapper for an Fuchsia WLAN device.

    Each object of this class represents a generic WLAN device.
    Android device and Fuchsia devices are the currently supported devices/

    Attributes:
        fuchsia_device: A Fuchsia WLAN device.
    """
    def __init__(self, fuchsia_device):
        super().__init__(fuchsia_device)

    def wifi_toggle_state(self, state):
        """Stub for Fuchsia implementation."""
        pass

    def reset_wifi(self):
        """Stub for Fuchsia implementation."""
        pass

    def take_bug_report(self, test_name, begin_time):
        """Stub for Fuchsia implementation."""
        pass

    def get_log(self, test_name, begin_time):
        """Stub for Fuchsia implementation."""
        pass

    def turn_location_off_and_scan_toggle_off(self):
        """Stub for Fuchsia implementation."""
        pass

    def associate(self,
                  target_ssid,
                  target_pwd=None,
                  key_mgmt=None,
                  check_connectivity=True,
                  hidden=False):
        """Function to associate a Fuchsia WLAN device.

        Args:
            target_ssid: SSID to associate to.
            target_pwd: Password for the SSID, if necessary.
            key_mgmt: the hostapd wpa_key_mgmt, if specified.
            check_connectivity: Whether to check for internet connectivity.
            hidden: Whether the network is hidden.
        Returns:
            True if successfully connected to WLAN, False if not.
        """
        connection_response = self.device.wlan_lib.wlanConnectToNetwork(
            target_ssid, target_pwd=target_pwd)

        return self.device.check_connect_response(connection_response)

    def disconnect(self):
        """Function to disconnect from a Fuchsia WLAN device.
           Asserts if disconnect was not successful.
        """
        disconnect_response = self.device.wlan_lib.wlanDisconnect()
        asserts.assert_true(
            self.device.check_disconnect_response(disconnect_response),
            'Failed to disconnect.')

    def status(self):
        return self.device.wlan_lib.wlanStatus()

    def ping(self, dest_ip, count=3, interval=1000, timeout=1000, size=25):
        ping_result = self.device.ping(dest_ip,
                                       count=count,
                                       interval=interval,
                                       timeout=timeout,
                                       size=size)
        return ping_result['status']

    def get_wlan_interface_id_list(self):
        """Function to list available WLAN interfaces.

        Returns:
            A list of wlan interface IDs.
        """
        return self.device.wlan_lib.wlanGetIfaceIdList().get('result')

    def destroy_wlan_interface(self, iface_id):
        """Function to associate a Fuchsia WLAN device.

        Args:
            target_ssid: SSID to associate to.
            target_pwd: Password for the SSID, if necessary.
            check_connectivity: Whether to check for internet connectivity.
            hidden: Whether the network is hidden.
        Returns:
            True if successfully destroyed wlan interface, False if not.
        """
        result = self.device.wlan_lib.wlanDestroyIface(iface_id)
        if result.get('error') is None:
            return True
        else:
            self.log.error("Failed to destroy interface with: {}".format(
                result.get('error')))
            return False

    def send_command(self, command):
        return self.device.send_command_ssh(str(command)).stdout

    def get_interface_ip_addresses(self, interface):
        return get_interface_ip_addresses(self.device, interface)

    def is_connected(self):
        return fwutils.is_connected(self)
