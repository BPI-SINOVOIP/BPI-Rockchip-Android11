#!/usr/bin/env python3
#
# Copyright (C) 2018 The Android Open Source Project
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
"""This file is used to send relay devices commands.

Usage:
    See --help for more details.

    relay_tool.py -c <acts_config> -tb <testbed_name> -rd <device_name> -cmd cmd
        Runs the command cmd for the given device pulled from the ACTS config.

    relay_tool.py -c <acts_config> -tb <testbed_name> -rd <device_name> -l
        Lists all possible functions (as well as documentation) for the device.

Examples:

    # For a Bluetooth Relay Device:
    $ relay_tool.py -c ... -tb ... -rd ... -cmd get_mac_address
    <MAC_ADDRESS>
    relay_tool.py -c ... -tb ... -rd ... -cmd enter_pairing_mode
    # No output. Waits for enter_pairing_mode to complete.
"""

import argparse
import json
import re
import sys
import inspect

from acts.controllers import relay_device_controller


def get_relay_device(config_path, testbed_name, device_name):
    """Returns the relay device specified by the arguments.

    Args:
        config_path: the path to the ACTS config.
        testbed_name: the name of the testbed the device is a part of.
        device_name: the name of the device within the testbed.

    Returns:
        The RelayDevice object.
    """
    with open(config_path) as config_file:
        config = json.load(config_file)

    relay_config = config['testbed'][testbed_name]['RelayDevice']
    relay_devices = relay_device_controller.create(relay_config)

    try:
        return next(device for device in relay_devices
                    if device.name == device_name)
    except StopIteration:
        # StopIteration is raised when no device is found.
        all_device_names = [device.name for device in relay_devices]
        print('Unable to find device with name "%s" in testbed "%s". Expected '
              'any of %s.' % (device_name, testbed_name, all_device_names),
              file=sys.stderr)
        raise


def print_docstring(relay_device, func_name):
    """Prints the docstring of the specified function to stderr.

    Note that the documentation will be printed as follows:

        func_name:
            Docstring information, indented with a minimum of 4 spaces.

    Args:
        relay_device: the RelayDevice to find a function on.
        func_name: the function to pull the docstring from.
    """
    func = getattr(relay_device, func_name)
    signature = inspect.signature(func)
    docstring = func.__doc__
    if docstring is None:
        docstring = '    No docstring available.'
    else:
        # Make the indentation uniform.

        min_line_indentation = sys.maxsize
        # Skip the first line, because docstrings begin with 'One liner.\n',
        # instead of an indentation.
        for line in docstring.split('\n')[1:]:
            index = 0
            for index, char in enumerate(line):
                if char != ' ':
                    break
            if index + 1 < min_line_indentation and index != 0:
                min_line_indentation = index + 1

        if min_line_indentation == sys.maxsize:
            min_line_indentation = 0

        min_indent = '\n' + ' ' * (min_line_indentation - 4)
        docstring = ' ' * 4 + docstring.rstrip()
        docstring = re.sub(min_indent, '\n', docstring,
                           flags=re.MULTILINE)

    print('%s%s: \n%s\n' % (func_name, str(signature), docstring),
          file=sys.stderr)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--config', type=str, required=True,
                        help='The path to the config file.')
    parser.add_argument('-tb', '--testbed', type=str, required=True,
                        help='The testbed within the config file to use.')
    parser.add_argument('-rd', '--relay_device', type=str, required=True,
                        help='The name of the relay device to use.')
    group = parser.add_mutually_exclusive_group()
    group.add_argument('-cmd', '--command', type=str, nargs='+',
                       help='The command to run on the relay device.')
    group.add_argument('-l', '--list_commmands',
                       action='store_true',
                       help='lists all commands for the given device.')

    args = parser.parse_args()
    relay_device = get_relay_device(args.config, args.testbed,
                                    args.relay_device)

    func_names = [func_name
                  for func_name in dir(relay_device)
                  if (not func_name.startswith('_') and
                      not func_name.endswith('__') and
                      callable(getattr(relay_device, func_name)))]

    if args.command:
        if args.command[0] not in func_names:
            print('Received command %s. Expected any of %s.' %
                  (repr(args.command[0]), repr(func_names)), file=sys.stderr)
        else:
            # getattr should not be able to fail here.
            func = getattr(relay_device, args.command[0])
            try:
                ret = func(*args.command[1:])
                if ret is not None:
                    print(ret)
            except TypeError as e:
                # The above call may raise TypeError if an incorrect number
                # of args are passed.
                if len(e.args) == 1 and func.__name__ in e.args[0]:
                    # If calling the function raised the TypeError, log this
                    # more informative message instead of the traceback.
                    print('Incorrect number of args passed to command "%s".' %
                          args.command[0], file=sys.stderr)
                    print_docstring(relay_device, args.command[0])
                else:
                    raise

    else:  # args.list_commands is set
        print('Note: These commands are specific to the device given. '
              'Some of these commands may not work on other devices.\n',
              file=sys.stderr)
        for func_name in func_names:
            print_docstring(relay_device, func_name)
        exit(1)


if __name__ == '__main__':
    main()
