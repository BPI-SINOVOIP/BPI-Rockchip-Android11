#!/usr/bin/env python
#
# Copyright 2018 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
r"""Delete args.

Defines the delete arg parser that holds delete specific args.
"""
import argparse

from acloud.internal import constants


CMD_DELETE = "delete"


def GetDeleteArgParser(subparser):
    """Return the delete arg parser.

    Args:
       subparser: argparse.ArgumentParser that is attached to main acloud cmd.

    Returns:
        argparse.ArgumentParser with delete options defined.
    """
    delete_parser = subparser.add_parser(CMD_DELETE)
    delete_parser.required = False
    delete_parser.set_defaults(which=CMD_DELETE)
    delete_group = delete_parser.add_mutually_exclusive_group()
    delete_group.add_argument(
        "--instance-names",
        dest="instance_names",
        nargs="+",
        required=False,
        help="The names of the instances that need to delete, separated by "
        "spaces, e.g. --instance-names instance-1 local-instance-1")
    delete_group.add_argument(
        "--all",
        action="store_true",
        dest="all",
        required=False,
        help="If more than 1 AVD instance is found, delete them all.")
    delete_group.add_argument(
        "--adb-port", "-p",
        type=int,
        dest="adb_port",
        required=False,
        help="Delete instance with specified adb-port.")
    delete_parser.add_argument(
        "--local-only",
        action="store_true",
        dest="local_only",
        required=False,
        help="Do not authenticate and query remote instances.")
    delete_parser.add_argument(
        "--host",
        type=str,
        dest="remote_host",
        default=None,
        help="'cuttlefish only' Provide host name to clean up the remote host. "
        "For example: '--host 1.1.1.1'")
    delete_parser.add_argument(
        "--host-user",
        type=str,
        dest="host_user",
        default=constants.GCE_USER,
        help="'remote host only' Provide host user for logging in to the host. "
        "The default value is vsoc-01. For example: '--host 1.1.1.1 --host-user "
        "vsoc-02'")
    delete_parser.add_argument(
        "--host-ssh-private-key-path",
        type=str,
        dest="host_ssh_private_key_path",
        default=None,
        help="'remote host only' Provide host ssh private key path for logging "
        "in to the host. For exmaple: '--host-ssh-private-key-path "
        "~/.ssh/acloud_rsa'")

    # TODO(b/118439885): Old arg formats to support transition, delete when
    # transistion is done.
    delete_group.add_argument(
        "--instance_names",
        dest="instance_names",
        nargs="+",
        required=False,
        help=argparse.SUPPRESS)

    return delete_parser
