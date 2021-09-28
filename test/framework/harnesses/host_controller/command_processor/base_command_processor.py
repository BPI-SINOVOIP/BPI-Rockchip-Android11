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

import argparse
import logging
import re

from host_controller import console_argument_parser

# tmp_dir variable name.
TMP_DIR_VAR ="{tmp_dir}"


class BaseCommandProcessor(object):
    '''Base class for command processors.

    Attributes:
        arg_parser: ConsoleArgumentParser object, argument parser.
        arg_buffer: dict, stores last parsed argument, value pairs.
        console: cmd.Cmd console object.
        command: string, command name which this processor will handle.
        command_detail: string, detailed explanation for the command.
    '''

    command = 'base'
    command_detail = 'Command processor template'

    def _SetUp(self, console):
        '''Internal SetUp function that will call subclass' Setup function.

        Args:
            console: Console object.
        '''
        self.console = console
        self.arg_buffer = {}
        self.arg_parser = console_argument_parser.ConsoleArgumentParser(
            self.command, self.command_detail)
        self.SetUp()

    def SetUp(self):
        '''SetUp method for subclass to override.'''
        pass

    def _Run(self, arg_line):
        '''Internal function that will call subclass' Run function.

        Args:
            arg_line: string, line of command arguments
        '''
        ret = self.Run(arg_line)
        args = self.arg_parser.ParseLine(arg_line)
        for arg_tuple in args._get_kwargs():
            key = arg_tuple[0]
            value = arg_tuple[1]
            self.arg_buffer[key] = value

        if ret is not None:
            if ret == True:  # exit command executed.
                return True
            elif ret == False:
                return False
            else:
                logging.warning("{} coommand returned {}".format(
                    self.command, ret))

    def Run(self, arg_line):
        '''Run method to perform action when invoked from console.

        Args:
            arg_line: string, line of command
        '''
        pass

    def _Help(self):
        '''Internal function that will call subclass' Help function.'''
        self.Help()

    def Help(self):
        '''Help method to print help informations.'''
        if hasattr(self, 'arg_parser') and hasattr(self.console, '_out_file'):
            self.arg_parser.print_help(self.console._out_file)

    def _TearDown(self):
        '''Internal function that will call subclass' TearDown function.'''
        self.TearDown()

    def TearDown(self):
        '''TearDown tasks to be called when console is shutting down.'''
        pass

    def ReplaceVars(self, message_list):
        """Replaces vars in a 'messsag_list' to their values."""
        new_message_list = []
        for message in message_list:
            new_message = message

            vars = re.findall(r"{device-image\[[^]]+\]}", message)
            if vars:
                for var in vars:
                    var_name = var[len("{device-image")+1:-2]
                    if var_name and var_name in self.console.device_image_info:
                        new_message = new_message.replace(
                                var, self.console.device_image_info[var_name])
                    else:
                        new_message = new_message.replace(var, "{undefined}")

            vars = re.findall(r"{tools\[[^]]+\]}", message)
            if vars:
                for var in vars:
                    var_name = var[len("{tools")+1:-2]
                    if var_name and var_name in self.console.tools_info:
                        new_message = new_message.replace(
                            var, self.console.tools_info[var_name])
                    else:
                        new_message = new_message.replace(var, "{undefined}")

            if TMP_DIR_VAR in new_message:
                new_message = new_message.replace(
                    TMP_DIR_VAR, self.console.tmpdir_default)

            new_message_list.append(new_message)
        return new_message_list
