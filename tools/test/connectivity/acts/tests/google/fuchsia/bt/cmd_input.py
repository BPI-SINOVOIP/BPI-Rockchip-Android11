#!/usr/bin/env python3
#
# Copyright (C) 2019 The Android Open Source Project
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
"""
Python script for wrappers to various libraries.

Class CmdInput inherts from the cmd library.

Functions that start with "do_" have a method
signature that doesn't match the actual command
line command and that is intended. This is so the
"help" command knows what to display (in this case
the documentation of the command itself).

For example:
Looking at the function "do_tool_set_target_device_name"
has the inputs self and line which is expected of this type
of method signature. When the "help" command is done on the
method name you get the function documentation as such:

(Cmd) help tool_set_target_device_name

        Description: Reset the target device name.
        Input(s):
            device_name: Required. The advertising name to connect to.
        Usage: tool_set_target_device_name new_target_device name
          Examples:
            tool_set_target_device_name le_watch

This is all to say this documentation pattern is expected.

"""

from acts.test_utils.abstract_devices.bluetooth_device import create_bluetooth_device
from acts.test_utils.bt.bt_constants import bt_attribute_values
from acts.test_utils.bt.bt_constants import sig_uuid_constants

import acts.test_utils.bt.gatt_test_database as gatt_test_database

import cmd
import pprint
import time
"""Various Global Strings"""
CMD_LOG = "CMD {} result: {}"
FAILURE = "CMD {} threw exception: {}"
BASIC_ADV_NAME = "fs_test"


