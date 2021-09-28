#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import getpass
import logging

from host_controller.command_processor import base_command_processor


class CommandPassword(base_command_processor.BaseCommandProcessor):
    """Command processor for password command.

    Attributes:
        console: cmd.Cmd console object.
        command: string, command name which this processor will handle.
        command_detail: string, detailed explanation for the command.
    """

    command = "password"
    command_detail = "Sets password."

    # @Override
    def Run(self, arg_line):
        """Gets a new password."""
        new_password = getpass.getpass()
        if new_password:
            self.console.password.value = new_password
            logging.info("new password set.")
        else:
            logging.warn("password is not updated.")

    def Help(self):
        base_command_processor.BaseCommandProcessor.Help(self)
