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
from acts import logger
from acts.controllers.fuchsia_lib.base_lib import BaseLib

COMMAND_SCAN = "wlan.scan"
COMMAND_CONNECT = "wlan.connect"
COMMAND_DISCONNECT = "wlan.disconnect"
COMMAND_STATUS = "wlan.status"
COMMAND_GET_IFACE_ID_LIST = "wlan.get_iface_id_list"
COMMAND_DESTROY_IFACE = "wlan.destroy_iface"


class FuchsiaWlanLib(BaseLib):
    def __init__(self, addr, tc, client_id):
        self.address = addr
        self.test_counter = tc
        self.client_id = client_id
        self.log = logger.create_tagged_trace_logger(str(addr))

    def wlanStartScan(self):
        """ Starts a wlan scan

        Returns:
            scan results
        """
        test_cmd = COMMAND_SCAN
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, {})

    def wlanConnectToNetwork(self, target_ssid, target_pwd=None):
        """ Triggers a network connection
        Args:
            target_ssid: the network to attempt a connection to
            target_pwd: (optional) password for the target network

        Returns:
            boolean indicating if the connection was successful
        """
        test_cmd = COMMAND_CONNECT
        test_args = {"target_ssid": target_ssid, "target_pwd": target_pwd}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def wlanDisconnect(self):
        """ Disconnect any current wifi connections"""
        test_cmd = COMMAND_DISCONNECT
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, {})

    def wlanDestroyIface(self, iface_id):
        """ Destroy WLAN interface by ID.
        Args:
            iface_id: the interface id.

        Returns:
            Dictionary, service id if success, error if error.
        """
        test_cmd = COMMAND_DESTROY_IFACE
        test_args = {"identifier": iface_id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def wlanGetIfaceIdList(self):
        """ Get a list if wlan interface IDs.

        Returns:
            Dictionary, service id if success, error if error.
        """
        test_cmd = COMMAND_GET_IFACE_ID_LIST
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, {})

    def wlanStatus(self):
        """ Request connection status

        Returns:
            Client state summary containing WlanClientState and
            status of various networks connections
        """
        test_cmd = COMMAND_STATUS
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, {})
