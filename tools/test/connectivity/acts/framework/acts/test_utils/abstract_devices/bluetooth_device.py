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

from queue import Empty

from acts.controllers.android_device import AndroidDevice
from acts.controllers.fuchsia_device import FuchsiaDevice
from acts.test_utils.bt.bt_constants import ble_scan_settings_modes
from acts.test_utils.bt.bt_constants import gatt_cb_strings
from acts.test_utils.bt.bt_constants import gatt_event
from acts.test_utils.bt.bt_constants import scan_result
from acts.test_utils.bt.bt_gatt_utils import GattTestUtilsError
from acts.test_utils.bt.bt_gatt_utils import disconnect_gatt_connection
from acts.test_utils.bt.bt_gatt_utils import setup_gatt_connection
from acts.test_utils.fuchsia.bt_test_utils import le_scan_for_device_by_name

import acts.test_utils.bt.bt_test_utils as bt_test_utils


def create_bluetooth_device(hardware_device):
    """Creates a generic Bluetooth device based on type of device that is sent
    to the functions.

    Args:
        hardware_device: A Bluetooth hardware device that is supported by ACTS.
    """
    if isinstance(hardware_device, FuchsiaDevice):
        return FuchsiaBluetoothDevice(hardware_device)
    elif isinstance(hardware_device, AndroidDevice):
        return AndroidBluetoothDevice(hardware_device)
    else:
        raise ValueError('Unable to create BluetoothDevice for type %s' %
                         type(hardware_device))


class BluetoothDevice(object):
    """Class representing a generic Bluetooth device.

    Each object of this class represents a generic Bluetooth device.
    Android device and Fuchsia devices are the currently supported devices.

    Attributes:
        device: A generic Bluetooth device.
    """
    def __init__(self, device):
        self.device = device
        self.log = logging

    def a2dp_initiate_open_stream(self):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def start_profile_a2dp_sink(self):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def stop_profile_a2dp_sink(self):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def start_pairing_helper(self):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def set_discoverable(self, is_discoverable):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def bluetooth_toggle_state(self, state):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def gatt_client_discover_characteristic_by_uuid(self, peer_identifier,
                                                    uuid):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def initialize_bluetooth_controller(self):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def get_pairing_pin(self):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def input_pairing_pin(self, pin):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def get_bluetooth_local_address(self):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def gatt_connect(self, peer_identifier, transport, autoconnect):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def gatt_client_write_characteristic_without_response_by_handle(
            self, peer_identifier, handle, value):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def gatt_client_write_characteristic_by_handle(self, peer_identifier,
                                                   handle, offset, value):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def gatt_client_read_characteristic_by_handle(self, peer_identifier,
                                                  handle):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def gatt_client_read_long_characteristic_by_handle(self, peer_identifier,
                                                       handle, offset,
                                                       max_bytes):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def gatt_client_enable_notifiy_characteristic_by_handle(
            self, peer_identifier, handle):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def gatt_client_disable_notifiy_characteristic_by_handle(
            self, peer_identifier, handle):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def gatt_client_read_descriptor_by_handle(self, peer_identifier, handle):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def gatt_client_write_descriptor_by_handle(self, peer_identifier, handle,
                                               offset, value):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def gatt_client_long_read_descriptor_by_handle(self, peer_identifier,
                                                   handle, offset, max_bytes):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def gatt_disconnect(self, peer_identifier):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def gatt_client_refresh(self, peer_identifier):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def le_scan_with_name_filter(self, name, timeout):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def log_info(self, log):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def reset_bluetooth(self):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def sdp_add_search(self, attribute_list, profile_id):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def sdp_add_service(self, sdp_record):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def sdp_clean_up(self):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def sdp_init(self):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def sdp_remove_service(self, service_id):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def start_le_advertisement(self, adv_data, adv_interval):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def stop_le_advertisement(self):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def set_bluetooth_local_name(self, name):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def setup_gatt_server(self, database):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def close_gatt_server(self):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def unbond_device(self, peer_identifier):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def unbond_all_known_devices(self):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))

    def init_pair(self, peer_identifier, security_level, non_bondable,
                  transport):
        """Base generic Bluetooth interface. Only called if not overridden by
        another supported device.
        """
        raise NotImplementedError("{} must be defined.".format(
            inspect.currentframe().f_code.co_name))


