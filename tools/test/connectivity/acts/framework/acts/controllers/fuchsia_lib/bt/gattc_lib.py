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

from acts.controllers.fuchsia_lib.base_lib import BaseLib


class FuchsiaGattcLib(BaseLib):
    def __init__(self, addr, tc, client_id):
        self.address = addr
        self.test_counter = tc
        self.client_id = client_id

    def bleStartBleScan(self, scan_filter):
        """Starts a BLE scan

        Args:
            scan_time_ms: int, Amount of time to scan for.
            scan_filter: dictionary, Device filter for a scan.
            scan_count: int, Number of devices to scan for before termination.

        Returns:
            None if pass, err if fail.
        """
        test_cmd = "gatt_client_facade.BleStartScan"
        test_args = {
            "filter": scan_filter,
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def bleStopBleScan(self):
        """Stops a BLE scan

        Returns:
            Dictionary, List of devices discovered, error string if error.
        """
        test_cmd = "gatt_client_facade.BleStopScan"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def listServices(self, id):
        """Lists services of a peripheral specified by id.

        Args:
            id: string, Peripheral identifier to list services.

        Returns:
            Dictionary, List of Service Info if success, error string if error.
        """
        test_cmd = "gatt_client_facade.GattcListServices"
        test_args = {"identifier": id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def bleGetDiscoveredDevices(self):
        """Stops a BLE scan

        Returns:
            Dictionary, List of devices discovered, error string if error.
        """
        test_cmd = "gatt_client_facade.BleGetDiscoveredDevices"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def discoverCharacteristics(self):
        """Discover the characteristics of a connected service.

        Returns:
            Dictionary, List of Characteristics and Descriptors if success,
            error string if error.
        """
        test_cmd = "gatt_client_facade.GattcDiscoverCharacteristics"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def writeCharById(self, id, offset, write_value):
        """Write Characteristic by id..

        Args:
            id: string, Characteristic identifier.
            offset: int, The offset of bytes to write to.
            write_value: byte array, The bytes to write.

        Returns:
            None if success, error string if error.
        """
        test_cmd = "gatt_client_facade.GattcWriteCharacteristicById"
        test_args = {
            "identifier": id,
            "offset": offset,
            "write_value": write_value,
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def writeCharByIdWithoutResponse(self, id, write_value):
        """Write Characteristic by id without response.

        Args:
            id: string, Characteristic identifier.
            write_value: byte array, The bytes to write.

        Returns:
            None if success, error string if error.
        """
        test_cmd = "gatt_client_facade.GattcWriteCharacteristicByIdWithoutResponse"
        test_args = {
            "identifier": id,
            "write_value": write_value,
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def enableNotifyCharacteristic(self, id):
        """Enable notifications on a Characteristic.

        Args:
            id: string, Characteristic identifier.

        Returns:
            None if success, error string if error.
        """
        test_cmd = "gatt_client_facade.GattcEnableNotifyCharacteristic"
        test_args = {
            "identifier": id,
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def disableNotifyCharacteristic(self, id):
        """Disable notifications on a Characteristic.

        Args:
            id: string, Characteristic identifier.

        Returns:
            None if success, error string if error.
        """
        test_cmd = "gatt_client_facade.GattcDisableNotifyCharacteristic"
        test_args = {
            "identifier": id,
            "value": False,
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def readCharacteristicById(self, id):
        """Read Characteristic value by id..

        Args:
            id: string, Characteristic identifier.

        Returns:
            Characteristic value if success, error string if error.
        """
        test_cmd = "gatt_client_facade.GattcReadCharacteristicById"
        test_args = {
            "identifier": id,
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def readDescriptorById(self, id):
        """Read Descriptor value by id..

        Args:
            id: string, Descriptor identifier.

        Returns:
            Descriptor value if success, error string if error.
        """
        test_cmd = "gatt_client_facade.GattcReadDescriptorById"
        test_args = {
            "identifier": id,
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def readLongDescriptorById(self, id, offset, max_bytes):
        """Reads Long Descriptor value by id.

        Args:
            id: string, Descriptor identifier.
            offset: int, The offset to start reading from.
            max_bytes: int, The max bytes to return.

        Returns:
            Descriptor value if success, error string if error.
        """
        test_cmd = "gatt_client_facade.GattcReadLongDescriptorById"
        test_args = {
            "identifier": id,
            "offset": offset,
            "max_bytes": max_bytes
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def writeDescriptorById(self, id, offset, write_value):
        """Write Descriptor by id.

        Args:
            id: string, Descriptor identifier.
            write_value: byte array, The bytes to write.

        Returns:
            None if success, error string if error.
        """
        test_cmd = "gatt_client_facade.GattcWriteDescriptorById"
        test_args = {
            "identifier": id,
            "write_value": write_value,
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def readLongCharacteristicById(self, id, offset, max_bytes):
        """Reads Long Characteristic value by id.

        Args:
            id: string, Characteristic identifier.
            offset: int, The offset to start reading from.
            max_bytes: int, The max bytes to return.

        Returns:
            Characteristic value if success, error string if error.
        """
        test_cmd = "gatt_client_facade.GattcReadLongCharacteristicById"
        test_args = {
            "identifier": id,
            "offset": offset,
            "max_bytes": max_bytes
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def connectToService(self, id, service_id):
        """ Connect to a specific Service specified by id.

        Args:
            id: string, Service id.

        Returns:
            None if success, error string if error.
        """
        test_cmd = "gatt_client_facade.GattcConnectToService"
        test_args = {"identifier": id, "service_identifier": service_id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def bleConnectToPeripheral(self, id):
        """Connects to a peripheral specified by id.

        Args:
            id: string, Peripheral identifier to connect to.

        Returns:
            Dictionary, List of Service Info if success, error string if error.
        """
        test_cmd = "gatt_client_facade.BleConnectPeripheral"
        test_args = {"identifier": id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def bleDisconnectPeripheral(self, id):
        """Disconnects from a peripheral specified by id.

        Args:
            id: string, Peripheral identifier to disconnect from.

        Returns:
            Dictionary, None if success, error string if error.
        """
        test_cmd = "gatt_client_facade.BleDisconnectPeripheral"
        test_args = {"identifier": id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)