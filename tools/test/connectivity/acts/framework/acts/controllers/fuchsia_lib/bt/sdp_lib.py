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

from acts.controllers.fuchsia_lib.base_lib import BaseLib


class FuchsiaProfileServerLib(BaseLib):
    def __init__(self, addr, tc, client_id):
        self.address = addr
        self.test_counter = tc
        self.client_id = client_id

    def addService(self, record):
        """Publishes an SDP service record specified by input args

        Args:
            record: A database that represents an SDP record to
                be published.

        Returns:
            Dictionary, service id if success, error if error.
        """
        test_cmd = "profile_server_facade.ProfileServerAddService"
        test_args = {
            "record": record,
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def addSearch(self, attribute_list, profile_id):
        """Publishes services specified by input args

        Args:
            attribute_list: The list of attributes to set
            profile_id: The profile ID to set.
        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "profile_server_facade.ProfileServerAddSearch"
        test_args = {
            "attribute_list": attribute_list,
            "profile_id": profile_id
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def removeService(self, service_id):
        """Removes a service.

        Args:
            record: A database that represents an SDP record to
                be published.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "profile_server_facade.ProfileServerRemoveService"
        test_args = {
            "service_id": service_id,
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def init(self):
        """Initializes the ProfileServerFacade's proxy object.

        No operations for SDP can be performed until this is initialized.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "profile_server_facade.ProfileServerInit"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def cleanUp(self):
        """Cleans up all objects related to SDP.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "profile_server_facade.ProfileServerCleanup"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def connectL2cap(self, identifier, psm):
        """ Sends an outgoing l2cap connection to a connected peer device.

        Args:
            psm: The psm value to connect over. Available PSMs:
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

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "profile_server_facade.ProfileServerConnectL2cap"
        test_args = {"identifier": identifier, "psm": psm}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)