class AndroidBluetoothDevice(BluetoothDevice):
    """Class wrapper for an Android Bluetooth device.

    Each object of this class represents a generic Bluetooth device.
    Android device and Fuchsia devices are the currently supported devices/

    Attributes:
        android_device: An Android Bluetooth device.
    """
    def __init__(self, android_device):
        super().__init__(android_device)
        self.gatt_timeout = 10
        self.peer_mapping = {}
        self.discovered_services_index = None

    def _client_wait(self, gatt_event, gatt_callback):
        return self._timed_pop(gatt_event, gatt_callback)

    def _timed_pop(self, gatt_event, gatt_callback):
        expected_event = gatt_event["evt"].format(gatt_callback)
        try:
            return self.device.ed.pop_event(expected_event, self.gatt_timeout)
        except Empty as emp:
            raise AssertionError(gatt_event["err"].format(expected_event))

    def _setup_discovered_services_index(self, bluetooth_gatt):
        """ Sets the discovered services index for the gatt connection
        related to the Bluetooth GATT callback object.

        Args:
            bluetooth_gatt: The BluetoothGatt callback id
        """
        if not self.discovered_services_index:
            self.device.droid.gattClientDiscoverServices(bluetooth_gatt)
            expected_event = gatt_cb_strings['gatt_serv_disc'].format(
                self.gatt_callback)
            event = self.dut.ed.pop_event(expected_event, self.gatt_timeout)
            self.discovered_services_index = event['data']['ServicesIndex']

    def a2dp_initiate_open_stream(self):
        raise NotImplementedError("{} not yet implemented.".format(
            inspect.currentframe().f_code.co_name))

    def start_profile_a2dp_sink(self):
        raise NotImplementedError("{} not yet implemented.".format(
            inspect.currentframe().f_code.co_name))

    def stop_profile_a2dp_sink(self):
        raise NotImplementedError("{} not yet implemented.".format(
            inspect.currentframe().f_code.co_name))

    def bluetooth_toggle_state(self, state):
        self.device.droid.bluetoothToggleState(state)

    def set_discoverable(self, is_discoverable):
        """ Sets the device's discoverability.

        Args:
            is_discoverable: True if discoverable, false if not discoverable
        """
        if is_discoverable:
            self.device.droid.bluetoothMakeDiscoverable()
        else:
            self.device.droid.bluetoothMakeUndiscoverable()

    def initialize_bluetooth_controller(self):
        """ Just pass for Android as there is no concept of initializing
        a Bluetooth controller.
        """
        pass

    def start_pairing_helper(self):
        """ Starts the Android pairing helper.
        """
        self.device.droid.bluetoothStartPairingHelper(True)

    def gatt_client_write_characteristic_without_response_by_handle(
            self, peer_identifier, handle, value):
        """ Perform a GATT Client write Characteristic without response to
        remote peer GATT server database.

        Args:
            peer_identifier: The mac address associated with the GATT connection
            handle: The characteristic handle (or instance id).
            value: The list of bytes to write.
        Returns:
            True if success, False if failure.
        """
        peer_info = self.peer_mapping.get(peer_identifier)
        if not peer_info:
            self.log.error(
                "Peer idenifier {} not currently connected or unknown.".format(
                    peer_identifier))
            return False
        self._setup_discovered_services_index()
        self.device.droid.gattClientWriteCharacteristicByInstanceId(
            peer_info.get('bluetooth_gatt'), self.discovered_services_index,
            handle, value)
        try:
            event = self._client_wait(gatt_event['char_write'],
                                      peer_info.get('gatt_callback'))
        except AssertionError as err:
            self.log.error("Failed to write Characteristic: {}".format(err))
        return True

    def gatt_client_write_characteristic_by_handle(self, peer_identifier,
                                                   handle, offset, value):
        """ Perform a GATT Client write Characteristic without response to
        remote peer GATT server database.

        Args:
            peer_identifier: The mac address associated with the GATT connection
            handle: The characteristic handle (or instance id).
            offset: Not used yet.
            value: The list of bytes to write.
        Returns:
            True if success, False if failure.
        """
        peer_info = self.peer_mapping.get(peer_identifier)
        if not peer_info:
            self.log.error(
                "Peer idenifier {} not currently connected or unknown.".format(
                    peer_identifier))
            return False
        self._setup_discovered_services_index()
        self.device.droid.gattClientWriteCharacteristicByInstanceId(
            peer_info.get('bluetooth_gatt'), self.discovered_services_index,
            handle, value)
        try:
            event = self._client_wait(gatt_event['char_write'],
                                      peer_info.get('gatt_callback'))
        except AssertionError as err:
            self.log.error("Failed to write Characteristic: {}".format(err))
        return True

    def gatt_client_read_characteristic_by_handle(self, peer_identifier,
                                                  handle):
        """ Perform a GATT Client read Characteristic to remote peer GATT
        server database.

        Args:
            peer_identifier: The mac address associated with the GATT connection
            handle: The characteristic handle (or instance id).
        Returns:
            Value of Characteristic if success, None if failure.
        """
        peer_info = self.peer_mapping.get(peer_identifier)
        if not peer_info:
            self.log.error(
                "Peer idenifier {} not currently connected or unknown.".format(
                    peer_identifier))
            return False
        self._setup_discovered_services_index()
        self.dut.droid.gattClientReadCharacteristicByInstanceId(
            peer_info.get('bluetooth_gatt'), self.discovered_services_index,
            handle)
        try:
            event = self._client_wait(gatt_event['char_read'],
                                      peer_info.get('gatt_callback'))
        except AssertionError as err:
            self.log.error("Failed to read Characteristic: {}".format(err))

        return event['data']['CharacteristicValue']

    def gatt_client_read_long_characteristic_by_handle(self, peer_identifier,
                                                       handle, offset,
                                                       max_bytes):
        """ Perform a GATT Client read Characteristic to remote peer GATT
        server database.

        Args:
            peer_identifier: The mac address associated with the GATT connection
            offset: Not used yet.
            handle: The characteristic handle (or instance id).
            max_bytes: Not used yet.
        Returns:
            Value of Characteristic if success, None if failure.
        """
        peer_info = self.peer_mapping.get(peer_identifier)
        if not peer_info:
            self.log.error(
                "Peer idenifier {} not currently connected or unknown.".format(
                    peer_identifier))
            return False
        self._setup_discovered_services_index()
        self.dut.droid.gattClientReadCharacteristicByInstanceId(
            peer_info.get('bluetooth_gatt'), self.discovered_services_index,
            handle)
        try:
            event = self._client_wait(gatt_event['char_read'],
                                      peer_info.get('gatt_callback'))
        except AssertionError as err:
            self.log.error("Failed to read Characteristic: {}".format(err))

        return event['data']['CharacteristicValue']

    def gatt_client_enable_notifiy_characteristic_by_handle(
            self, peer_identifier, handle):
        """ Perform a GATT Client enable Characteristic notification to remote
        peer GATT server database.

        Args:
            peer_identifier: The mac address associated with the GATT connection
            handle: The characteristic handle.
        Returns:
            True is success, False if failure.
        """
        raise NotImplementedError("{} not yet implemented.".format(
            inspect.currentframe().f_code.co_name))

    def gatt_client_disable_notifiy_characteristic_by_handle(
            self, peer_identifier, handle):
        """ Perform a GATT Client disable Characteristic notification to remote
        peer GATT server database.

        Args:
            peer_identifier: The mac address associated with the GATT connection
            handle: The characteristic handle.
        Returns:
            True is success, False if failure.
        """
        raise NotImplementedError("{} not yet implemented.".format(
            inspect.currentframe().f_code.co_name))

    def gatt_client_read_descriptor_by_handle(self, peer_identifier, handle):
        """ Perform a GATT Client read Descriptor to remote peer GATT
        server database.

        Args:
            peer_identifier: The mac address associated with the GATT connection
            handle: The Descriptor handle (or instance id).
        Returns:
            Value of Descriptor if success, None if failure.
        """
        peer_info = self.peer_mapping.get(peer_identifier)
        if not peer_info:
            self.log.error(
                "Peer idenifier {} not currently connected or unknown.".format(
                    peer_identifier))
            return False
        self._setup_discovered_services_index()
        self.dut.droid.gattClientReadDescriptorByInstanceId(
            peer_info.get('bluetooth_gatt'), self.discovered_services_index,
            handle)
        try:
            event = self._client_wait(gatt_event['desc_read'],
                                      peer_info.get('gatt_callback'))
        except AssertionError as err:
            self.log.error("Failed to read Descriptor: {}".format(err))
        # TODO: Implement sending Descriptor value in SL4A such that the data
        # can be represented by: event['data']['DescriptorValue']
        return ""

    def gatt_client_write_descriptor_by_handle(self, peer_identifier, handle,
                                               offset, value):
        """ Perform a GATT Client write Descriptor to the remote peer GATT
        server database.

        Args:
            peer_identifier: The mac address associated with the GATT connection
            handle: The Descriptor handle (or instance id).
            offset: Not used yet
            value: The list of bytes to write.
        Returns:
            True if success, False if failure.
        """
        peer_info = self.peer_mapping.get(peer_identifier)
        if not peer_info:
            self.log.error(
                "Peer idenifier {} not currently connected or unknown.".format(
                    peer_identifier))
            return False
        self._setup_discovered_services_index()
        self.device.droid.gattClientWriteDescriptorByInstanceId(
            peer_info.get('bluetooth_gatt'), self.discovered_services_index,
            handle, value)
        try:
            event = self._client_wait(gatt_event['desc_write'],
                                      peer_info.get('gatt_callback'))
        except AssertionError as err:
            self.log.error("Failed to write Characteristic: {}".format(err))
        return True

    def gatt_connect(self, peer_identifier, transport, autoconnect=False):
        """ Perform a GATT connection to a perihperal.

        Args:
            peer_identifier: The mac address to connect to.
            transport: Which transport to use.
            autoconnect: Set autocnnect to True or False.
        Returns:
            True if success, False if failure.
        """
        try:
            bluetooth_gatt, gatt_callback = setup_gatt_connection(
                self.device, peer_identifier, autoconnect, transport)
            self.peer_mapping[peer_identifier] = {
                "bluetooth_gatt": bluetooth_gatt,
                "gatt_callback": gatt_callback
            }
        except GattTestUtilsError as err:
            self.log.error(err)
            return False
        return True

    def gatt_disconnect(self, peer_identifier):
        """ Perform a GATT disconnect from a perihperal.

        Args:
            peer_identifier: The peer to disconnect from.
        Returns:
            True if success, False if failure.
        """
        peer_info = self.peer_mapping.get(peer_identifier)
        if not peer_info:
            self.log.error(
                "No previous connections made to {}".format(peer_identifier))
            return False

        try:
            disconnect_gatt_connection(self.device,
                                       peer_info.get("bluetooth_gatt"),
                                       peer_info.get("gatt_callback"))
            self.device.droid.gattClientClose(peer_info.get("bluetooth_gatt"))
        except GattTestUtilsError as err:
            self.log.error(err)
            return False
        self.device.droid.gattClientClose(peer_info.get("bluetooth_gatt"))

    def gatt_client_refresh(self, peer_identifier):
        """ Perform a GATT Client Refresh of a perihperal.

        Clears the internal cache and forces a refresh of the services from the
        remote device.

        Args:
            peer_identifier: The peer to refresh.
        """
        peer_info = self.peer_mapping.get(peer_identifier)
        if not peer_info:
            self.log.error(
                "No previous connections made to {}".format(peer_identifier))
            return False
        self.device.droid.gattClientRefresh(peer_info["bluetooth_gatt"])

    def le_scan_with_name_filter(self, name, timeout):
        """ Scan over LE for a specific device name.

         Args:
            name: The name filter to set.
            timeout: The timeout to wait to find the advertisement.
        Returns:
            Discovered mac address or None
        """
        self.device.droid.bleSetScanSettingsScanMode(
            ble_scan_settings_modes['low_latency'])
        filter_list = self.device.droid.bleGenFilterList()
        scan_settings = self.device.droid.bleBuildScanSetting()
        scan_callback = self.device.droid.bleGenScanCallback()
        self.device.droid.bleSetScanFilterDeviceName(name)
        self.device.droid.bleBuildScanFilter(filter_list)
        self.device.droid.bleSetScanFilterDeviceName(self.name)
        self.device.droid.bleStartBleScan(filter_list, scan_settings,
                                          scan_callback)
        try:
            event = self.device.ed.pop_event(scan_result.format(scan_callback),
                                             timeout)
            return event['data']['Result']['deviceInfo']['address']
        except Empty as err:
            self.log.info("Scanner did not find advertisement {}".format(err))
            return None

    def log_info(self, log):
        """ Log directly onto the device.

        Args:
            log: The informative log.
        """
        self.device.droid.log.logI(log)

    def set_bluetooth_local_name(self, name):
        """ Sets the Bluetooth controller's local name
        Args:
            name: The name to set.
        """
        self.device.droid.bluetoothSetLocalName(name)

    def get_local_bluetooth_address(self):
        """ Returns the Bluetooth local address.
        """
        return self.device.droid.bluetoothGetLocalAddress()

    def reset_bluetooth(self):
        """ Resets Bluetooth on the Android Device.
        """
        bt_test_utils.reset_bluetooth([self.device])

    def sdp_add_search(self, attribute_list, profile_id):
        """Adds an SDP search record.
        Args:
            attribute_list: The list of attributes to set
            profile_id: The profile ID to set.
        """
        # Android devices currently have no hooks to modify the SDP record.
        pass

    def sdp_add_service(self, sdp_record):
        """Adds an SDP service record.
        Args:
            sdp_record: The dictionary representing the search record to add.
        Returns:
            service_id: The service id to track the service record published.
                None if failed.
        """
        # Android devices currently have no hooks to modify the SDP record.
        pass

    def sdp_clean_up(self):
        """Cleans up all objects related to SDP.
        """
        self.device.sdp_lib.cleanUp()

    def sdp_init(self):
        """Initializes SDP on the device.
        """
        # Android devices currently have no hooks to modify the SDP record.
        pass

    def sdp_remove_service(self, service_id):
        """Removes a service based on an input id.
        Args:
            service_id: The service ID to remove.
        """
        # Android devices currently have no hooks to modify the SDP record.
        pass

    def unbond_all_known_devices(self):
        """ Unbond all known remote devices.
        """
        self.device.droid.bluetoothFactoryReset()

    def unbond_device(self, peer_identifier):
        """ Unbond peer identifier.

        Args:
            peer_identifier: The mac address for the peer to unbond.

        """
        self.device.droid.bluetoothUnbond(peer_identifier)

    def init_pair(self, peer_identifier, security_level, non_bondable,
                  transport):
        """ Send an outgoing pairing request the input peer_identifier.

        Android currently does not support setting various security levels or
        bondable modes. Making them available for other bluetooth_device
        variants. Depending on the Address type, Android will figure out the
        transport to pair automatically.

        Args:
            peer_identifier: A string representing the device id.
            security_level: Not yet implemented. See Fuchsia device impl.
            non_bondable: Not yet implemented. See Fuchsia device impl.
            transport: Not yet implemented. See Fuchsia device impl.

        """
        self.dut.droid.bluetoothBond(self.peer_identifier)