class CmdInput(cmd.Cmd):
    ble_advertise_interval = 1000
    bt_control_ids = []
    bt_control_names = []
    bt_control_devices = []
    bt_scan_poll_timer = 0.5
    target_device_name = ""
    le_ids = []
    unique_mac_addr_id = None

    def setup_vars(self, dut, target_device_name, log):
        self.pri_dut = dut
        # Note: test_dut is the start of a slow conversion from a Fuchsia specific
        # Tool to an abstract_device tool. Only commands that use test_dut will work
        # Otherwise this tool is primarially targeted at Fuchsia devices.
        self.test_dut = create_bluetooth_device(self.pri_dut)
        self.test_dut.initialize_bluetooth_controller()
        self.target_device_name = target_device_name
        self.log = log

    def emptyline(self):
        pass

    def do_EOF(self, line):
        "End Script"
        return True

    """ Useful Helper functions and cmd line tooling """
    def str_to_bool(self, s):
        if s.lower() == 'true':
            return True
        elif s.lower() == 'false':
            return False

    def _find_unique_id_over_le(self):
        scan_filter = {"name_substring": self.target_device_name}
        self.unique_mac_addr_id = None
        self.pri_dut.gattc_lib.bleStartBleScan(scan_filter)
        tries = 10
        for i in range(tries):
            time.sleep(self.bt_scan_poll_timer)
            scan_res = self.pri_dut.gattc_lib.bleGetDiscoveredDevices(
            )['result']
            for device in scan_res:
                name, did, connectable = device["name"], device["id"], device[
                    "connectable"]
                if (self.target_device_name in name):
                    self.unique_mac_addr_id = did
                    self.log.info(
                        "Successfully found device: name, id: {}, {}".format(
                            name, did))
                    break
            if self.unique_mac_addr_id:
                break
        self.pri_dut.gattc_lib.bleStopBleScan()

    def _find_unique_id_over_bt_control(self):
        self.unique_mac_addr_id = None
        self.bt_control_devices = []
        self.pri_dut.btc_lib.requestDiscovery(True)
        tries = 10
        for i in range(tries):
            if self.unique_mac_addr_id:
                break
            time.sleep(self.bt_scan_poll_timer)
            device_list = self.pri_dut.btc_lib.getKnownRemoteDevices(
            )['result']
            for id_dict in device_list:
                device = device_list[id_dict]
                self.bt_control_devices.append(device)
                name = None
                if device['name'] is not None:
                    name = device['name']
                did, address = device['id'], device['address']

                self.bt_control_ids.append(did)
                if name is not None:
                    self.bt_control_names.append(name)
                    if self.target_device_name in name:
                        self.unique_mac_addr_id = did
                        self.log.info(
                            "Successfully found device: name, id, address: {}, {}, {}"
                            .format(name, did, address))
                        break
        self.pri_dut.btc_lib.requestDiscovery(False)

    def do_tool_take_bt_snoop_log(self, custom_name):
        """
        Description: Takes the bt snoop log from the Fuchsia device.
        Logs will show up in your config files' logpath directory.

        Input(s):
            custom_name: Optional. Override the default pcap file name.

        Usage: tool_set_target_device_name new_target_device name
          Examples:
            tool_take_bt_snoop_log connection_error
            tool_take_bt_snoop_log
        """
        self.pri_dut.take_bt_snoop_log(custom_name)

    def do_tool_refresh_unique_id(self, line):
        """
        Description: Refresh command line tool mac unique id.
        Usage:
          Examples:
            tool_refresh_unique_id
        """
        try:
            self._find_unique_id_over_le()
        except Exception as err:
            self.log.error(
                "Failed to scan or find scan result: {}".format(err))

    def do_tool_refresh_unique_id_using_bt_control(self, line):
        """
        Description: Refresh command line tool mac unique id.
        Usage:
          Examples:
            tool_refresh_unique_id_using_bt_control
        """
        try:
            self._find_unique_id_over_bt_control()
        except Exception as err:
            self.log.error(
                "Failed to scan or find scan result: {}".format(err))

    def do_tool_set_target_device_name(self, line):
        """
        Description: Reset the target device name.
        Input(s):
            device_name: Required. The advertising name to connect to.
        Usage: tool_set_target_device_name new_target_device name
          Examples:
            tool_set_target_device_name le_watch
        """
        self.log.info("Setting target_device_name to: {}".format(line))
        self.target_device_name = line

    """Begin BLE advertise wrappers"""
    def do_ble_start_generic_connectable_advertisement(self, line):
        """
        Description: Start a connectable LE advertisement
        Usage: ble_start_generic_connectable_advertisement
        """
        cmd = "Start a connectable LE advertisement"
        try:
            adv_data = {"name": BASIC_ADV_NAME}
            self.pri_dut.ble_lib.bleStartBleAdvertising(
                adv_data, self.ble_advertise_interval)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_ble_start_generic_nonconnectable_advertisement(self, line):
        """
        Description: Start a non-connectable LE advertisement
        Usage: ble_start_generic_nonconnectable_advertisement
        """
        cmd = "Start a nonconnectable LE advertisement"
        try:
            adv_data = {"name": BASIC_ADV_NAME}
            self.pri_dut.ble_lib.bleStartBleAdvertising(
                adv_data, self.ble_advertise_interval, False)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_ble_stop_advertisement(self, line):
        """
        Description: Stop a BLE advertisement.
        Usage: ble_stop_advertisement
        """
        cmd = "Stop a connectable LE advertisement"
        try:
            self.pri_dut.ble_lib.bleStopBleAdvertising()
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    """End BLE advertise wrappers"""
    """Begin GATT client wrappers"""
    def complete_gattc_connect_by_id(self, text, line, begidx, endidx):
        if not text:
            completions = list(self.le_ids)[:]
        else:
            completions = [s for s in self.le_ids if s.startswith(text)]
        return completions

    def do_gattc_connect_by_id(self, line):
        """
        Description: Connect to a LE peripheral.
        Input(s):
            device_id: Required. The unique device ID from Fuchsia
                discovered devices.
        Usage:
          Examples:
            gattc_connect device_id
        """
        cmd = "Connect to a LE peripheral by input ID."
        try:

            connection_status = self.pri_dut.gattc_lib.bleConnectToPeripheral(
                line)
            self.log.info("Connection status: {}".format(
                pprint.pformat(connection_status)))
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_connect(self, line):
        """
        Description: Connect to a LE peripheral.
        Optional input: device_name
        Input(s):
            device_name: Optional. The peripheral ID to connect to.
        Usage:
          Examples:
            gattc_connect
            gattc_connect eddystone_123
        """
        cmd = "Connect to a LE peripheral."
        try:
            if len(line) > 0:
                self.target_device_name = line
                self.unique_mac_addr_id = None
            if not self.unique_mac_addr_id:
                try:
                    self._find_unique_id()
                except Exception as err:
                    self.log.info("Failed to scan or find device.")
                    return
            connection_status = self.pri_dut.gattc_lib.bleConnectToPeripheral(
                self.unique_mac_addr_id)
            self.log.info("Connection status: {}".format(
                pprint.pformat(connection_status)))
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_connect_disconnect_iterations(self, line):
        """
        Description: Connect then disconnect to a LE peripheral multiple times.
        Input(s):
            iterations: Required. The number of iterations to run.
        Usage:
          Examples:
            gattc_connect_disconnect_iterations 10
        """
        cmd = "Connect to a LE peripheral."
        try:
            if not self.unique_mac_addr_id:
                try:
                    self._find_unique_id()
                except Exception as err:
                    self.log.info("Failed to scan or find device.")
                    return
            for i in range(int(line)):
                self.log.info("Running iteration {}".format(i + 1))
                connection_status = self.pri_dut.gattc_lib.bleConnectToPeripheral(
                    self.unique_mac_addr_id)
                self.log.info("Connection status: {}".format(
                    pprint.pformat(connection_status)))
                time.sleep(4)
                disc_status = self.pri_dut.gattc_lib.bleDisconnectPeripheral(
                    self.unique_mac_addr_id)
                self.log.info("Disconnect status: {}".format(disc_status))
                time.sleep(3)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_disconnect(self, line):
        """
        Description: Disconnect from LE peripheral.
        Assumptions: Already connected to a peripheral.
        Usage:
          Examples:
            gattc_disconnect
        """
        cmd = "Disconenct from LE peripheral."
        try:
            disconnect_status = self.pri_dut.gattc_lib.bleDisconnectPeripheral(
                self.unique_mac_addr_id)
            self.log.info("Disconnect status: {}".format(disconnect_status))
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_list_services(self, line):
        """
        Description: List services from LE peripheral.
        Assumptions: Already connected to a peripheral.
        Usage:
          Examples:
            gattc_list_services
        """
        cmd = "List services from LE peripheral."
        try:
            services = self.pri_dut.gattc_lib.listServices(
                self.unique_mac_addr_id)
            self.log.info("Discovered Services: \n{}".format(
                pprint.pformat(services)))
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_connect_to_service(self, line):
        """
        Description: Connect to Peripheral GATT server service.
        Assumptions: Already connected to peripheral.
        Input(s):
            service_id: Required. The service id reference on the GATT server.
        Usage:
          Examples:
            gattc_connect_to_service service_id
        """
        cmd = "GATT client connect to GATT server service."
        try:
            self.pri_dut.gattc_lib.connectToService(self.unique_mac_addr_id,
                                                    int(line))
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_discover_characteristics(self, line):
        """
        Description: Discover characteristics from a connected service.
        Assumptions: Already connected to a GATT server service.
        Usage:
          Examples:
            gattc_discover_characteristics
        """
        cmd = "Discover and list characteristics from a GATT server."
        try:
            chars = self.pri_dut.gattc_lib.discoverCharacteristics()
            self.log.info("Discovered chars:\n{}".format(
                pprint.pformat(chars)))
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_notify_all_chars(self, line):
        """
        Description: Enable all notifications on all Characteristics on
            a GATT server.
        Assumptions: Basic GATT connection made.
        Usage:
          Examples:
            gattc_notify_all_chars
        """
        cmd = "Read all characteristics from the GATT service."
        try:
            services = self.pri_dut.gattc_lib.listServices(
                self.unique_mac_addr_id)
            for service in services['result']:
                service_id = service['id']
                service_uuid = service['uuid_type']
                self.pri_dut.gattc_lib.connectToService(
                    self.unique_mac_addr_id, service_id)
                chars = self.pri_dut.gattc_lib.discoverCharacteristics()
                print("Reading chars in service uuid: {}".format(service_uuid))

                for char in chars['result']:
                    char_id = char['id']
                    char_uuid = char['uuid_type']
                    # quick char filter for apple-4 test... remove later
                    print("found uuid {}".format(char_uuid))
                    try:
                        self.pri_dut.gattc_lib.enableNotifyCharacteristic(
                            char_id)
                    except Exception as err:
                        print("error enabling notification")
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_read_all_chars(self, line):
        """
        Description: Read all Characteristic values from a GATT server across
            all services.
        Assumptions: Basic GATT connection made.
        Usage:
          Examples:
            gattc_read_all_chars
        """
        cmd = "Read all characteristics from the GATT service."
        try:
            services = self.pri_dut.gattc_lib.listServices(
                self.unique_mac_addr_id)
            for service in services['result']:
                service_id = service['id']
                service_uuid = service['uuid_type']
                self.pri_dut.gattc_lib.connectToService(
                    self.unique_mac_addr_id, service_id)
                chars = self.pri_dut.gattc_lib.discoverCharacteristics()
                print("Reading chars in service uuid: {}".format(service_uuid))

                for char in chars['result']:
                    char_id = char['id']
                    char_uuid = char['uuid_type']
                    try:
                        read_val =  \
                            self.pri_dut.gattc_lib.readCharacteristicById(
                                char_id)
                        print("  Characteristic uuid / Value: {} / {}".format(
                            char_uuid, read_val['result']))
                        str_value = ""
                        for val in read_val['result']:
                            str_value += chr(val)
                        print("    str val: {}".format(str_value))
                    except Exception as err:
                        print(err)
                        pass
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_read_all_desc(self, line):
        """
        Description: Read all Descriptors values from a GATT server across
            all services.
        Assumptions: Basic GATT connection made.
        Usage:
          Examples:
            gattc_read_all_chars
        """
        cmd = "Read all descriptors from the GATT service."
        try:
            services = self.pri_dut.gattc_lib.listServices(
                self.unique_mac_addr_id)
            for service in services['result']:
                service_id = service['id']
                service_uuid = service['uuid_type']
                self.pri_dut.gattc_lib.connectToService(
                    self.unique_mac_addr_id, service_id)
                chars = self.pri_dut.gattc_lib.discoverCharacteristics()
                print("Reading descs in service uuid: {}".format(service_uuid))

                for char in chars['result']:
                    char_id = char['id']
                    char_uuid = char['uuid_type']
                    descriptors = char['descriptors']
                    print("  Reading descs in char uuid: {}".format(char_uuid))
                    for desc in descriptors:
                        desc_id = desc["id"]
                        desc_uuid = desc["uuid_type"]
                    try:
                        read_val = self.pri_dut.gattc_lib.readDescriptorById(
                            desc_id)
                        print("    Descriptor uuid / Value: {} / {}".format(
                            desc_uuid, read_val['result']))
                    except Exception as err:
                        pass
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_write_all_desc(self, line):
        """
        Description: Write a value to all Descriptors on the GATT server.
        Assumptions: Basic GATT connection made.
        Input(s):
            offset: Required. The offset to start writing to.
            size: Required. The size of bytes to write (value will be generated).
                IE: Input of 5 will send a byte array of [00, 01, 02, 03, 04]
        Usage:
          Examples:
            gattc_write_all_desc 0 100
            gattc_write_all_desc 10 2
        """
        cmd = "Read all descriptors from the GATT service."
        try:
            args = line.split()
            if len(args) != 2:
                self.log.info("2 Arguments required: [Offset] [Size]")
                return
            offset = int(args[0])
            size = args[1]
            write_value = []
            for i in range(int(size)):
                write_value.append(i % 256)
            services = self.pri_dut.gattc_lib.listServices(
                self.unique_mac_addr_id)
            for service in services['result']:
                service_id = service['id']
                service_uuid = service['uuid_type']
                self.pri_dut.gattc_lib.connectToService(
                    self.unique_mac_addr_id, service_id)
                chars = self.pri_dut.gattc_lib.discoverCharacteristics()
                print("Writing descs in service uuid: {}".format(service_uuid))

                for char in chars['result']:
                    char_id = char['id']
                    char_uuid = char['uuid_type']
                    descriptors = char['descriptors']
                    print("  Reading descs in char uuid: {}".format(char_uuid))
                    for desc in descriptors:
                        desc_id = desc["id"]
                        desc_uuid = desc["uuid_type"]
                    try:
                        write_val = self.pri_dut.gattc_lib.writeDescriptorById(
                            desc_id, offset, write_value)
                        print("    Descriptor uuid / Result: {} / {}".format(
                            desc_uuid, write_val['result']))
                    except Exception as err:
                        pass
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_read_all_long_desc(self, line):
        """
        Description: Read all long Characteristic Descriptors
        Assumptions: Basic GATT connection made.
        Input(s):
            offset: Required. The offset to start reading from.
            max_bytes: Required. The max size of bytes to return.
        Usage:
          Examples:
            gattc_read_all_long_desc 0 100
            gattc_read_all_long_desc 10 20
        """
        cmd = "Read all long descriptors from the GATT service."
        try:
            args = line.split()
            if len(args) != 2:
                self.log.info("2 Arguments required: [Offset] [Size]")
                return
            offset = int(args[0])
            max_bytes = int(args[1])
            services = self.pri_dut.ble_lib.bleListServices(
                self.unique_mac_addr_id)
            for service in services['result']:
                service_id = service['id']
                service_uuid = service['uuid_type']
                self.pri_dut.gattc_lib.connectToService(
                    self.unique_mac_addr_id, service_id)
                chars = self.pri_dut.gattc_lib.discoverCharacteristics()
                print("Reading descs in service uuid: {}".format(service_uuid))

                for char in chars['result']:
                    char_id = char['id']
                    char_uuid = char['uuid_type']
                    descriptors = char['descriptors']
                    print("  Reading descs in char uuid: {}".format(char_uuid))
                    for desc in descriptors:
                        desc_id = desc["id"]
                        desc_uuid = desc["uuid_type"]
                    try:
                        read_val = self.pri_dut.gattc_lib.readLongDescriptorById(
                            desc_id, offset, max_bytes)
                        print("    Descriptor uuid / Result: {} / {}".format(
                            desc_uuid, read_val['result']))
                    except Exception as err:
                        pass
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_read_all_long_char(self, line):
        """
        Description: Read all long Characteristic
        Assumptions: Basic GATT connection made.
        Input(s):
            offset: Required. The offset to start reading from.
            max_bytes: Required. The max size of bytes to return.
        Usage:
          Examples:
            gattc_read_all_long_char 0 100
            gattc_read_all_long_char 10 20
        """
        cmd = "Read all long Characteristics from the GATT service."
        try:
            args = line.split()
            if len(args) != 2:
                self.log.info("2 Arguments required: [Offset] [Size]")
                return
            offset = int(args[0])
            max_bytes = int(args[1])
            services = self.pri_dut.ble_lib.bleListServices(
                self.unique_mac_addr_id)
            for service in services['result']:
                service_id = service['id']
                service_uuid = service['uuid_type']
                self.pri_dut.gattc_lib.connectToService(
                    self.unique_mac_addr_id, service_id)
                chars = self.pri_dut.gattc_lib.discoverCharacteristics()
                print("Reading chars in service uuid: {}".format(service_uuid))

                for char in chars['result']:
                    char_id = char['id']
                    char_uuid = char['uuid_type']
                    try:
                        read_val = self.pri_dut.gattc_lib.readLongCharacteristicById(
                            char_id, offset, max_bytes)
                        print("    Char uuid / Result: {} / {}".format(
                            char_uuid, read_val['result']))
                    except Exception as err:
                        pass
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_write_all_chars(self, line):
        """
        Description: Write all characteristic values from a GATT server across
            all services.
        Assumptions: Basic GATT connection made.
        Input(s):
            offset: Required. The offset to start writing on.
            size: The write value size (value will be generated)
                IE: Input of 5 will send a byte array of [00, 01, 02, 03, 04]
        Usage:
          Examples:
            gattc_write_all_chars 0 10
            gattc_write_all_chars 10 1
        """
        cmd = "Read all characteristics from the GATT service."
        try:
            args = line.split()
            if len(args) != 2:
                self.log.info("2 Arguments required: [Offset] [Size]")
                return
            offset = int(args[0])
            size = int(args[1])
            write_value = []
            for i in range(size):
                write_value.append(i % 256)
            services = self.pri_dut.gattc_lib.listServices(
                self.unique_mac_addr_id)
            for service in services['result']:
                service_id = service['id']
                service_uuid = service['uuid_type']
                self.pri_dut.gattc_lib.connectToService(
                    self.unique_mac_addr_id, service_id)
                chars = self.pri_dut.gattc_lib.discoverCharacteristics()
                print("Writing chars in service uuid: {}".format(service_uuid))

                for char in chars['result']:
                    char_id = char['id']
                    char_uuid = char['uuid_type']
                    try:
                        write_result = self.pri_dut.gattc_lib.writeCharById(
                            char_id, offset, write_value)
                        print("  Characteristic uuid write result: {} / {}".
                              format(char_uuid, write_result['result']))
                    except Exception as err:
                        print("error writing char {}".format(err))
                        pass
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_write_all_chars_without_response(self, line):
        """
        Description: Write all characteristic values from a GATT server across
            all services.
        Assumptions: Basic GATT connection made.
        Input(s):
            size: The write value size (value will be generated).
                IE: Input of 5 will send a byte array of [00, 01, 02, 03, 04]
        Usage:
          Examples:
            gattc_write_all_chars_without_response 100
        """
        cmd = "Read all characteristics from the GATT service."
        try:
            args = line.split()
            if len(args) != 1:
                self.log.info("1 Arguments required: [Size]")
                return
            size = int(args[0])
            write_value = []
            for i in range(size):
                write_value.append(i % 256)
            services = self.pri_dut.gattc_lib.listServices(
                self.unique_mac_addr_id)
            for service in services['result']:
                service_id = service['id']
                service_uuid = service['uuid_type']
                self.pri_dut.gattc_lib.connectToService(
                    self.unique_mac_addr_id, service_id)
                chars = self.pri_dut.gattc_lib.discoverCharacteristics()
                print("Reading chars in service uuid: {}".format(service_uuid))

                for char in chars['result']:
                    char_id = char['id']
                    char_uuid = char['uuid_type']
                    try:
                        write_result = \
                            self.pri_dut.gattc_lib.writeCharByIdWithoutResponse(
                                char_id, write_value)
                        print("  Characteristic uuid write result: {} / {}".
                              format(char_uuid, write_result['result']))
                    except Exception as err:
                        pass
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_write_char_by_id(self, line):
        """
        Description: Write char by characteristic id reference.
        Assumptions: Already connected to a GATT server service.
        Input(s):
            characteristic_id: The characteristic id reference on the GATT
                service
            offset: The offset value to use
            size: Function will generate random bytes by input size.
                IE: Input of 5 will send a byte array of [00, 01, 02, 03, 04]
        Usage:
          Examples:
            gattc_write_char_by_id char_id 0 5
            gattc_write_char_by_id char_id 20 1
        """
        cmd = "Write to GATT server characteristic ."
        try:
            args = line.split()
            if len(args) != 3:
                self.log.info("3 Arguments required: [Id] [Offset] [Size]")
                return
            id = int(args[0], 16)
            offset = int(args[1])
            size = int(args[2])
            write_value = []
            for i in range(size):
                write_value.append(i % 256)
            self.test_dut.gatt_client_write_characteristic_by_handle(
                self.unique_mac_addr_id, id, offset, write_value)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_write_char_by_id_without_response(self, line):
        """
        Description: Write char by characteristic id reference without response.
        Assumptions: Already connected to a GATT server service.
        Input(s):
            characteristic_id: The characteristic id reference on the GATT
                service
            size: Function will generate random bytes by input size.
                IE: Input of 5 will send a byte array of [00, 01, 02, 03, 04]
        Usage:
          Examples:
            gattc_write_char_by_id_without_response char_id 5
        """
        cmd = "Write characteristic by id without response."
        try:
            args = line.split()
            if len(args) != 2:
                self.log.info("2 Arguments required: [Id] [Size]")
                return
            id = int(args[0], 16)
            size = args[1]
            write_value = []
            for i in range(int(size)):
                write_value.append(i % 256)
            self.test_dut.gatt_client_write_characteristic_without_response_by_handle(
                self.unique_mac_addr_id, id, write_value)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_enable_notify_char_by_id(self, line):
        """
        Description: Enable Characteristic notification on Characteristic ID.
        Assumptions: Already connected to a GATT server service.
        Input(s):
            characteristic_id: The characteristic id reference on the GATT
                service
        Usage:
          Examples:
            gattc_enable_notify_char_by_id char_id
        """
        cmd = "Enable notifications by Characteristic id."
        try:
            id = int(line, 16)
            self.test_dut.gatt_client_enable_notifiy_characteristic_by_handle(
                self.unique_mac_addr_id, id)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_disable_notify_char_by_id(self, line):
        """
        Description: Disable Characteristic notification on Characteristic ID.
        Assumptions: Already connected to a GATT server service.
        Input(s):
            characteristic_id: The characteristic id reference on the GATT
                service
        Usage:
          Examples:
            gattc_disable_notify_char_by_id char_id
        """
        cmd = "Disable notify Characteristic by id."
        try:
            id = int(line, 16)
            self.test_dut.gatt_client_disable_notifiy_characteristic_by_handle(
                self.unique_mac_addr_id, id)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_read_char_by_id(self, line):
        """
        Description: Read Characteristic by ID.
        Assumptions: Already connected to a GATT server service.
        Input(s):
            characteristic_id: The characteristic id reference on the GATT
                service
        Usage:
          Examples:
            gattc_read_char_by_id char_id
        """
        cmd = "Read Characteristic value by ID."
        try:
            id = int(line, 16)
            read_val = self.test_dut.gatt_client_read_characteristic_by_handle(
                self.unique_mac_addr_id, id)
            self.log.info("Characteristic Value with id {}: {}".format(
                id, read_val))
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_write_desc_by_id(self, line):
        """
        Description: Write Descriptor by characteristic id reference.
        Assumptions: Already connected to a GATT server service.
        Input(s):
            descriptor_id: The Descriptor id reference on the GATT service
            offset: The offset value to use
            size: Function will generate random bytes by input size.
                IE: Input of 5 will send a byte array of [00, 01, 02, 03, 04]
        Usage:
          Examples:
            gattc_write_desc_by_id desc_id 0 5
            gattc_write_desc_by_id desc_id 20 1
        """
        cmd = "Write Descriptor by id."
        try:
            args = line.split()
            id = int(args[0], 16)
            offset = int(args[1])
            size = args[2]
            write_value = []
            for i in range(int(size)):
                write_value.append(i % 256)
            write_result = self.test_dut.gatt_client_write_descriptor_by_handle(
                self.unique_mac_addr_id, id, offset, write_value)
            self.log.info("Descriptor Write result {}: {}".format(
                id, write_result))
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_read_desc_by_id(self, line):
        """
        Description: Read Descriptor by ID.
        Assumptions: Already connected to a GATT server service.
        Input(s):
            descriptor_id: The Descriptor id reference on the GATT service
        Usage:
          Examples:
            gattc_read_desc_by_id desc_id
        """
        cmd = "Read Descriptor by ID."
        try:
            id = int(line, 16)
            read_val = self.test_dut.gatt_client_read_descriptor_by_handle(
                self.unique_mac_addr_id, id)
            self.log.info("Descriptor Value with id {}: {}".format(
                id, read_val))
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_gattc_read_long_char_by_id(self, line):
        """
        Description: Read long Characteristic value by id.
        Assumptions: Already connected to a GATT server service.
        Input(s):
            characteristic_id: The characteristic id reference on the GATT
                service
            offset: The offset value to use.
            max_bytes: The max bytes size to return.
        Usage:
          Examples:
            gattc_read_long_char_by_id char_id 0 10
            gattc_read_long_char_by_id char_id 20 1
        """
        cmd = "Read long Characteristic value by id."
        try:
            args = line.split()
            if len(args) != 3:
                self.log.info("3 Arguments required: [Id] [Offset] [Size]")
                return
            id = int(args[0], 16)
            offset = int(args[1])
            max_bytes = int(args[2])
            read_val = self.pri_dut.gattc_lib.readLongCharacteristicById(
                id, offset, max_bytes)
            self.log.info("Characteristic Value with id {}: {}".format(
                id, read_val['result']))

        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    """End GATT client wrappers"""
    """Begin LE scan wrappers"""
    def _update_scan_results(self, scan_results):
        self.le_ids = []
        for scan in scan_results['result']:
            self.le_ids.append(scan['id'])

    def do_ble_start_scan(self, line):
        """
        Description: Perform a BLE scan.
        Default filter name: ""
        Optional input: filter_device_name
        Usage:
          Examples:
            ble_start_scan
            ble_start_scan eddystone
        """
        cmd = "Perform a BLE scan and list discovered devices."
        try:
            scan_filter = {"name_substring": ""}
            if line:
                scan_filter = {"name_substring": line}
            self.pri_dut.gattc_lib.bleStartBleScan(scan_filter)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_ble_stop_scan(self, line):
        """
        Description: Stops a BLE scan and returns discovered devices.
        Usage:
          Examples:
            ble_stop_scan
        """
        cmd = "Stops a BLE scan and returns discovered devices."
        try:
            scan_results = self.pri_dut.gattc_lib.bleStopBleScan()
            self._update_scan_results(scan_results)
            self.log.info(pprint.pformat(scan_results))
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_ble_get_discovered_devices(self, line):
        """
        Description: Get discovered LE devices of an active scan.
        Usage:
          Examples:
            ble_stop_scan
        """
        cmd = "Get discovered LE devices of an active scan."
        try:
            scan_results = self.pri_dut.gattc_lib.bleGetDiscoveredDevices()
            self._update_scan_results(scan_results)
            self.log.info(pprint.pformat(scan_results))
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    """End LE scan wrappers"""
    """Begin GATT Server wrappers"""
    def do_gatts_close(self, line):
        """
        Description: Close active GATT server.

        Usage:
          Examples:
            gatts_close
        """
        cmd = "Close active GATT server."
        try:
            result = self.pri_dut.gatts_lib.closeServer()
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def complete_gatts_setup_database(self, text, line, begidx, endidx):
        if not text:
            completions = list(
                gatt_test_database.GATT_SERVER_DB_MAPPING.keys())
        else:
            completions = [
                s for s in gatt_test_database.GATT_SERVER_DB_MAPPING.keys()
                if s.startswith(text)
            ]
        return completions

    def do_gatts_setup_database(self, line):
        """
        Description: Setup a Gatt server database based on pre-defined inputs.
            Supports Tab Autocomplete.
        Input(s):
            descriptor_db_name: The descriptor db name that matches one in
                acts.test_utils.bt.gatt_test_database
        Usage:
          Examples:
            gatts_setup_database LARGE_DB_1
        """
        cmd = "Setup GATT Server Database Based of pre-defined dictionaries"
        try:
            scan_results = self.pri_dut.gatts_lib.publishServer(
                gatt_test_database.GATT_SERVER_DB_MAPPING.get(line))
            self.log.info(scan_results)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    """End GATT Server wrappers"""
    """Begin Bluetooth Controller wrappers"""
    def complete_btc_pair(self, text, line, begidx, endidx):
        """ Provides auto-complete for btc_pair cmd.

        See Cmd module for full description.
        """
        arg_completion = len(line.split(" ")) - 1
        pairing_security_level_options = ['ENCRYPTED', 'AUTHENTICATED', 'NONE']
        non_bondable_options = ['BONDABLE', 'NON_BONDABLE', 'NONE']
        transport_options = ['BREDR', 'LE']
        if arg_completion == 1:
            if not text:
                completions = pairing_security_level_options
            else:
                completions = [
                    s for s in pairing_security_level_options
                    if s.startswith(text)
                ]
            return completions
        if arg_completion == 2:
            if not text:
                completions = non_bondable_options
            else:
                completions = [
                    s for s in non_bondable_options if s.startswith(text)
                ]
            return completions
        if arg_completion == 3:
            if not text:
                completions = transport_options
            else:
                completions = [
                    s for s in transport_options if s.startswith(text)
                ]
            return completions

    def do_btc_pair(self, line):
        """
        Description: Sends an outgoing pairing request.

        Input(s):
            pairing security level: ENCRYPTED, AUTHENTICATED, or NONE
            non_bondable: BONDABLE, NON_BONDABLE, or NONE
            transport: BREDR or LE

        Usage:
          Examples:
            btc_pair NONE NONE BREDR
            btc_pair ENCRYPTED NONE LE
            btc_pair AUTHENTICATED NONE LE
            btc_pair NONE NON_BONDABLE BREDR
        """
        cmd = "Send an outgoing pairing request."
        pairing_security_level_mapping = {
            "ENCRYPTED": 1,
            "AUTHENTICATED": 2,
            "NONE": None,
        }

        non_bondable_mapping = {
            "BONDABLE": False,  # Note: Reversed on purpose
            "NON_BONDABLE": True,  # Note: Reversed on purpose
            "NONE": None,
        }

        transport_mapping = {
            "BREDR": 1,
            "LE": 2,
        }

        try:
            options = line.split(" ")
            result = self.test_dut.init_pair(
                self.unique_mac_addr_id,
                pairing_security_level_mapping.get(options[0]),
                non_bondable_mapping.get(options[1]),
                transport_mapping.get(options[2]),
            )
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_btc_accept_pairing(self, line):
        """
        Description: Accept all incoming pairing requests.

        Usage:
          Examples:
            btc_accept_pairing
        """
        cmd = "Accept incoming pairing requests"
        try:
            result = self.pri_dut.btc_lib.acceptPairing()
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_btc_forget_device(self, line):
        """
        Description: Forget pairing of the current device under test.
            Current device under test is the device found by
            tool_refresh_unique_id from custom user param. This function
            will also perform a clean disconnect if actively connected.

        Usage:
          Examples:
            btc_forget_device
        """
        cmd = "For pairing of the current device under test."
        try:
            self.log.info("Forgetting device id: {}".format(
                self.unique_mac_addr_id))
            result = self.pri_dut.btc_lib.forgetDevice(self.unique_mac_addr_id)
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_btc_set_discoverable(self, discoverable):
        """
        Description: Change Bluetooth Controller discoverablility.
        Input(s):
            discoverable: true to set discoverable
                          false to set non-discoverable
        Usage:
          Examples:
            btc_set_discoverable true
            btc_set_discoverable false
        """
        cmd = "Change Bluetooth Controller discoverablility."
        try:
            result = self.test_dut.set_discoverable(
                self.str_to_bool(discoverable))
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_btc_set_name(self, name):
        """
        Description: Change Bluetooth Controller local name.
        Input(s):
            name: The name to set the Bluetooth Controller name to.

        Usage:
          Examples:
            btc_set_name fs_test
        """
        cmd = "Change Bluetooth Controller local name."
        try:
            result = self.test_dut.set_bluetooth_local_name(name)
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_btc_request_discovery(self, discover):
        """
        Description: Change whether the Bluetooth Controller is in active.
            discovery or not.
        Input(s):
            discover: true to start discovery
                      false to end discovery
        Usage:
          Examples:
            btc_request_discovery true
            btc_request_discovery false
        """
        cmd = "Change whether the Bluetooth Controller is in active."
        try:
            result = self.pri_dut.btc_lib.requestDiscovery(
                self.str_to_bool(discover))
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_btc_get_known_remote_devices(self, line):
        """
        Description: Get a list of known devices.

        Usage:
          Examples:
            btc_get_known_remote_devices
        """
        cmd = "Get a list of known devices."
        self.bt_control_devices = []
        try:
            device_list = self.pri_dut.btc_lib.getKnownRemoteDevices(
            )['result']
            for id_dict in device_list:
                device = device_list[id_dict]
                self.bt_control_devices.append(device)
                self.log.info("Device found {}".format(device))

        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_btc_forget_all_known_devices(self, line):
        """
        Description: Forget all known devices.

        Usage:
          Examples:
            btc_forget_all_known_devices
        """
        cmd = "Forget all known devices."
        try:
            device_list = self.pri_dut.btc_lib.getKnownRemoteDevices(
            )['result']
            for device in device_list:
                d = device_list[device]
                if d['bonded'] or d['connected']:
                    self.log.info("Unbonding deivce: {}".format(d))
                    self.log.info(
                        self.pri_dut.btc_lib.forgetDevice(d['id'])['result'])
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_btc_connect_device(self, line):
        """
        Description: Connect to device under test.
            Device under test is specified by either user params
            or
                tool_set_target_device_name <name>
                do_tool_refresh_unique_id_using_bt_control

        Usage:
          Examples:
            btc_connect_device
        """
        cmd = "Connect to device under test."
        try:
            result = self.pri_dut.btc_lib.connectDevice(
                self.unique_mac_addr_id)
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def complete_btc_connect_device_by_id(self, text, line, begidx, endidx):
        if not text:
            completions = list(self.bt_control_ids)[:]
        else:
            completions = [
                s for s in self.bt_control_ids if s.startswith(text)
            ]
        return completions

    def do_btc_connect_device_by_id(self, device_id):
        """
        Description: Connect to device id based on pre-defined inputs.
            Supports Tab Autocomplete.
        Input(s):
            device_id: The device id to connect to.

        Usage:
          Examples:
            btc_connect_device_by_id <device_id>
        """
        cmd = "Connect to device id based on pre-defined inputs."
        try:
            result = self.pri_dut.btc_lib.connectDevice(device_id)
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def complete_btc_connect_device_by_name(self, text, line, begidx, endidx):
        if not text:
            completions = list(self.bt_control_names)[:]
        else:
            completions = [
                s for s in self.bt_control_names if s.startswith(text)
            ]
        return completions

    def do_btc_connect_device_by_name(self, device_name):
        """
        Description: Connect to device id based on pre-defined inputs.
            Supports Tab Autocomplete.
        Input(s):
            device_id: The device id to connect to.

        Usage:
          Examples:
            btc_connect_device_by_name <device_id>
        """
        cmd = "Connect to device name based on pre-defined inputs."
        try:
            for device in self.bt_control_devices:
                if device_name is device['name']:

                    result = self.pri_dut.btc_lib.connectDevice(device['id'])
                    self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_btc_disconnect_device(self, line):
        """
        Description: Disconnect to device under test.
            Device under test is specified by either user params
            or
                tool_set_target_device_name <name>
                do_tool_refresh_unique_id_using_bt_control

        Usage:
          Examples:
            btc_disconnect_device
        """
        cmd = "Disconnect to device under test."
        try:
            result = self.pri_dut.btc_lib.disconnectDevice(
                self.unique_mac_addr_id)
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_btc_init_bluetooth_control(self, line):
        """
        Description: Initialize the Bluetooth Controller.

        Usage:
          Examples:
            btc_init_bluetooth_control
        """
        cmd = "Initialize the Bluetooth Controller."
        try:
            result = self.test_dut.initialize_bluetooth_controller()
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_btc_get_local_address(self, line):
        """
        Description: Get the local BR/EDR address of the Bluetooth Controller.

        Usage:
          Examples:
            btc_get_local_address
        """
        cmd = "Get the local BR/EDR address of the Bluetooth Controller."
        try:
            result = self.test_dut.get_local_bluetooth_address()
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_btc_input_pairing_pin(self, line):
        """
        Description: Sends a pairing pin to SL4F's Bluetooth Control's
        Pairing Delegate.

        Usage:
          Examples:
            btc_input_pairing_pin 123456
        """
        cmd = "Input pairing pin to the Fuchsia device."
        try:
            result = self.pri_dut.btc_lib.inputPairingPin(line)['result']
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_btc_get_pairing_pin(self, line):
        """
        Description: Gets the pairing pin from SL4F's Bluetooth Control's
        Pairing Delegate.

        Usage:
          Examples:
            btc_get_pairing_pin
        """
        cmd = "Get the pairing pin from the Fuchsia device."
        try:
            result = self.pri_dut.btc_lib.getPairingPin()['result']
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    """End Bluetooth Control wrappers"""
    """Begin Profile Server wrappers"""
    def do_sdp_pts_example(self, num_of_records):
        """
        Description: An example of how to setup a generic SDP record
            and SDP search capabilities. This example will pass a few
            SDP tests.

        Input(s):
            num_of_records: The number of records to add.

        Usage:
          Examples:
            sdp_pts_example 1
            sdp pts_example 10
        """
        cmd = "Setup SDP for PTS testing."
        record = {
            'service_class_uuids': ["0001"],
            'protocol_descriptors': [
                {
                    'protocol':
                    int(sig_uuid_constants['AVDTP'], 16),
                    'params': [
                        {
                            'data': 0x0103  # to indicate 1.3
                        },
                        {
                            'data': 0x0105  # to indicate 1.5
                        }
                    ]
                },
                {
                    'protocol': int(sig_uuid_constants['SDP'], 16),
                    'params': [{
                        'data': int(sig_uuid_constants['AVDTP'], 16),
                    }]
                }
            ],
            'profile_descriptors': [{
                'profile_id':
                int(sig_uuid_constants['AdvancedAudioDistribution'], 16),
                'major_version':
                1,
                'minor_version':
                3,
            }],
            'additional_protocol_descriptors': [{
                'protocol':
                int(sig_uuid_constants['L2CAP'], 16),
                'params': [
                    {
                        'data': int(sig_uuid_constants['AVDTP'], 16),
                    },
                    {
                        'data': int(sig_uuid_constants['AVCTP'], 16),
                    },
                    {
                        'data': int(sig_uuid_constants['GenericAudio'], 16),
                    },
                ]
            }],
            'information': [{
                'language': "en",
                'name': "A2DP",
                'description': "Advanced Audio Distribution Profile",
                'provider': "Fuchsia"
            }],
            'additional_attributes': [
                {
                    'id': 0x0200,
                    'element': {
                        'data': int(sig_uuid_constants['AVDTP'], 16)
                    }
                },
                {
                    'id': 0x0201,
                    'element': {
                        'data': int(sig_uuid_constants['AVDTP'], 16)
                    }
                },
            ]
        }

        attributes = [
            bt_attribute_values['ATTR_PROTOCOL_DESCRIPTOR_LIST'],
            bt_attribute_values['ATTR_SERVICE_CLASS_ID_LIST'],
            bt_attribute_values['ATTR_BLUETOOTH_PROFILE_DESCRIPTOR_LIST'],
            bt_attribute_values['ATTR_A2DP_SUPPORTED_FEATURES'],
            bt_attribute_values['ATTR_ADDITIONAL_PROTOCOL_DESCRIPTOR_LIST'],
            bt_attribute_values['ATTR_SERVICE_RECORD_HANDLE'],
        ]

        try:
            self.pri_dut.sdp_lib.addSearch(
                attributes, int(sig_uuid_constants['AudioSource'], 16))
            self.pri_dut.sdp_lib.addSearch(
                attributes,
                int(sig_uuid_constants['AdvancedAudioDistribution'], 16))
            for _ in range(int(num_of_records)):
                result = self.pri_dut.sdp_lib.addService(record)
                self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_sdp_cleanup(self, line):
        """
        Description: Cleanup any existing SDP records

        Usage:
          Examples:
            sdp_cleanup
        """
        cmd = "Cleanup SDP objects."
        try:
            result = self.pri_dut.sdp_lib.cleanUp()
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_sdp_init(self, line):
        """
        Description: Init the profile proxy for setting up SDP records

        Usage:
          Examples:
            sdp_init
        """
        cmd = "Initialize profile proxy objects for adding SDP records"
        try:
            result = self.pri_dut.sdp_lib.init()
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_sdp_connect_l2cap(self, psm):
        """
        Description: Send an l2cap connection request over an input psm value.

        Note: Must be already connected to a peer.

        Input(s):
            psm: The int hex value to connect over. Available PSMs:
                SDP 0x0001  See Bluetooth Service Discovery Protocol (SDP)
                RFCOMM  0x0003  See RFCOMM with TS 07.10
                TCS-BIN 0x0005  See Bluetooth Telephony Control Specification /
                    TCS Binary
                TCS-BIN-CORDLESS    0x0007  See Bluetooth Telephony Control
                    Specification / TCS Binary
                BNEP    0x000F  See Bluetooth Network Encapsulation Protocol
                HID_Control 0x0011  See Human Interface Device
                HID_Interrupt   0x0013  See Human Interface Device
                UPnP    0x0015  See [ESDP]
                AVCTP   0x0017  See Audio/Video Control Transport Protocol
                AVDTP   0x0019  See Audio/Video Distribution Transport Protocol
                AVCTP_Browsing  0x001B  See Audio/Video Remote Control Profile
                UDI_C-Plane 0x001D  See the Unrestricted Digital Information
                    Profile [UDI]
                ATT 0x001F  See Bluetooth Core Specification​
                ​3DSP   0x0021​ ​​See 3D Synchronization Profile.
                ​LE_PSM_IPSP    ​0x0023 ​See Internet Protocol Support Profile
                    (IPSP)
                OTS 0x0025  See Object Transfer Service (OTS)
                EATT    0x0027  See Bluetooth Core Specification

        Usage:
          Examples:
            sdp_connect_l2cap 0001
            sdp_connect_l2cap 0015
        """
        cmd = "Connect l2cap"
        try:
            result = self.pri_dut.sdp_lib.connectL2cap(self.unique_mac_addr_id,
                                                       int(psm, 16))
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    """End Profile Server wrappers"""
    """Begin AVDTP wrappers"""
    def complete_avdtp_init(self, text, line, begidx, endidx):
        roles = ["sink", "source"]
        if not text:
            completions = roles
        else:
            completions = [s for s in roles if s.startswith(text)]
        return completions

    def do_avdtp_init(self, role):
        """
        Description: Init the AVDTP and A2DP service corresponding to the input
        role.

        Input(s):
            role: The specified role. Either 'source' or 'sink'.

        Usage:
          Examples:
            avdtp_init source
            avdtp_init sink
        """
        cmd = "Initialize AVDTP proxy"
        try:
            result = self.pri_dut.avdtp_lib.init(role)
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_avdtp_kill_a2dp_sink(self, line):
        """
        Description: Quickly kill any A2DP sink service currently running on the
        device.

        Usage:
          Examples:
            avdtp_kill_a2dp_sink
        """
        cmd = "Killing A2DP sink"
        try:
            result = self.pri_dut.control_daemon("bt-a2dp-sink.cmx", "stop")
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_avdtp_kill_a2dp_source(self, line):
        """
        Description: Quickly kill any A2DP source service currently running on
        the device.

        Usage:
          Examples:
            avdtp_kill_a2dp_source
        """
        cmd = "Killing A2DP source"
        try:
            result = self.pri_dut.control_daemon("bt-a2dp-source.cmx", "stop")
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_avdtp_get_connected_peers(self, line):
        """
        Description: Get the connected peers for the AVDTP service

        Usage:
          Examples:
            avdtp_get_connected_peers
        """
        cmd = "AVDTP get connected peers"
        try:
            result = self.pri_dut.avdtp_lib.getConnectedPeers()
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_avdtp_set_configuration(self, peer_id):
        """
        Description: Send AVDTP command to connected peer: set configuration

        Input(s):
            peer_id: The specified peer_id.

        Usage:
          Examples:
            avdtp_set_configuration <peer_id>
        """
        cmd = "Send AVDTP set configuration to connected peer"
        try:
            result = self.pri_dut.avdtp_lib.setConfiguration(int(peer_id))
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_avdtp_get_configuration(self, peer_id):
        """
        Description: Send AVDTP command to connected peer: get configuration

        Input(s):
            peer_id: The specified peer_id.

        Usage:
          Examples:
            avdtp_get_configuration <peer_id>
        """
        cmd = "Send AVDTP get configuration to connected peer"
        try:
            result = self.pri_dut.avdtp_lib.getConfiguration(int(peer_id))
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_avdtp_get_capabilities(self, peer_id):
        """
        Description: Send AVDTP command to connected peer: get capabilities

        Input(s):
            peer_id: The specified peer_id.

        Usage:
          Examples:
            avdtp_get_capabilities <peer_id>
        """
        cmd = "Send AVDTP get capabilities to connected peer"
        try:
            result = self.pri_dut.avdtp_lib.getCapabilities(int(peer_id))
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_avdtp_get_all_capabilities(self, peer_id):
        """
        Description: Send AVDTP command to connected peer: get all capabilities

        Input(s):
            peer_id: The specified peer_id.

        Usage:
          Examples:
            avdtp_get_all_capabilities <peer_id>
        """
        cmd = "Send AVDTP get all capabilities to connected peer"
        try:
            result = self.pri_dut.avdtp_lib.getAllCapabilities(int(peer_id))
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_avdtp_reconfigure_stream(self, peer_id):
        """
        Description: Send AVDTP command to connected peer: reconfigure stream

        Input(s):
            peer_id: The specified peer_id.

        Usage:
          Examples:
            avdtp_reconfigure_stream <peer_id>
        """
        cmd = "Send AVDTP reconfigure stream to connected peer"
        try:
            result = self.pri_dut.avdtp_lib.reconfigureStream(int(peer_id))
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_avdtp_suspend_stream(self, peer_id):
        """
        Description: Send AVDTP command to connected peer: suspend stream

        Input(s):
            peer_id: The specified peer_id.

        Usage:
          Examples:
            avdtp_suspend_stream <peer_id>
        """
        cmd = "Send AVDTP suspend stream to connected peer"
        try:
            result = self.pri_dut.avdtp_lib.suspendStream(int(peer_id))
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_avdtp_suspend_reconfigure(self, peer_id):
        """
        Description: Send AVDTP command to connected peer: suspend reconfigure

        Input(s):
            peer_id: The specified peer_id.

        Usage:
          Examples:
            avdtp_suspend_reconfigure <peer_id>
        """
        cmd = "Send AVDTP suspend reconfigure to connected peer"
        try:
            result = self.pri_dut.avdtp_lib.suspendAndReconfigure(int(peer_id))
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_avdtp_release_stream(self, peer_id):
        """
        Description: Send AVDTP command to connected peer: release stream

        Input(s):
            peer_id: The specified peer_id.

        Usage:
          Examples:
            avdtp_release_stream <peer_id>
        """
        cmd = "Send AVDTP release stream to connected peer"
        try:
            result = self.pri_dut.avdtp_lib.releaseStream(int(peer_id))
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_avdtp_establish_stream(self, peer_id):
        """
        Description: Send AVDTP command to connected peer: establish stream

        Input(s):
            peer_id: The specified peer_id.

        Usage:
          Examples:
            avdtp_establish_stream <peer_id>
        """
        cmd = "Send AVDTP establish stream to connected peer"
        try:
            result = self.pri_dut.avdtp_lib.establishStream(int(peer_id))
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_avdtp_start_stream(self, peer_id):
        """
        Description: Send AVDTP command to connected peer: start stream

        Input(s):
            peer_id: The specified peer_id.

        Usage:
          Examples:
            avdtp_start_stream <peer_id>
        """
        cmd = "Send AVDTP start stream to connected peer"
        try:
            result = self.pri_dut.avdtp_lib.startStream(int(peer_id))
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_avdtp_abort_stream(self, peer_id):
        """
        Description: Send AVDTP command to connected peer: abort stream

        Input(s):
            peer_id: The specified peer_id.

        Usage:
          Examples:
            avdtp_abort_stream <peer_id>
        """
        cmd = "Send AVDTP abort stream to connected peer"
        try:
            result = self.pri_dut.avdtp_lib.abortStream(int(peer_id))
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    def do_avdtp_remove_service(self, line):
        """
        Description: Removes the AVDTP service in use.

        Usage:
          Examples:
            avdtp_establish_stream <peer_id>
        """
        cmd = "Remove AVDTP service"
        try:
            result = self.pri_dut.avdtp_lib.removeService()
            self.log.info(result)
        except Exception as err:
            self.log.error(FAILURE.format(cmd, err))

    """End AVDTP wrappers"""
