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

import datetime

from acts.controllers.fuchsia_lib.base_lib import BaseLib


class FuchsiaHwinfoLib(BaseLib):
    def __init__(self, addr, tc, client_id):
        self.address = addr
        self.test_counter = tc
        self.client_id = client_id

    def getDeviceInfo(self):
        """Get's the hw info of the Fuchsia device under test.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "hwinfo_facade.HwinfoGetDeviceInfo"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def getProductInfo(self):
        """Get's the hw info of the Fuchsia device under test.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "hwinfo_facade.HwinfoGetProductInfo"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)

    def getBoardInfo(self):
        """Get's the hw info of the Fuchsia device under test.

        Returns:
            Dictionary, None if success, error if error.
        """
        test_cmd = "hwinfo_facade.HwinfoGetBoardInfo"
        test_args = {}
        test_id = self.build_id(self.test_counter)
        self.test_counter += 1

        return self.send_command(test_id, test_cmd, test_args)
