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

import collections
import json
import logging
import math
import os
import random
import re
import requests
import socket
import time

from acts.controllers.fuchsia_lib.base_lib import BaseLib


class FuchsiaBtcLib(BaseLib):
    # Class representing the Bluetooth Controller Library.

    def __init__(self, addr, tc, client_id):
        self.address = addr
        self.test_counter = tc
        self.client_id = client_id

    def acceptPairing(self):
        """Accepts incomming pairing requests.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "bt_control_facade.BluetoothAcceptPairing"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def setDiscoverable(self, discoverable):
        """Sets the device to be discoverable over BR/EDR.

        Args:
            discoverable: A bool object for setting Bluetooth
              device discoverable or not.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "bt_control_facade.BluetoothSetDiscoverable"
        test_args = {"discoverable": discoverable}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def setName(self, name):
        """Sets the local Bluetooth name of the device.

        Args:
            name: A string that represents the name to set.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "bt_control_facade.BluetoothSetName"
        test_args = {"name": name}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def inputPairingPin(self, pin):
        """Inputs the pairing pin to the Fuchsia devices' pairing delegate.

        Args:
            pin: A string that represents the pin to input.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "bt_control_facade.BluetoothInputPairingPin"
        test_args = {"pin": pin}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def getPairingPin(self):
        """Gets the pairing pin from the Fuchsia devices' pairing delegate.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "bt_control_facade.BluetoothGetPairingPin"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def initBluetoothControl(self):
        """Initialises the Bluetooth Control Interface proxy in SL4F.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "bt_control_facade.BluetoothInitControl"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def requestDiscovery(self, discovery):
        """Start or stop Bluetooth Control device discovery.

        Args:
            discovery: A bool object representing starting or stopping
              device discovery.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "bt_control_facade.BluetoothRequestDiscovery"
        test_args = {"discovery": discovery}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def getKnownRemoteDevices(self):
        """Get known remote BR/EDR and LE devices.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "bt_control_facade.BluetoothGetKnownRemoteDevices"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def forgetDevice(self, identifier):
        """Forgets a devices pairing.

        Args:
            identifier: A string representing the device id.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "bt_control_facade.BluetoothForgetDevice"
        test_args = {"identifier": identifier}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def disconnectDevice(self, identifier):
        """Disconnects a devices.

        Args:
            identifier: A string representing the device id.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "bt_control_facade.BluetoothDisconnectDevice"
        test_args = {"identifier": identifier}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def connectDevice(self, identifier):
        """Connects to a devices.

        Args:
            identifier: A string representing the device id.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "bt_control_facade.BluetoothConnectDevice"
        test_args = {"identifier": identifier}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def getActiveAdapterAddress(self):
        """Gets the current Active Adapter's address.

        Returns:
            Dictionary, String address if success, error if error.
        """
        test_cmd = "bt_control_facade.BluetoothGetActiveAdapterAddress"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def pair(self, identifier, pairing_security_level, non_bondable,
             transport):
        """Pairs to a device.

        Args:
            identifier: A string representing the device id.
            pairing_security_level: The security level required for this pairing request
                represented as a u64. (Only for LE pairing)
                Available Values
                1 - ENCRYPTED: Encrypted without MITM protection (unauthenticated)
                2 - AUTHENTICATED: Encrypted with MITM protection (authenticated).
                None: No pairing security level.
            non_bondable: A bool representing whether the pairing mode is bondable or not. None is
                also accepted. False if bondable, True if non-bondable.
            transport: A u64 representing the transport type.
                Available Values
                1 - BREDR: Classic BR/EDR transport
                2 - LE: Bluetooth Low Energy Transport

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "bt_control_facade.BluetoothPairDevice"
        test_args = {
            "identifier": identifier,
            "pairing_security_level": pairing_security_level,
            "non_bondable": non_bondable,
            "transport": transport,
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1
        return self.send_command(test_id, test_cmd, test_args)
