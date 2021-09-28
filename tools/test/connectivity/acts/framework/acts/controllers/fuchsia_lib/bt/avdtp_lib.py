#!/usr/bin/env python3
#
#   Copyright 2020 - The Android Open Source Project
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


class FuchsiaAvdtpLib(BaseLib):
    def __init__(self, addr, tc, client_id):
        self.address = addr
        self.test_counter = tc
        self.client_id = client_id

    def init(self, role):
        """Initializes the ProfileServerFacade's proxy object.

        No operations for SDP can be performed until this is initialized.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "avdtp_facade.AvdtpInit"

        test_args = {"role": role}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def getConnectedPeers(self):
        """Gets the AVDTP connected peers.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "avdtp_facade.AvdtpGetConnectedPeers"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def setConfiguration(self, peer_id):
        """Sends the AVDTP command to input peer_id: set configuration

        Args:
            peer_id: The peer id to send the AVDTP command to.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "avdtp_facade.AvdtpSetConfiguration"
        test_args = {"identifier": peer_id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def getConfiguration(self, peer_id):
        """Sends the AVDTP command to input peer_id: get configuration

        Args:
            peer_id: The peer id to send the AVDTP command to.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "avdtp_facade.AvdtpGetConfiguration"
        test_args = {"identifier": peer_id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def getCapabilities(self, peer_id):
        """Sends the AVDTP command to input peer_id: get capabilities

        Args:
            peer_id: The peer id to send the AVDTP command to.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "avdtp_facade.AvdtpGetCapabilities"
        test_args = {"identifier": peer_id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def getAllCapabilities(self, peer_id):
        """Sends the AVDTP command to input peer_id: get all capabilities

        Args:
            peer_id: The peer id to send the AVDTP command to.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "avdtp_facade.AvdtpGetAllCapabilities"
        test_args = {"identifier": peer_id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def reconfigureStream(self, peer_id):
        """Sends the AVDTP command to input peer_id: reconfigure stream

        Args:
            peer_id: The peer id to send the AVDTP command to.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "avdtp_facade.AvdtpReconfigureStream"
        test_args = {"identifier": peer_id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def suspendStream(self, peer_id):
        """Sends the AVDTP command to input peer_id: suspend stream
        Args:
            peer_id: The peer id to send the AVDTP command to.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "avdtp_facade.AvdtpSuspendStream"
        test_args = {"identifier": peer_id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def suspendAndReconfigure(self, peer_id):
        """Sends the AVDTP command to input peer_id: suspend and reconfigure

        Args:
            peer_id: The peer id to send the AVDTP command to.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "avdtp_facade.AvdtpSuspendAndReconfigure"
        test_args = {"identifier": peer_id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def releaseStream(self, peer_id):
        """Sends the AVDTP command to input peer_id: release stream

        Args:
            peer_id: The peer id to send the AVDTP command to.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "avdtp_facade.AvdtpReleaseStream"
        test_args = {"identifier": peer_id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def establishStream(self, peer_id):
        """Sends the AVDTP command to input peer_id: establish stream

        Args:
            peer_id: The peer id to send the AVDTP command to.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "avdtp_facade.AvdtpEstablishStream"
        test_args = {"identifier": peer_id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def startStream(self, peer_id):
        """Sends the AVDTP command to input peer_id: start stream

        Args:
            peer_id: The peer id to send the AVDTP command to.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "avdtp_facade.AvdtpStartStream"
        test_args = {"identifier": peer_id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def abortStream(self, peer_id):
        """Sends the AVDTP command to input peer_id: abort stream

        Args:
            peer_id: The peer id to send the AVDTP command to.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "avdtp_facade.AvdtpAbortStream"
        test_args = {"identifier": peer_id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def establishStream(self, peer_id):
        """Sends the AVDTP command to input peer_id: establish stream

        Args:
            peer_id: The peer id to send the AVDTP command to.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "avdtp_facade.AvdtpEstablishStream"
        test_args = {"identifier": peer_id}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def removeService(self):
        """Removes the AVDTP service from the Fuchsia device

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "avdtp_facade.AvdtpRemoveService"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)
