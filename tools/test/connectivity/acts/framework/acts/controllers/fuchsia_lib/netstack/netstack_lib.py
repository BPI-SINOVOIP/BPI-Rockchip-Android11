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

class FuchsiaNetstackLib(BaseLib):
    def __init__(self, addr, tc, client_id):
        self.address = addr
        self.test_counter = tc
        self.client_id = client_id

    def netstackListInterfaces(self):
        """ListInterfaces command

        Returns:
            List of interface paths
        """
        test_cmd = "netstack_facade.ListInterfaces"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def init(self):
        """ListInterfaces command

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "netstack_facade.InitNetstack"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def getInterfaceInfo(self, id):
        """Get interface info.

        Args:
            id: The interface ID.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "netstack_facade.GetInterfaceInfo"
        test_args = {
            "identifier": id
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def enableInterface(self, id):
        """Enable Interface

        Args:
            id: The interface ID.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "netstack_facade.EnableInterface"
        test_args = {
            "identifier": id
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def disableInterface(self, id):
        """Disable Interface

        Args:
            id: The interface ID.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "netstack_facade.DisableInterface"
        test_args = {
            "identifier": id
        }
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

