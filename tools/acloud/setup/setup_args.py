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
r"""Setup args.

Defines the setup arg parser that holds setup specific args.
"""

CMD_SETUP = "setup"


def GetSetupArgParser(subparser):
    """Return the setup arg parser.

    Args:
       subparser: argparse.ArgumentParser that is attached to main acloud cmd.

    Returns:
        argparse.ArgumentParser with setup options defined.
    """
    setup_parser = subparser.add_parser(CMD_SETUP)
    setup_parser.required = False
    setup_parser.set_defaults(which=CMD_SETUP)
    setup_parser.add_argument(
        "--host",
        action="store_true",
        dest="host",
        required=False,
        help="Setup host to run local instance of an Android Virtual Device. "
        "Must explicitly set to kick off host setup. Automatically installs "
        "host base packages as well")
    setup_parser.add_argument(
        "--host-base",
        action="store_true",
        dest="host_base",
        required=False,
        help="Install base packages on the host.")
    setup_parser.add_argument(
        "--gcp-init",
        action="store_true",
        dest="gcp_init",
        required=False,
        help="Setup Google Cloud project name and enable required GCP APIs."
        "E.G. Google Cloud Storage/ Internal Android Build/ Compute Engine")
    setup_parser.add_argument(
        "--force",
        action="store_true",
        dest="force",
        required=False,
        help="Force the setup steps even if it's not required.")

    return setup_parser
