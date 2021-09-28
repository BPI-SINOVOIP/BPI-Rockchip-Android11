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
r"""host setup runner

A setup sub task runner to support setting up the local host for AVD local
instance.
"""

from __future__ import print_function

import getpass
import logging
import os
import shutil
import sys
import tempfile

from acloud.internal import constants
from acloud.internal.lib import utils
from acloud.setup import base_task_runner
from acloud.setup import setup_common


logger = logging.getLogger(__name__)

# Packages "devscripts" and "equivs" are required for "mk-build-deps".
_AVD_REQUIRED_PKGS = [
    "devscripts", "equivs", "libvirt-clients", "libvirt-daemon-system"]
_BASE_REQUIRED_PKGS = ["ssvnc", "lzop"]
_CUTTLEFISH_COMMOM_PKG = "cuttlefish-common"
_CF_COMMOM_FOLDER = "cf-common"
_LIST_OF_MODULES = ["kvm_intel", "kvm"]
_UPDATE_APT_GET_CMD = "sudo apt-get update"
_INSTALL_CUTTLEFISH_COMMOM_CMD = [
    "git clone https://github.com/google/android-cuttlefish.git {git_folder}",
    "cd {git_folder}",
    "yes | sudo mk-build-deps -i -r -B",
    "dpkg-buildpackage -uc -us",
    "sudo apt-get install -y -f ../cuttlefish-common_*_amd64.deb"]


class BasePkgInstaller(base_task_runner.BaseTaskRunner):
    """Subtask base runner class for installing packages."""

    # List of packages for child classes to override.
    PACKAGES = []

    def ShouldRun(self):
        """Check if required packages are all installed.

        Returns:
            Boolean, True if required packages are not installed.
        """
        if not utils.IsSupportedPlatform():
            return False

        # Any required package is not installed or not up-to-date will need to
        # run installation task.
        for pkg_name in self.PACKAGES:
            if not setup_common.PackageInstalled(pkg_name):
                return True

        return False

    def _Run(self):
        """Install specified packages."""
        cmd = "\n".join(
            [setup_common.PKG_INSTALL_CMD % pkg
             for pkg in self.PACKAGES
             if not setup_common.PackageInstalled(pkg)])

        if not utils.GetUserAnswerYes("\nStart to install package(s):\n%s"
                                      "\nPress 'y' to continue or anything "
                                      "else to do it myself and run acloud "
                                      "again[y/N]: " % cmd):
            sys.exit(constants.EXIT_BY_USER)

        setup_common.CheckCmdOutput(_UPDATE_APT_GET_CMD, shell=True)
        for pkg in self.PACKAGES:
            setup_common.InstallPackage(pkg)

        logger.info("All package(s) installed now.")


class AvdPkgInstaller(BasePkgInstaller):
    """Subtask runner class for installing packages for local instances."""

    WELCOME_MESSAGE_TITLE = ("Install required packages for host setup for "
                             "local instances")
    WELCOME_MESSAGE = ("This step will walk you through the required packages "
                       "installation for running Android cuttlefish devices "
                       "on your host.")
    PACKAGES = _AVD_REQUIRED_PKGS


class HostBasePkgInstaller(BasePkgInstaller):
    """Subtask runner class for installing base host packages."""

    WELCOME_MESSAGE_TITLE = "Install base packages on the host"
    WELCOME_MESSAGE = ("This step will walk you through the base packages "
                       "installation for your host.")
    PACKAGES = _BASE_REQUIRED_PKGS


class CuttlefishCommonPkgInstaller(base_task_runner.BaseTaskRunner):
    """Subtask base runner class for installing cuttlefish-common."""

    WELCOME_MESSAGE_TITLE = "Install cuttlefish-common packages on the host"
    WELCOME_MESSAGE = ("This step will walk you through the cuttlefish-common "
                       "packages installation for your host.")

    def ShouldRun(self):
        """Check if cuttlefish-common package is installed.

        Returns:
            Boolean, True if cuttlefish-common is not installed.
        """
        if not utils.IsSupportedPlatform():
            return False

        # Any required package is not installed or not up-to-date will need to
        # run installation task.
        if not setup_common.PackageInstalled(_CUTTLEFISH_COMMOM_PKG):
            return True
        return False

    def _Run(self):
        """Install cuttlefilsh-common packages."""
        cf_common_path = os.path.join(tempfile.mkdtemp(), _CF_COMMOM_FOLDER)
        logger.debug("cuttlefish-common path: %s", cf_common_path)
        cmd = "\n".join(sub_cmd.format(git_folder=cf_common_path)
                        for sub_cmd in _INSTALL_CUTTLEFISH_COMMOM_CMD)

        if not utils.GetUserAnswerYes("\nStart to install cuttlefish-common :\n%s"
                                      "\nPress 'y' to continue or anything "
                                      "else to do it myself and run acloud "
                                      "again[y/N]: " % cmd):
            sys.exit(constants.EXIT_BY_USER)
        try:
            setup_common.CheckCmdOutput(cmd, shell=True)
        finally:
            shutil.rmtree(os.path.dirname(cf_common_path))
        logger.info("Cuttlefish-common package installed now.")


class CuttlefishHostSetup(base_task_runner.BaseTaskRunner):
    """Subtask class that setup host for cuttlefish."""

    WELCOME_MESSAGE_TITLE = "Host Enviornment Setup"
    WELCOME_MESSAGE = (
        "This step will help you to setup enviornment for running Android "
        "cuttlefish devices on your host. That includes adding user to kvm "
        "related groups and checking required linux modules."
    )

    def ShouldRun(self):
        """Check host user groups and modules.

         Returns:
             Boolean: False if user is in all required groups and all modules
                      are reloaded.
         """
        if not utils.IsSupportedPlatform():
            return False

        return not (utils.CheckUserInGroups(constants.LIST_CF_USER_GROUPS)
                    and self._CheckLoadedModules(_LIST_OF_MODULES))

    @staticmethod
    def _CheckLoadedModules(module_list):
        """Check if the modules are all in use.

        Args:
            module_list: The list of module name.
        Returns:
            True if all modules are in use.
        """
        logger.info("Checking if modules are loaded: %s", module_list)
        lsmod_output = setup_common.CheckCmdOutput("lsmod", print_cmd=False)
        current_modules = [r.split()[0] for r in lsmod_output.splitlines()]
        all_modules_present = True
        for module in module_list:
            if module not in current_modules:
                logger.info("missing module: %s", module)
                all_modules_present = False
        return all_modules_present

    def _Run(self):
        """Setup host environment for local cuttlefish instance support."""
        # TODO: provide --uid args to let user use prefered username
        username = getpass.getuser()
        setup_cmds = [
            "sudo rmmod kvm_intel",
            "sudo rmmod kvm",
            "sudo modprobe kvm",
            "sudo modprobe kvm_intel"]
        for group in constants.LIST_CF_USER_GROUPS:
            setup_cmds.append("sudo usermod -aG %s % s" % (group, username))

        print("Below commands will be run:")
        for setup_cmd in setup_cmds:
            print(setup_cmd)

        if self._ConfirmContinue():
            for setup_cmd in setup_cmds:
                setup_common.CheckCmdOutput(setup_cmd, shell=True)
            print("Host environment setup has done!")

    @staticmethod
    def _ConfirmContinue():
        """Ask user if they want to continue.

        Returns:
            True if user answer yes.
        """
        answer_client = utils.InteractWithQuestion(
            "\nPress 'y' to continue or anything else to do it myself[y/N]: ",
            utils.TextColors.WARNING)
        return answer_client in constants.USER_ANSWER_YES
