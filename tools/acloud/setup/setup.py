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
r"""Setup entry point.

Setup will handle all of the necessary steps to enable acloud to create a local
or remote instance of an Android Virtual Device.
"""

from __future__ import print_function
import os
import subprocess
import sys

from acloud.internal import constants
from acloud.internal.lib import utils
from acloud.setup import host_setup_runner
from acloud.setup import gcp_setup_runner


def Run(args):
    """Run setup.

    Setup options:
        -host: Setup host settings.
        -gcp_init: Setup gcp settings.
        -None, default behavior will setup host and gcp settings.

    Args:
        args: Namespace object from argparse.parse_args.
    """
    _RunPreSetup()

    # Setup process will be in the following manner:
    # 1.Print welcome message.
    _PrintWelcomeMessage()

    # 2.Init all subtasks in queue and traverse them.
    host_base_runner = host_setup_runner.HostBasePkgInstaller()
    host_avd_runner = host_setup_runner.AvdPkgInstaller()
    host_cf_common_runner = host_setup_runner.CuttlefishCommonPkgInstaller()
    host_env_runner = host_setup_runner.CuttlefishHostSetup()
    gcp_runner = gcp_setup_runner.GcpTaskRunner(args.config_file)
    task_queue = []
    # User must explicitly specify --host to install the avd host packages.
    if args.host:
        task_queue.append(host_base_runner)
        task_queue.append(host_avd_runner)
        task_queue.append(host_cf_common_runner)
        task_queue.append(host_env_runner)
    # We should do these setup tasks if specified or if no args were used.
    if args.host_base or (not args.host and not args.gcp_init):
        task_queue.append(host_base_runner)
    if args.gcp_init or (not args.host and not args.host_base):
        task_queue.append(gcp_runner)

    for subtask in task_queue:
        subtask.Run(force_setup=args.force)

    # 3.Print the usage hints.
    _PrintUsage()


def _PrintWelcomeMessage():
    """Print welcome message when acloud setup been called."""

    # pylint: disable=anomalous-backslash-in-string
    asc_art = "                                    \n" \
            "   ___  _______   ____  __  _____ \n" \
            "  / _ |/ ___/ /  / __ \/ / / / _ \\ \n" \
            " / __ / /__/ /__/ /_/ / /_/ / // /  \n" \
            "/_/ |_\\___/____/\\____/\\____/____/ \n" \
            "                                  \n"

    print("\nWelcome to")
    print(asc_art)


def _PrintUsage():
    """Print cmd usage hints when acloud setup been finished."""
    utils.PrintColorString("")
    utils.PrintColorString("Setup process finished")


def _RunPreSetup():
    """This will run any pre-setup scripts.

    If we can find any pre-setup scripts, run it and don't care about the
    results. Pre-setup scripts will do any special setup before actual
    setup occurs (e.g. copying configs).
    """
    if constants.ENV_ANDROID_BUILD_TOP not in os.environ:
        print("Can't find $%s." % constants.ENV_ANDROID_BUILD_TOP)
        print("Please run '#source build/envsetup.sh && lunch <target>' first.")
        sys.exit(constants.EXIT_BY_USER)

    pre_setup_sh = os.path.join(os.environ.get(constants.ENV_ANDROID_BUILD_TOP),
                                "tools",
                                "acloud",
                                "setup",
                                "pre_setup_sh",
                                "acloud_pre_setup.sh")

    if os.path.exists(pre_setup_sh):
        subprocess.call([pre_setup_sh])
