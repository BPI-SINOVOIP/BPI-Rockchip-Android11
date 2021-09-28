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
r"""Create entry point.

Create will handle all the logic related to creating a local/remote instance
an Android Virtual Device and the logic related to prepping the local/remote
image artifacts.
"""

from __future__ import print_function

import os
import subprocess
import sys

from acloud import errors
from acloud.create import avd_spec
from acloud.create import cheeps_remote_image_remote_instance
from acloud.create import gce_local_image_remote_instance
from acloud.create import gce_remote_image_remote_instance
from acloud.create import goldfish_local_image_local_instance
from acloud.create import goldfish_remote_image_remote_instance
from acloud.create import local_image_local_instance
from acloud.create import local_image_remote_instance
from acloud.create import local_image_remote_host
from acloud.create import remote_image_remote_instance
from acloud.create import remote_image_local_instance
from acloud.create import remote_image_remote_host
from acloud.internal import constants
from acloud.internal.lib import utils
from acloud.setup import setup
from acloud.setup import gcp_setup_runner
from acloud.setup import host_setup_runner


_MAKE_CMD = "build/soong/soong_ui.bash"
_MAKE_ARG = "--make-mode"

_CREATOR_CLASS_DICT = {
    # GCE types
    (constants.TYPE_GCE, constants.IMAGE_SRC_LOCAL, constants.INSTANCE_TYPE_REMOTE):
        gce_local_image_remote_instance.GceLocalImageRemoteInstance,
    (constants.TYPE_GCE, constants.IMAGE_SRC_REMOTE, constants.INSTANCE_TYPE_REMOTE):
        gce_remote_image_remote_instance.GceRemoteImageRemoteInstance,
    # CF types
    (constants.TYPE_CF, constants.IMAGE_SRC_LOCAL, constants.INSTANCE_TYPE_LOCAL):
        local_image_local_instance.LocalImageLocalInstance,
    (constants.TYPE_CF, constants.IMAGE_SRC_LOCAL, constants.INSTANCE_TYPE_REMOTE):
        local_image_remote_instance.LocalImageRemoteInstance,
    (constants.TYPE_CF, constants.IMAGE_SRC_LOCAL, constants.INSTANCE_TYPE_HOST):
        local_image_remote_host.LocalImageRemoteHost,
    (constants.TYPE_CF, constants.IMAGE_SRC_REMOTE, constants.INSTANCE_TYPE_REMOTE):
        remote_image_remote_instance.RemoteImageRemoteInstance,
    (constants.TYPE_CF, constants.IMAGE_SRC_REMOTE, constants.INSTANCE_TYPE_LOCAL):
        remote_image_local_instance.RemoteImageLocalInstance,
    (constants.TYPE_CF, constants.IMAGE_SRC_REMOTE, constants.INSTANCE_TYPE_HOST):
        remote_image_remote_host.RemoteImageRemoteHost,
    # Cheeps types
    (constants.TYPE_CHEEPS, constants.IMAGE_SRC_REMOTE, constants.INSTANCE_TYPE_REMOTE):
        cheeps_remote_image_remote_instance.CheepsRemoteImageRemoteInstance,
    # GF types
    (constants.TYPE_GF, constants.IMAGE_SRC_REMOTE, constants.INSTANCE_TYPE_REMOTE):
        goldfish_remote_image_remote_instance.GoldfishRemoteImageRemoteInstance,
    (constants.TYPE_GF, constants.IMAGE_SRC_LOCAL, constants.INSTANCE_TYPE_LOCAL):
        goldfish_local_image_local_instance.GoldfishLocalImageLocalInstance,
}


def GetAvdCreatorClass(avd_type, instance_type, image_source):
    """Return the creator class for the specified spec.

    Based on the image source and the instance type, return the proper
    creator class.

    Args:
        avd_type: String, the AVD type(cuttlefish, gce).
        instance_type: String, the AVD instance type (local or remote).
        image_source: String, the source of the image (local or remote).

    Returns:
        An AVD creator class (e.g. LocalImageRemoteInstance).

    Raises:
        UnsupportedInstanceImageType if argments didn't match _CREATOR_CLASS_DICT.
    """
    creator_class = _CREATOR_CLASS_DICT.get(
        (avd_type, image_source, instance_type))

    if not creator_class:
        raise errors.UnsupportedInstanceImageType(
            "unsupported creation of avd type: %s, instance type: %s, "
            "image source: %s" % (avd_type, instance_type, image_source))
    return creator_class

