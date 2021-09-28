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
r"""List args.

Defines the list arg parser that holds list specific args.
"""

CMD_LIST = "list"


def GetListArgParser(subparser):
    """Return the list arg parser.

    Args:
       subparser: argparse.ArgumentParser that is attached to main acloud cmd.

    Returns:
        argparse.ArgumentParser with list options defined.
    """
    list_parser = subparser.add_parser(CMD_LIST)
    list_parser.required = False
    list_parser.set_defaults(which=CMD_LIST)
    list_parser.add_argument(
        "--local-only",
        action="store_true",
        dest="local_only",
        required=False,
        help="Do not authenticate and query remote instances.")

    return list_parser
