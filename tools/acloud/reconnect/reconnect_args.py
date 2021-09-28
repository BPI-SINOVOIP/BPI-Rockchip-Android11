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
r"""Reconnect args.

Defines the reconnect arg parser that holds delete specific args.
"""

CMD_RECONNECT = "reconnect"


def GetReconnectArgParser(subparser):
    """Return the reconnect arg parser.

    Args:
       subparser: argparse.ArgumentParser that is attached to main acloud cmd.

    Returns:
        argparse.ArgumentParser with reconnect options defined.
    """
    reconnect_parser = subparser.add_parser(CMD_RECONNECT)
    reconnect_parser.required = False
    reconnect_parser.set_defaults(which=CMD_RECONNECT)
    reconnect_parser.add_argument(
        "--instance-names",
        dest="instance_names",
        nargs="+",
        required=False,
        help="The names of the instances that need to reconnect remote/local "
        "instances. For remote instances, it is the GCE instance name. "
        "For local instances, it's the local keyword, e.g. --instance-names "
        "local-instance")
    reconnect_parser.add_argument(
        "--all",
        action="store_true",
        dest="all",
        required=False,
        help="If more than 1 AVD instance is found, reconnect them all.")
    reconnect_parser.add_argument(
        "--autoconnect",
        type=str,
        nargs="?",
        const=True,
        dest="autoconnect",
        required=False,
        choices=[True, "adb"],
        default=True,
        help="If need adb only, you can pass in 'adb' here.")

    return reconnect_parser