def _CheckForAutoconnect(args):
    """Check that we have all prerequisites for autoconnect.

    Autoconnect requires adb and ssh, we'll just check for adb for now and
    assume ssh is everywhere. If adb isn't around, ask the user if they want us
    to build it, if not we'll disable autoconnect.

    Args:
        args: Namespace object from argparse.parse_args.
    """
    if not args.autoconnect or utils.FindExecutable(constants.ADB_BIN):
        return

    disable_autoconnect = False
    answer = utils.InteractWithQuestion(
        "adb is required for autoconnect, without it autoconnect will be "
        "disabled, would you like acloud to build it[y/N]? ")
    if answer in constants.USER_ANSWER_YES:
        utils.PrintColorString("Building adb ... ", end="")
        android_build_top = os.environ.get(
            constants.ENV_ANDROID_BUILD_TOP)
        if not android_build_top:
            utils.PrintColorString("Fail! (Not in a lunch'd env)",
                                   utils.TextColors.FAIL)
            disable_autoconnect = True
        else:
            make_cmd = os.path.join(android_build_top, _MAKE_CMD)
            build_adb_cmd = [make_cmd, _MAKE_ARG, "adb"]
            try:
                with open(os.devnull, "w") as dev_null:
                    subprocess.check_call(build_adb_cmd, stderr=dev_null,
                                          stdout=dev_null)
                    utils.PrintColorString("OK!", utils.TextColors.OKGREEN)
            except subprocess.CalledProcessError:
                utils.PrintColorString("Fail! (build failed)",
                                       utils.TextColors.FAIL)
                disable_autoconnect = True
    else:
        disable_autoconnect = True

    if disable_autoconnect:
        utils.PrintColorString("Disabling autoconnect",
                               utils.TextColors.WARNING)
        args.autoconnect = False


def _CheckForSetup(args):
    """Check that host is setup to run the create commands.

    We'll check we have the necessary bits setup to do what the user wants, and
    if not, tell them what they need to do before running create again.

    Args:
        args: Namespace object from argparse.parse_args.
    """
    # Need to set all these so if we need to run setup, it won't barf on us
    # because of some missing fields.
    args.gcp_init = False
    args.host = False
    args.host_base = False
    args.force = False
    # Remote image/instance requires the GCP config setup.
    if not args.local_instance or args.local_image == "":
        gcp_setup = gcp_setup_runner.GcpTaskRunner(args.config_file)
        if gcp_setup.ShouldRun():
            args.gcp_init = True

    # Local instance requires host to be setup. We'll assume that if the
    # packages were installed, then the user was added into the groups. This
    # avoids the scenario where a user runs setup and creates a local instance.
    # The following local instance create will trigger this if statment and go
    # through the whole setup again even though it's already done because the
    # user groups aren't set until the user logs out and back in.
    if args.local_instance:
        host_pkg_setup = host_setup_runner.AvdPkgInstaller()
        if host_pkg_setup.ShouldRun():
            args.host = True

    # Install base packages if we haven't already.
    host_base_setup = host_setup_runner.HostBasePkgInstaller()
    if host_base_setup.ShouldRun():
        args.host_base = True

    run_setup = args.force or args.gcp_init or args.host or args.host_base

    if run_setup:
        answer = utils.InteractWithQuestion("Missing necessary acloud setup, "
                                            "would you like to run setup[y/N]?")
        if answer in constants.USER_ANSWER_YES:
            setup.Run(args)
        else:
            print("Please run '#acloud setup' so we can get your host setup")
            sys.exit(constants.EXIT_BY_USER)


def PreRunCheck(args):
    """Do some pre-run checks to ensure a smooth create experience.

    Args:
        args: Namespace object from argparse.parse_args.
    """
    _CheckForSetup(args)
    _CheckForAutoconnect(args)


def Run(args):
    """Run create.

    Args:
        args: Namespace object from argparse.parse_args.

    Returns:
        A Report instance.
    """
    if not args.skip_pre_run_check:
        PreRunCheck(args)
    spec = avd_spec.AVDSpec(args)
    avd_creator_class = GetAvdCreatorClass(spec.avd_type,
                                           spec.instance_type,
                                           spec.image_source)
    avd_creator = avd_creator_class()
    report = avd_creator.Create(spec, args.no_prompt)
    return report