class FuchsiaBluetoothDevice(BluetoothDevice):
    """Class wrapper for an Fuchsia Bluetooth device.

    Each object of this class represents a generic luetooth device.
    Android device and Fuchsia devices are the currently supported devices/

    Attributes:
        fuchsia_device: A Fuchsia Bluetooth device.
    """
    def __init__(self, fuchsia_device):
        super().__init__(fuchsia_device)

    def a2dp_initiate_open_stream(self):
        raise NotImplementedError("{} not yet implemented.".format(
            inspect.currentframe().f_code.co_name))

    def start_profile_a2dp_sink(self):
        """ Starts the A2DP sink profile.
        """
        self.device.control_daemon("bt-a2dp-sink.cmx", "start")

    def stop_profile_a2dp_sink(self):
        """ Stops the A2DP sink profile.
        """
        self.device.control_daemon("bt-a2dp-sink.cmx", "stop")

    def start_pairing_helper(self):
        self.device.btc_lib.acceptPairing()

    def bluetooth_toggle_state(self, state):
        """Stub for Fuchsia implementation."""
        pass

    def set_discoverable(self, is_discoverable):
        """ Sets the device's discoverability.

        Args:
            is_discoverable: True if discoverable, false if not discoverable
        """
        self.device.btc_lib.setDiscoverable(is_discoverable)

    def get_pairing_pin(self):
        """ Get the pairing pin from the active pairing delegate.
        """
        return self.device.btc_lib.getPairingPin()['result']

    def input_pairing_pin(self, pin):
        """ Input pairing pin to active pairing delegate.

        Args:
            pin: The pin to input.
        """
        self.device.btc_lib.inputPairingPin(pin)

    def initialize_bluetooth_controller(self):
        """ Initialize Bluetooth controller for first time use.
        """
        self.device.btc_lib.initBluetoothControl()

    def get_local_bluetooth_address(self):
        """ Returns the Bluetooth local address.
        """
        return self.device.btc_lib.getActiveAdapterAddress().get("result")

    def set_bluetooth_local_name(self, name):
        """ Sets the Bluetooth controller's local name
        Args:
            name: The name to set.
        """
        self.device.btc_lib.setName(name)

    def gatt_client_write_characteristic_without_response_by_handle(
            self, peer_identifier, handle, value):
        """ Perform a GATT Client write Characteristic without response to
        remote peer GATT server database.

        Args:
            peer_identifier: The peer to connect to.
            handle: The characteristic handle.
            value: The list of bytes to write.
        Returns:
            True if success, False if failure.
        """
        if (not self._find_service_id_and_connect_to_service_for_handle(
                peer_identifier, handle)):
            self.log.error(
                "Unable to find handle {} in GATT server db.".format(handle))
            return False
        result = self.device.gattc_lib.writeCharByIdWithoutResponse(
            handle, value)
        if result.get("error") is not None:
            self.log.error(
                "Failed to write characteristic handle {} with err: {}".format(
                    handle, result.get("error")))
            return False
        return True

    def gatt_client_write_characteristic_by_handle(self, peer_identifier,
                                                   handle, offset, value):
        """ Perform a GATT Client write Characteristic to remote peer GATT
        server database.

        Args:
            peer_identifier: The peer to connect to.
            handle: The characteristic handle.
            offset: The offset to start writing to.
            value: The list of bytes to write.
        Returns:
            True if success, False if failure.
        """
        if (not self._find_service_id_and_connect_to_service_for_handle(
                peer_identifier, handle)):
            self.log.error(
                "Unable to find handle {} in GATT server db.".format(handle))
            return False
        result = self.device.gattc_lib.writeCharById(handle, offset, value)
        if result.get("error") is not None:
            self.log.error(
                "Failed to write characteristic handle {} with err: {}".format(
                    handle, result.get("error")))
            return False
        return True

    def gatt_client_read_characteristic_by_handle(self, peer_identifier,
                                                  handle):
        """ Perform a GATT Client read Characteristic to remote peer GATT
        server database.

        Args:
            peer_identifier: The peer to connect to.
            handle: The characteristic handle.
        Returns:
            Value of Characteristic if success, None if failure.
        """
        if (not self._find_service_id_and_connect_to_service_for_handle(
                peer_identifier, handle)):
            self.log.error(
                "Unable to find handle {} in GATT server db.".format(handle))
            return False
        result = self.device.gattc_lib.readCharacteristicById(handle)
        if result.get("error") is not None:
            self.log.error(
                "Failed to read characteristic handle {} with err: {}".format(
                    handle, result.get("error")))
            return None
        return result.get("result")

    def gatt_client_read_long_characteristic_by_handle(self, peer_identifier,
                                                       handle, offset,
                                                       max_bytes):
        """ Perform a GATT Client read Characteristic to remote peer GATT
        server database.

        Args:
            peer_identifier: The peer to connect to.
            handle: The characteristic handle.
            offset: The offset to start reading.
            max_bytes: The max bytes to return for each read.
        Returns:
            Value of Characteristic if success, None if failure.
        """
        if (not self._find_service_id_and_connect_to_service_for_handle(
                peer_identifier, handle)):
            self.log.error(
                "Unable to find handle {} in GATT server db.".format(handle))
            return False
        result = self.device.gattc_lib.readLongCharacteristicById(
            handle, offset, max_bytes)
        if result.get("error") is not None:
            self.log.error(
                "Failed to read characteristic handle {} with err: {}".format(
                    handle, result.get("error")))
            return None
        return result.get("result")

    def gatt_client_enable_notifiy_characteristic_by_handle(
            self, peer_identifier, handle):
        """ Perform a GATT Client enable Characteristic notification to remote
        peer GATT server database.

        Args:
            peer_identifier: The peer to connect to.
            handle: The characteristic handle.
        Returns:
            True is success, False if failure.
        """
        if (not self._find_service_id_and_connect_to_service_for_handle(
                peer_identifier, handle)):
            self.log.error(
                "Unable to find handle {} in GATT server db.".format(handle))
            return False
        result = self.device.gattc_lib.enableNotifyCharacteristic(handle)
        if result.get("error") is not None:
            self.log.error(
                "Failed to enable characteristic notifications for handle {} "
                "with err: {}".format(handle, result.get("error")))
            return None
        return result.get("result")

    def gatt_client_disable_notifiy_characteristic_by_handle(
            self, peer_identifier, handle):
        """ Perform a GATT Client disable Characteristic notification to remote
        peer GATT server database.

        Args:
            peer_identifier: The peer to connect to.
            handle: The characteristic handle.
        Returns:
            True is success, False if failure.
        """
        if (not self._find_service_id_and_connect_to_service_for_handle(
                peer_identifier, handle)):
            self.log.error(
                "Unable to find handle {} in GATT server db.".format(handle))
            return False
        result = self.device.gattc_lib.disableNotifyCharacteristic(handle)
        if result.get("error") is not None:
            self.log.error(
                "Failed to disable characteristic notifications for handle {} "
                "with err: {}".format(peer_identifier, result.get("error")))
            return None
        return result.get("result")

    def gatt_client_read_descriptor_by_handle(self, peer_identifier, handle):
        """ Perform a GATT Client read Descriptor to remote peer GATT server
        database.

        Args:
            peer_identifier: The peer to connect to.
            handle: The Descriptor handle.
        Returns:
            Value of Descriptor if success, None if failure.
        """
        if (not self._find_service_id_and_connect_to_service_for_handle(
                peer_identifier, handle)):
            self.log.error(
                "Unable to find handle {} in GATT server db.".format(handle))
            return False
        result = self.device.gattc_lib.readDescriptorById(handle)
        if result.get("error") is not None:
            self.log.error(
                "Failed to read descriptor for handle {} with err: {}".format(
                    peer_identifier, result.get("error")))
            return None
        return result.get("result")

    def gatt_client_write_descriptor_by_handle(self, peer_identifier, handle,
                                               offset, value):
        """ Perform a GATT Client write Descriptor to remote peer GATT server
        database.

        Args:
            peer_identifier: The peer to connect to.
            handle: The Descriptor handle.
            offset: The offset to start writing at.
            value: The list of bytes to write.
        Returns:
            True if success, False if failure.
        """
        if (not self._find_service_id_and_connect_to_service_for_handle(
                peer_identifier, handle)):
            self.log.error(
                "Unable to find handle {} in GATT server db.".format(handle))
            return False
        result = self.device.gattc_lib.writeDescriptorById(
            handle, offset, value)
        if result.get("error") is not None:
            self.log.error(
                "Failed to write descriptor for handle {} with err: {}".format(
                    peer_identifier, result.get("error")))
            return None
        return True

    def gatt_connect(self, peer_identifier, transport, autoconnect):
        """ Perform a GATT connection to a perihperal.

        Args:
            peer_identifier: The peer to connect to.
            transport: Not implemented.
            autoconnect: Not implemented.
        Returns:
            True if success, False if failure.
        """
        connection_result = self.device.gattc_lib.bleConnectToPeripheral(
            peer_identifier)
        if connection_result.get("error") is not None:
            self.log.error("Failed to connect to peer id {}: {}".format(
                peer_identifier, connection_result.get("error")))
            return False
        return True

    def gatt_client_refresh(self, peer_identifier):
        """ Perform a GATT Client Refresh of a perihperal.

        Clears the internal cache and forces a refresh of the services from the
        remote device. In Fuchsia there is no FIDL api to automatically do this
        yet. Therefore just read all Characteristics which satisfies the same
        requirements.

        Args:
            peer_identifier: The peer to refresh.
        """
        self._read_all_characteristics(peer_identifier)

    def gatt_client_discover_characteristic_by_uuid(self, peer_identifier,
                                                    uuid):
        """ Perform a GATT Client Refresh of a perihperal.

        Clears the internal cache and forces a refresh of the services from the
        remote device. In Fuchsia there is no FIDL api to automatically do this
        yet. Therefore just read all Characteristics which satisfies the same
        requirements.

        Args:
            peer_identifier: The peer to refresh.
        """
        self._read_all_characteristics(peer_identifier, uuid)

    def gatt_disconnect(self, peer_identifier):
        """ Perform a GATT disconnect from a perihperal.

        Args:
            peer_identifier: The peer to disconnect from.
        Returns:
            True if success, False if failure.
        """
        disconnect_result = self.device.gattc_lib.bleDisconnectPeripheral(
            peer_identifier)
        if disconnect_result.get("error") is None:
            self.log.error("Failed to disconnect from peer id {}: {}".format(
                peer_identifier, disconnect_result.get("error")))
            return False
        return True

    def reset_bluetooth(self):
        """Stub for Fuchsia implementation."""
        pass

    def sdp_add_search(self, attribute_list, profile_id):
        """Adds an SDP search record.
        Args:
            attribute_list: The list of attributes to set
            profile_id: The profile ID to set.
        """
        return self.device.sdp_lib.addSearch(attribute_list, profile_id)

    def sdp_add_service(self, sdp_record):
        """Adds an SDP service record.
        Args:
            sdp_record: The dictionary representing the search record to add.
        """
        return self.device.sdp_lib.addService(sdp_record)

    def sdp_clean_up(self):
        """Cleans up all objects related to SDP.
        """
        return self.device.sdp_lib.cleanUp()

    def sdp_init(self):
        """Initializes SDP on the device.
        """
        return self.device.sdp_lib.init()

    def sdp_remove_service(self, service_id):
        """Removes a service based on an input id.
        Args:
            service_id: The service ID to remove.
        """
        return self.device.sdp_lib.init()

    def start_le_advertisement(self, adv_data, adv_interval):
        """ Starts an LE advertisement

        Args:
            adv_data: Advertisement data.
            adv_interval: Advertisement interval.
        """
        self.device.ble_lib.bleStartBleAdvertising(adv_data, adv_interval)

    def stop_le_advertisement(self):
        """ Stop active LE advertisement.
        """
        self.device.ble_lib.bleStopBleAdvertising()

    def setup_gatt_server(self, database):
        """ Sets up an input GATT server.

        Args:
            database: A dictionary representing the GATT database to setup.
        """
        self.device.gatts_lib.publishServer(database)

    def close_gatt_server(self):
        """ Closes an existing GATT server.
        """
        self.device.gatts_lib.closeServer()

    def le_scan_with_name_filter(self, name, timeout):
        """ Scan over LE for a specific device name.

        Args:
            name: The name filter to set.
            timeout: The timeout to wait to find the advertisement.
        Returns:
            Discovered device id or None
        """
        partial_match = True
        return le_scan_for_device_by_name(self.device, self.device.log, name,
                                          timeout, partial_match)

    def log_info(self, log):
        """ Log directly onto the device.

        Args:
            log: The informative log.
        """
        self.device.logging_lib.logI(log)
        pass

    def unbond_all_known_devices(self):
        """ Unbond all known remote devices.
        """
        try:
            device_list = self.device.btc_lib.getKnownRemoteDevices()['result']
            for device_info in device_list:
                device = device_list[device_info]
                if device['bonded']:
                    self.device.btc_lib.forgetDevice(device['id'])
        except Exception as err:
            self.log.err("Unable to unbond all devices: {}".format(err))

    def unbond_device(self, peer_identifier):
        """ Unbond peer identifier.

        Args:
            peer_identifier: The peer identifier for the peer to unbond.

        """
        self.device.btc_lib.forgetDevice(peer_identifier)

    def _find_service_id_and_connect_to_service_for_handle(
            self, peer_identifier, handle):
        fail_err = "Failed to find handle {} in Peer database."
        try:
            services = self.device.gattc_lib.listServices(peer_identifier)
            for service in services['result']:
                service_id = service['id']
                self.device.gattc_lib.connectToService(peer_identifier,
                                                       service_id)
                chars = self.device.gattc_lib.discoverCharacteristics()

                for char in chars['result']:
                    char_id = char['id']
                    if handle == char_id:
                        return True
                    descriptors = char['descriptors']
                    for desc in descriptors:
                        desc_id = desc["id"]
                        if handle == desc_id:
                            return True
        except Exception as err:
            self.log.error(fail_err.format(err))
            return False

    def _read_all_characteristics(self, peer_identifier, uuid=None):
        fail_err = "Failed to read all characteristics with: {}"
        try:
            services = self.device.gattc_lib.listServices(peer_identifier)
            for service in services['result']:
                service_id = service['id']
                service_uuid = service['uuid_type']
                self.device.gattc_lib.connectToService(peer_identifier,
                                                       service_id)
                chars = self.device.gattc_lib.discoverCharacteristics()
                self.log.info(
                    "Reading chars in service uuid: {}".format(service_uuid))

                for char in chars['result']:
                    char_id = char['id']
                    char_uuid = char['uuid_type']
                    if uuid and uuid.lower() not in char_uuid.lower():
                        continue
                    try:
                        read_val =  \
                            self.device.gattc_lib.readCharacteristicById(
                                char_id)
                        self.log.info(
                            "\tCharacteristic uuid / Value: {} / {}".format(
                                char_uuid, read_val['result']))
                        str_value = ""
                        for val in read_val['result']:
                            str_value += chr(val)
                        self.log.info("\t\tstr val: {}".format(str_value))
                    except Exception as err:
                        self.log.error(err)
                        pass
        except Exception as err:
            self.log.error(fail_err.forma(err))

    def _perform_read_all_descriptors(self, peer_identifier):
        fail_err = "Failed to read all characteristics with: {}"
        try:
            services = self.device.gattc_lib.listServices(peer_identifier)
            for service in services['result']:
                service_id = service['id']
                service_uuid = service['uuid_type']
                self.device.gattc_lib.connectToService(peer_identifier,
                                                       service_id)
                chars = self.device.gattc_lib.discoverCharacteristics()
                self.log.info(
                    "Reading descs in service uuid: {}".format(service_uuid))

                for char in chars['result']:
                    char_id = char['id']
                    char_uuid = char['uuid_type']
                    descriptors = char['descriptors']
                    self.log.info(
                        "\tReading descs in char uuid: {}".format(char_uuid))
                    for desc in descriptors:
                        desc_id = desc["id"]
                        desc_uuid = desc["uuid_type"]
                    try:
                        read_val = self.device.gattc_lib.readDescriptorById(
                            desc_id)
                        self.log.info(
                            "\t\tDescriptor uuid / Value: {} / {}".format(
                                desc_uuid, read_val['result']))
                    except Exception as err:
                        pass
        except Exception as err:
            self.log.error(fail_err.format(err))

    def init_pair(self, peer_identifier, security_level, non_bondable,
                  transport):
        """ Send an outgoing pairing request the input peer_identifier.

        Android currently does not support setting various security levels or
        bondable modes. Making them available for other bluetooth_device
        variants. Depending on the Address type, Android will figure out the
        transport to pair automatically.

        Args:
            peer_identifier: A string representing the device id.
            security_level: The security level required for this pairing request
                represented as a u64. (Only for LE pairing)
                Available Values
                1 - ENCRYPTED: Encrypted without MITM protection
                    (unauthenticated)
                2 - AUTHENTICATED: Encrypted with MITM protection
                    (authenticated)
                None: No pairing security level.
            non_bondable: A bool representing whether the pairing mode is
                bondable or not. None is also accepted. False if bondable, True
                if non-bondable
            transport: A u64 representing the transport type.
                Available Values
                1 - BREDR: Classic BR/EDR transport
                2 - LE: Bluetooth Low Energy Transport
        Returns:
            True if successful, False if failed.
        """
        try:
            self.device.btc_lib.pair(peer_identifier, security_level,
                                     non_bondable, transport)
            return True
        except Exception as err:
            fail_err = "Failed to pair to peer_identifier {} with: {}".format(
                peer_identifier)
            self.log.error(fail_err.format(err))
