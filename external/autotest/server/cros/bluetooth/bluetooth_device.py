# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import logging
from datetime import datetime

from autotest_lib.client.bin import utils
from autotest_lib.client.cros import constants
from autotest_lib.server import autotest


class BluetoothDevice(object):
    """BluetoothDevice is a thin layer of logic over a remote DUT.

    The Autotest host object representing the remote DUT, passed to this
    class on initialization, can be accessed from its host property.

    """

    XMLRPC_BRINGUP_TIMEOUT_SECONDS = 60
    XMLRPC_LOG_PATH = '/var/log/bluetooth_xmlrpc_device.log'

    def __init__(self, device_host):
        """Construct a BluetoothDevice.

        @param device_host: host object representing a remote host.

        """
        self.host = device_host
        # Make sure the client library is on the device so that the proxy code
        # is there when we try to call it.
        client_at = autotest.Autotest(self.host)
        client_at.install()
        # Start up the XML-RPC proxy on the client.
        self._proxy = self.host.rpc_server_tracker.xmlrpc_connect(
                constants.BLUETOOTH_DEVICE_XMLRPC_SERVER_COMMAND,
                constants.BLUETOOTH_DEVICE_XMLRPC_SERVER_PORT,
                command_name=
                  constants.BLUETOOTH_DEVICE_XMLRPC_SERVER_CLEANUP_PATTERN,
                ready_test_name=
                  constants.BLUETOOTH_DEVICE_XMLRPC_SERVER_READY_METHOD,
                timeout_seconds=self.XMLRPC_BRINGUP_TIMEOUT_SECONDS,
                logfile=self.XMLRPC_LOG_PATH)

        # Get some static information about the bluetooth adapter.
        properties = self.get_adapter_properties()
        self.bluez_version = properties.get('Name')
        self.address = properties.get('Address')
        self.bluetooth_class = properties.get('Class')
        self.UUIDs = properties.get('UUIDs')


    def set_debug_log_levels(self, dispatcher_vb, newblue_vb, bluez_vb,
                             kernel_vb):
        """Enable or disable the debug logs of bluetooth

        @param dispatcher_vb: verbosity of btdispatcher debug log, either 0 or 1
        @param newblue_vb: verbosity of newblued debug log, either 0 or 1
        @param bluez_vb: verbosity of bluez debug log, either 0 or 1
        @param kernel_vb: verbosity of kernel debug log, either 0 or 1

        """
        return self._proxy.set_debug_log_levels(dispatcher_vb, newblue_vb,
                                                bluez_vb, kernel_vb)

    def log_message(self, msg, dut=True, peer=True):
        """ Log a message in DUT log and peer logs with timestamp.

        @param msg: message to be logged.
        @param dut: log message on DUT
        @param peer: log message on peer devices
        """
        try:
            # TODO(b/146671469) Implement logging to tester

            date =  datetime.strftime(datetime.now(),"%Y:%m:%d %H:%M:%S:%f")
            msg = "bluetooth autotest --- %s : %s ---" % (date, msg)
            logging.debug("Broadcasting '%s'",msg)

            if dut:
                self._proxy.log_message(msg)

            if peer:
                for chameleon in self.host.chameleon_list:
                    chameleon.log_message(msg)
        except Exception as e:
            logging.error("Exception '%s' in log_message '%s'", str(e), msg)



    def start_bluetoothd(self):
        """start bluetoothd.

        @returns: True if bluetoothd is started correctly.
                  False otherwise.

        """
        return self._proxy.start_bluetoothd()


    def stop_bluetoothd(self):
        """stop bluetoothd.

        @returns: True if bluetoothd is stopped correctly.
                  False otherwise.

        """
        return self._proxy.stop_bluetoothd()


    def is_bluetoothd_running(self):
        """Is bluetoothd running?

        @returns: True if bluetoothd is running

        """
        return self._proxy.is_bluetoothd_running()


    def reset_on(self):
        """Reset the adapter and settings and power up the adapter.

        @return True on success, False otherwise.

        """
        return self._proxy.reset_on()


    def reset_off(self):
        """Reset the adapter and settings, leave the adapter powered off.

        @return True on success, False otherwise.

        """
        return self._proxy.reset_off()


    def has_adapter(self):
        """@return True if an adapter is present, False if not."""
        return self._proxy.has_adapter()


    def set_powered(self, powered):
        """Set the adapter power state.

        @param powered: adapter power state to set (True or False).

        @return True on success, False otherwise.

        """
        return self._proxy.set_powered(powered)


    def is_powered_on(self):
        """Is the adapter powered on?

        @returns: True if the adapter is powered on

        """
        properties = self.get_adapter_properties()
        return bool(properties.get(u'Powered'))


    def get_hci(self):
        """Get hci of the adapter; normally, it is 'hci0'.

        @returns: the hci name of the adapter.

        """
        dev_info = self.get_dev_info()
        hci = (dev_info[1] if isinstance(dev_info, list) and
               len(dev_info) > 1 else None)
        return hci


    def get_address(self):
        """Get the bluetooth address of the adapter.

        An example of the bluetooth address of the adapter: '6C:29:95:1A:D4:6F'

        @returns: the bluetooth address of the adapter.

        """
        return self.address


    def get_bluez_version(self):
        """Get bluez version.

        An exmaple of bluez version: 'BlueZ 5.39'

        @returns: the bluez version

        """
        return self.bluez_version


    def get_bluetooth_class(self):
        """Get the bluetooth class of the adapter.

        An example of the bluetooth class of a chromebook: 4718852

        @returns: the bluetooth class.

        """
        return self.bluetooth_class


    def get_UUIDs(self):
        """Get the UUIDs.

        An example of UUIDs:
            [u'00001112-0000-1000-8000-00805f9b34fb',
             u'00001801-0000-1000-8000-00805f9b34fb',
             u'0000110a-0000-1000-8000-00805f9b34fb',
             u'0000111f-0000-1000-8000-00805f9b34fb',
             u'00001200-0000-1000-8000-00805f9b34fb',
             u'00001800-0000-1000-8000-00805f9b34fb']

        @returns: the list of the UUIDs.

        """
        return self.UUIDs


    def set_discoverable(self, discoverable):
        """Set the adapter discoverable state.

        @param discoverable: adapter discoverable state to set (True or False).

        @return True on success, False otherwise.

        """
        return self._proxy.set_discoverable(discoverable)


    def is_discoverable(self):
        """Is the adapter in the discoverable state?

        @return True if discoverable. False otherwise.

        """
        properties = self.get_adapter_properties()
        return properties.get('Discoverable') == 1


    def set_discoverable_timeout(self, discoverable_timeout):
        """Set the adapter DiscoverableTimeout.

        @param discoverable_timeout: adapter DiscoverableTimeout
                value to set in seconds (Integer).

        @return True on success, False otherwise.

        """
        return self._proxy.set_discoverable_timeout(discoverable_timeout)

    def get_discoverable_timeout(self):
        """Get the adapter DiscoverableTimeout.

        @return Value of property DiscoverableTimeout in seconds (Integer).

        """
        return self._proxy.get_discoverable_timeout()

    def set_pairable_timeout(self, pairable_timeout):
        """Set the adapter PairableTimeout.

        @param pairable_timeout: adapter PairableTimeout
                value to set in seconds (Integer).

        @return True on success, False otherwise.

        """
        return self._proxy.set_pairable_timeout(pairable_timeout)

    def get_pairable_timeout(self):
        """Get the adapter PairableTimeout.

        @return Value of property PairableTimeout in seconds (Integer).

        """
        return self._proxy.get_pairable_timeout()


    def set_pairable(self, pairable):
        """Set the adapter pairable state.

        @param pairable: adapter pairable state to set (True or False).

        @return True on success, False otherwise.

        """
        return self._proxy.set_pairable(pairable)


    def is_pairable(self):
        """Is the adapter in the pairable state?

        @return True if pairable. False otherwise.

        """
        properties = self.get_adapter_properties()
        return properties.get('Pairable') == 1


    def get_adapter_properties(self):
        """Read the adapter properties from the Bluetooth Daemon.

        An example of the adapter properties looks like
        {u'Name': u'BlueZ 5.35',
         u'Alias': u'Chromebook',
         u'Modalias': u'bluetooth:v00E0p2436d0400',
         u'Powered': 1,
         u'DiscoverableTimeout': 180,
         u'PairableTimeout': 0,
         u'Discoverable': 0,
         u'Address': u'6C:29:95:1A:D4:6F',
         u'Discovering': 0,
         u'Pairable': 1,
         u'Class': 4718852,
         u'UUIDs': [u'00001112-0000-1000-8000-00805f9b34fb',
                    u'00001801-0000-1000-8000-00805f9b34fb',
                    u'0000110a-0000-1000-8000-00805f9b34fb',
                    u'0000111f-0000-1000-8000-00805f9b34fb',
                    u'00001200-0000-1000-8000-00805f9b34fb',
                    u'00001800-0000-1000-8000-00805f9b34fb']}

        @return the properties as a dictionary on success,
            the value False otherwise.

        """
        return json.loads(self._proxy.get_adapter_properties())


    def read_version(self):
        """Read the version of the management interface from the Kernel.

        @return the version as a tuple of:
          ( version, revision )

        """
        return json.loads(self._proxy.read_version())


    def read_supported_commands(self):
        """Read the set of supported commands from the Kernel.

        @return set of supported commands as arrays in a tuple of:
          ( commands, events )

        """
        return json.loads(self._proxy.read_supported_commands())


    def read_index_list(self):
        """Read the list of currently known controllers from the Kernel.

        @return array of controller indexes.

        """
        return json.loads(self._proxy.read_index_list())


    def read_info(self):
        """Read the adapter information from the Kernel.

        An example of the adapter information looks like
        [u'6C:29:95:1A:D4:6F', 6, 2, 65535, 2769, 4718852, u'Chromebook', u'']

        @return the information as a tuple of:
          ( address, bluetooth_version, manufacturer_id,
            supported_settings, current_settings, class_of_device,
            name, short_name )

        """
        return json.loads(self._proxy.read_info())


    def add_device(self, address, address_type, action):
        """Add a device to the Kernel action list.

        @param address: Address of the device to add.
        @param address_type: Type of device in @address.
        @param action: Action to take.

        @return tuple of ( address, address_type ) on success,
          None on failure.

        """
        return json.loads(self._proxy.add_device(address, address_type, action))


    def remove_device(self, address, address_type):
        """Remove a device from the Kernel action list.

        @param address: Address of the device to remove.
        @param address_type: Type of device in @address.

        @return tuple of ( address, address_type ) on success,
          None on failure.

        """
        return json.loads(self._proxy.remove_device(address, address_type))

    def _decode_json_base64(self, data):
        """Load serialized JSON and then base64 decode it

        Required to handle non-ascii data
        @param data: data to be JSON and base64 decode

        @return : JSON and base64 decoded date


        """
        logging.debug("_decode_json_base64 raw data is %s", data)
        json_encoded = json.loads(data)
        logging.debug("JSON encoded data is %s", json_encoded)
        base64_decoded = utils.base64_recursive_decode(json_encoded)
        logging.debug("base64 decoded data is %s", base64_decoded)
        return base64_decoded

    def get_devices(self):
        """Read information about remote devices known to the adapter.

        An example of the device information of RN-42 looks like
        [{u'Name': u'RNBT-A96F',
          u'Alias': u'RNBT-A96F',
          u'Adapter': u'/org/bluez/hci0',
          u'LegacyPairing': 0,
          u'Paired': 1,
          u'Connected': 0,
          u'UUIDs': [u'00001124-0000-1000-8000-00805f9b34fb'],
          u'Address': u'00:06:66:75:A9:6F',
          u'Icon': u'input-mouse',
          u'Class': 1408,
          u'Trusted': 1,
          u'Blocked': 0}]

        @return the properties of each device as an array of
            dictionaries on success, the value False otherwise.

        """
        encoded_devices = self._proxy.get_devices()
        return self._decode_json_base64(encoded_devices)


    def get_device_property(self, address, prop_name):
        """Read a property of BT device by directly querying device dbus object

        @param address: Address of the device to query
        @param prop_name: Property to be queried

        @return The property if device is found and has property, None otherwise
        """

        prop_val = self._proxy.get_device_property(address, prop_name)

        # Handle dbus error case returned by xmlrpc_server.dbus_safe decorator
        if prop_val is None:
            return prop_val

        # Decode and return property value
        return self._decode_json_base64(prop_val)


    def start_discovery(self):
        """Start discovery of remote devices.

        Obtain the discovered device information using get_devices(), called
        stop_discovery() when done.

        @return (True, None) on success, (False, <error>) otherwise.

        """
        return self._proxy.start_discovery()


    def stop_discovery(self):
        """Stop discovery of remote devices.

        @return (True, None) on success, (False, <error>) otherwise.

        """
        return self._proxy.stop_discovery()

    def pause_discovery(self, system_suspend_resume=False):
        """ Pause discovery of remote devices

        @params: boolean system_suspend_resume Is this request related to
                 system suspend resume.

        @return (True, None) on success (False, <error>) otherwise
        """
        return self._proxy.pause_discovery(system_suspend_resume)

    def unpause_discovery(self, system_suspend_resume=False):
        """ Unpause discovery of remote devices

        @params: boolean system_suspend_resume Is this request related to
                 system suspend resume.

        @return (True, None) on success (False, <error>) otherwise
        """
        return self._proxy.unpause_discovery(system_suspend_resume)


    def pause_discovery(self, system_suspend_resume=False):
        """Pause discovery of remote devices.

        This pauses all device discovery sessions.

        @param system_suspend_resume: whether the
               request is related to system suspend/resume.

        @return True on success, False otherwise.

        """
        return self._proxy.pause_discovery(system_suspend_resume)


    def unpause_discovery(self, system_suspend_resume=False):
        """Unpause discovery of remote devices.

        This unpauses all device discovery sessions.

        @param system_suspend_resume: whether the
               request is related to system suspend/resume.

        @return True on success, False otherwise.

        """
        return self._proxy.unpause_discovery(system_suspend_resume)


    def is_discovering(self):
        """Is it discovering?

        @return True if it is discovering. False otherwise.

        """
        return self.get_adapter_properties().get('Discovering') == 1


    def get_dev_info(self):
        """Read raw HCI device information.

        An example of the device information looks like:
        [0, u'hci0', u'6C:29:95:1A:D4:6F', 13, 0, 1, 581900950526, 52472, 7,
         32768, 1021, 5, 96, 6, 0, 0, 151, 151, 0, 0, 0, 0, 1968, 12507]

        @return tuple of (index, name, address, flags, device_type, bus_type,
                       features, pkt_type, link_policy, link_mode,
                       acl_mtu, acl_pkts, sco_mtu, sco_pkts,
                       err_rx, err_tx, cmd_tx, evt_rx, acl_tx, acl_rx,
                       sco_tx, sco_rx, byte_rx, byte_tx) on success,
                None on failure.

        """
        return json.loads(self._proxy.get_dev_info())

    def get_supported_capabilities(self):
        """ Get the supported_capabilities of the adapter
        @returns (capabilities,None) on success (None, <error>) on failure
        """
        return self._proxy.get_supported_capabilities()

    def register_profile(self, path, uuid, options):
        """Register new profile (service).

        @param path: Path to the profile object.
        @param uuid: Service Class ID of the service as string.
        @param options: Dictionary of options for the new service, compliant
                        with BlueZ D-Bus Profile API standard.

        @return True on success, False otherwise.

        """
        return self._proxy.register_profile(path, uuid, options)


    def has_device(self, address):
        """Checks if the device with a given address exists.

        @param address: Address of the device.

        @returns: True if there is a device with that address.
                  False otherwise.

        """
        return self._proxy.has_device(address)


    def device_is_paired(self, address):
        """Checks if a device is paired.

        @param address: address of the device.

        @returns: True if device is paired. False otherwise.

        """
        return self._proxy.device_is_paired(address)


    def device_services_resolved(self, address):
        """Checks if services are resolved for a device.

        @param address: address of the device.

        @returns: True if services are resolved. False otherwise.

        """
        return self._proxy.device_services_resolved(address)


    def set_trusted(self, address, trusted=True):
        """Set the device trusted.

        @param address: The bluetooth address of the device.
        @param trusted: True or False indicating whether to set trusted or not.

        @returns: True if successful. False otherwise.

        """
        return self._proxy.set_trusted(address, trusted)


    def pair_legacy_device(self, address, pin, trusted, timeout):
        """Pairs a device with a given pin code.

        Registers an agent who handles pin code request and
        pairs a device with known pin code.

        @param address: Address of the device to pair.
        @param pin: The pin code of the device to pair.
        @param trusted: indicating whether to set the device trusted.
        @param timeout: The timeout in seconds for pairing.

        @returns: True on success. False otherwise.

        """
        return self._proxy.pair_legacy_device(address, pin, trusted, timeout)


    def remove_device_object(self, address):
        """Removes a device object and the pairing information.

        Calls RemoveDevice method to remove remote device
        object and the pairing information.

        @param address: address of the device to unpair.

        @returns: True on success. False otherwise.

        """
        return self._proxy.remove_device_object(address)


    def connect_device(self, address):
        """Connects a device.

        Connects a device if it is not connected.

        @param address: Address of the device to connect.

        @returns: True on success. False otherwise.

        """
        return self._proxy.connect_device(address)


    def device_is_connected(self, address):
        """Checks if a device is connected.

        @param address: Address of the device to check if it is connected.

        @returns: True if device is connected. False otherwise.

        """
        return self._proxy.device_is_connected(address)


    def disconnect_device(self, address):
        """Disconnects a device.

        Disconnects a device if it is connected.

        @param address: Address of the device to disconnect.

        @returns: True on success. False otherwise.

        """
        return self._proxy.disconnect_device(address)


    def btmon_start(self):
        """Start btmon monitoring."""
        self._proxy.btmon_start()


    def btmon_stop(self):
        """Stop btmon monitoring."""
        self._proxy.btmon_stop()


    def btmon_get(self, search_str='', start_str=''):
        """Get btmon output contents.

        @param search_str: only lines with search_str would be kept.
        @param start_str: all lines before the occurrence of start_str would be
                filtered.

        @returns: the recorded btmon output.

        """
        return self._proxy.btmon_get(search_str, start_str)


    def btmon_find(self, pattern_str):
        """Find if a pattern string exists in btmon output.

        @param pattern_str: the pattern string to find.

        @returns: True on success. False otherwise.

        """
        return self._proxy.btmon_find(pattern_str)


    def register_advertisement(self, advertisement_data):
        """Register an advertisement.

        Note that rpc supports only conformable types. Hence, a
        dict about the advertisement is passed as a parameter such
        that the advertisement object could be contructed on the host.

        @param advertisement_data: a dict of the advertisement for
                                   the adapter to register.

        @returns: True on success. False otherwise.

        """
        return self._proxy.register_advertisement(advertisement_data)


    def unregister_advertisement(self, advertisement_data):
        """Unregister an advertisement.

        @param advertisement_data: a dict of the advertisement to unregister.

        @returns: True on success. False otherwise.

        """
        return self._proxy.unregister_advertisement(advertisement_data)


    def set_advertising_intervals(self, min_adv_interval_ms,
                                  max_adv_interval_ms):
        """Set advertising intervals.

        @param min_adv_interval_ms: the min advertising interval in ms.
        @param max_adv_interval_ms: the max advertising interval in ms.

        @returns: True on success. False otherwise.

        """
        return self._proxy.set_advertising_intervals(min_adv_interval_ms,
                                                     max_adv_interval_ms)


    def reset_advertising(self):
        """Reset advertising.

        This includes unregister all advertisements, reset advertising
        intervals, and disable advertising.

        @returns: True on success. False otherwise.

        """
        return self._proxy.reset_advertising()


    def read_characteristic(self, uuid, address):
        """Reads the value of a gatt characteristic.

        Reads the current value of a gatt characteristic.

        @param uuid: The uuid of the characteristic to read, as a string.
        @param address: The MAC address of the remote device.

        @returns: A byte array containing the value of the if the uuid/address
                      was found in the object tree.
                  None if the uuid/address was not found in the object tree, or
                      if a DBus exception was raised by the read operation.

        """
        value = self._proxy.read_characteristic(uuid, address)
        if value is None:
            return None
        return bytearray(base64.standard_b64decode(value))


    def write_characteristic(self, uuid, address, bytes_to_write):
        """Performs a write operation on a gatt characteristic.

        Writes to a GATT characteristic on a remote device.

        @param uuid: The uuid of the characteristic to write to, as a string.
        @param address: The MAC address of the remote device, as a string.
        @param bytes_to_write: A byte array containing the data to write.

        @returns: True if the write operation does not raise an exception.
                  None if the uuid/address was not found in the object tree, or
                      if a DBus exception was raised by the write operation.

        """
        return self._proxy.write_characteristic(
            uuid, address, base64.standard_b64encode(bytes_to_write))


    def start_notify(self, address, uuid, cccd_value):
        """Starts the notification session on the gatt characteristic.

        @param address: The MAC address of the remote device.
        @param uuid: The uuid of the characteristic.
        @param cccd_value: Possible CCCD values include
               0x00 - inferred from the remote characteristic's properties
               0x01 - notification
               0x02 - indication

        @returns: True if the operation succeeds.
                  False if the characteristic is not found, or
                      if a DBus exception was raised by the operation.

        """
        return self._proxy.start_notify(address, uuid, cccd_value)


    def stop_notify(self, address, uuid):
        """Stops the notification session on the gatt characteristic.

        @param address: The MAC address of the remote device.
        @param uuid: The uuid of the characteristic.

        @returns: True if the operation succeeds.
                  False if the characteristic is not found, or
                      if a DBus exception was raised by the operation.

        """
        return self._proxy.stop_notify(address, uuid)


    def is_notifying(self, address, uuid):
        """Is the GATT characteristic in a notifying session?

        @param address: The MAC address of the remote device.
        @param uuid: The uuid of the characteristic.

        @return True if it is in a notification session. False otherwise.

        """
        return self._proxy.is_notifying(address, uuid)


    def is_characteristic_path_resolved(self, uuid, address):
        """Checks whether a characteristic is in the object tree.

        Checks whether a characteristic is curently found in the object tree.

        @param uuid: The uuid of the characteristic to search for.
        @param address: The MAC address of the device on which to search for
            the characteristic.

        @returns: True if the characteristic is found, False otherwise.

        """
        return self._proxy.is_characteristic_path_resolved(uuid, address)


    def get_gatt_attributes_map(self, address):
        """Return a JSON formated string of the GATT attributes of a device,
        keyed by UUID
        @param address: a string of the MAC address of the device

        @return: JSON formated string, stored the nested structure of the
        attributes. Each attribute has 'path' and ['chrcs' | 'descs'], which
        store their object path and children respectively.
        """
        return self._proxy.get_gatt_attributes_map(address)


    def get_gatt_service_property(self, object_path, property_name):
        """Get property from a service attribute
        @param object_path: a string of the object path of the service
        @param property_name: a string of a property, ex: 'Value', 'UUID'

        @return: the property if success,
                 None otherwise
        """
        return self._proxy.get_gatt_service_property(object_path, property_name)


    def get_gatt_characteristic_property(self, object_path, property_name):
        """Get property from a characteristic attribute
        @param object_path: a string of the object path of the characteristic
        @param property_name: a string of a property, ex: 'Value', 'UUID'

        @return: the property if success,
                 None otherwise
        """
        return self._proxy.get_gatt_characteristic_property(object_path,
                                                            property_name)


    def get_gatt_descriptor_property(self, object_path, property_name):
        """Get property from a descriptor attribute
        @param object_path: a string of the object path of the descriptor
        @param property_name: a string of a property, ex: 'Value', 'UUID'

        @return: the property if success,
                 None otherwise
        """
        return self._proxy.get_gatt_descriptor_property(object_path,
                                                        property_name)


    def gatt_characteristic_read_value(self, uuid, object_path):
        """Perform method ReadValue on a characteristic attribute
        @param uuid: a string of uuid
        @param object_path: a string of the object path of the characteristic

        @return: base64 string of dbus bytearray
        """
        return self._proxy.gatt_characteristic_read_value(uuid, object_path)


    def gatt_descriptor_read_value(self, uuid, object_path):
        """Perform method ReadValue on a descriptor attribute
        @param uuid: a string of uuid
        @param object_path: a string of the object path of the descriptor

        @return: base64 string of dbus bytearray
        """
        return self._proxy.gatt_descriptor_read_value(uuid, object_path)


    def copy_logs(self, destination):
        """Copy the logs generated by this device to a given location.

        @param destination: destination directory for the logs.

        """
        self.host.collect_logs(self.XMLRPC_LOG_PATH, destination)


    def get_connection_info(self, address):
        """Get device connection info.

        @param address: The MAC address of the device.

        @returns: On success, a tuple of:
                      ( RSSI, transmit_power, max_transmit_power )
                  None otherwise.

        """
        return self._proxy.get_connection_info(address)


    def set_discovery_filter(self, filter):
        """Set the discovery filter.

        @param filter: The discovery filter to set.

        @return True on success, False otherwise.

        """
        return self._proxy.set_discovery_filter(filter)


    def set_le_connection_parameters(self, address, parameters):
        """Set the LE connection parameters.

        @param address: The MAC address of the device.
        @param parameters: The LE connection parameters to set.

        @return: True on success. False otherwise.

        """
        return self._proxy.set_le_connection_parameters(address, parameters)


    def close(self, close_host=True):
        """Tear down state associated with the client.

        @param close_host: If True, shut down the xml rpc server by closing the
            underlying host object (which also shuts down all other xml rpc
            servers running on the DUT). Otherwise, only shut down the
            bluetooth device xml rpc server, which can be desirable if the host
            object and/or other xml rpc servers need to be used afterwards.
        """
        # Turn off the discoverable flag since it may affect future tests.
        self._proxy.set_discoverable(False)
        # Reset the adapter and leave it on.
        self._proxy.reset_on()
        # This kills the RPC server.
        if close_host:
          self.host.close()
        else:
          self.host.rpc_server_tracker.disconnect(
              constants.BLUETOOTH_DEVICE_XMLRPC_SERVER_PORT)
