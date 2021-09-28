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
"""Common code used by acloud setup tools."""

from __future__ import print_function
import logging
import re
import subprocess

from acloud import errors


logger = logging.getLogger(__name__)

PKG_INSTALL_CMD = "sudo apt-get --assume-yes install %s"
APT_CHECK_CMD = "LANG=en_US.UTF-8 apt-cache policy %s"
_INSTALLED_RE = re.compile(r"(.*\s*Installed:)(?P<installed_ver>.*\s?)")
_CANDIDATE_RE = re.compile(r"(.*\s*Candidate:)(?P<candidate_ver>.*\s?)")


def CheckCmdOutput(cmd, print_cmd=True, **kwargs):
    """Helper function to run subprocess.check_output.

    This function will return the command output for parsing the result and will
    raise Error if command return code was non-zero.

    Args:
        cmd: String, the cmd string.
        print_cmd: True to print cmd to stdout.
        kwargs: Other option args to subprocess.

    Returns:
        Return cmd output as a byte string.
        If the return code was non-zero it raises a CalledProcessError.
    """
    if print_cmd:
        print("Run command: %s" % cmd)

    logger.debug("Run command: %s", cmd)
    return subprocess.check_output(cmd, **kwargs)


def InstallPackage(pkg):
    """Install package.

    Args:
        pkg: String, the name of package.

    Raises:
        PackageInstallError: package is not installed.
    """
    try:
        print(CheckCmdOutput(PKG_INSTALL_CMD % pkg,
                             shell=True,
                             stderr=subprocess.STDOUT))
    except subprocess.CalledProcessError as cpe:
        logger.error("Package install for %s failed: %s", pkg, cpe.output)
        raise errors.PackageInstallError(
            "Could not install package [" + pkg + "], :" + str(cpe.output))

    if not PackageInstalled(pkg, compare_version=False):
        raise errors.PackageInstallError(
            "Package was not detected as installed after installation [" +
            pkg + "]")


def PackageInstalled(pkg_name, compare_version=True):
    """Check if the package is installed or not.

    This method will validate that the specified package is installed
    (via apt cache policy) and check if the installed version is up-to-date.

    Args:
        pkg_name: String, the package name.
        compare_version: Boolean, True to compare version.

    Returns:
        True if package is installed.and False if not installed or
        the pre-installed package is not the same version as the repo candidate
        version.

    Raises:
        UnableToLocatePkgOnRepositoryError: Unable to locate package on repository.
    """
    try:
        pkg_info = CheckCmdOutput(
            APT_CHECK_CMD % pkg_name,
            print_cmd=False,
            shell=True,
            stderr=subprocess.STDOUT)

        logger.debug("Check package install status")
        logger.debug(pkg_info)
    except subprocess.CalledProcessError as error:
        # Unable locate package name on repository.
        raise errors.UnableToLocatePkgOnRepositoryError(
            "Could not find package [" + pkg_name + "] on repository, :" +
            str(error.output) + ", have you forgotten to run 'apt update'?")

    installed_ver = None
    candidate_ver = None
    for line in pkg_info.splitlines():
        match = _INSTALLED_RE.match(line)
        if match:
            installed_ver = match.group("installed_ver").strip()
            continue
        match = _CANDIDATE_RE.match(line)
        if match:
            candidate_ver = match.group("candidate_ver").strip()
            continue

    # package isn't installed
    if installed_ver == "(none)":
        logger.debug("Package is not installed, status is (none)")
        return False
    # couldn't find the package
    if not (installed_ver and candidate_ver):
        logger.debug("Version info not found [installed: %s ,candidate: %s]",
                     installed_ver,
                     candidate_ver)
        return False
    # TODO(148116924):Setup process should ask user to update package if the
    # minimax required version is specified.
    if compare_version and installed_ver != candidate_ver:
        logger.warning("Package %s version at %s, expected %s",
                       pkg_name, installed_ver, candidate_ver)
    return True
