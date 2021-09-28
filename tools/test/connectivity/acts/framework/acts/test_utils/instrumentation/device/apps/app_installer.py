#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the 'License');
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an 'AS IS' BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import os
import re

from acts.libs.proc import job

PKG_NAME_PATTERN = r"^package:\s+name='(?P<pkg_name>.*?)'"
PM_PATH_PATTERN = r"^package:(?P<apk_path>.*)"


class AppInstaller(object):
    """Class that represents an app on an Android device. Includes methods
    for install, uninstall, and getting info.
    """
    def __init__(self, ad, apk_path):
        """Initializes an AppInstaller.

        Args:
            ad: device to install the apk
            apk_path: path to the apk
        """
        self._ad = ad
        self._apk_path = apk_path
        self._pkg_name = None

    @staticmethod
    def pull_from_device(ad, pkg_name, dest):
        """Initializes an AppInstaller by pulling the apk file from the device,
        given the package name

        Args:
            ad: device on which the apk is installed
            pkg_name: package name
            dest: destination directory
                (Note: If path represents a directory, it must already exist as
                 a directory)

        Returns: AppInstaller object representing the pulled apk, or None if
            package not installed
        """
        if not ad.is_apk_installed(pkg_name):
            ad.log.warning('Unable to find package %s on device. Pull aborted.'
                           % pkg_name)
            return None
        path_on_device = re.compile(PM_PATH_PATTERN).search(
            ad.adb.shell('pm path %s' % pkg_name)).group('apk_path')
        ad.pull_files(path_on_device, dest)
        if os.path.isdir(dest):
            dest = os.path.join(dest, os.path.basename(path_on_device))
        return AppInstaller(ad, dest)

    @property
    def apk_path(self):
        return self._apk_path

    @property
    def pkg_name(self):
        """Get the package name corresponding to the apk from aapt

        Returns: The package name, or empty string if not found.
        """
        if self._pkg_name is None:
            dump = job.run(
                'aapt dump badging %s' % self.apk_path,
                ignore_status=True).stdout
            match = re.compile(PKG_NAME_PATTERN).search(dump)
            self._pkg_name = match.group('pkg_name') if match else ''
        return self._pkg_name

    def install(self, *extra_args):
        """Installs the apk on the device.

        Args:
            extra_args: Additional flags to the ADB install command.
                Note that '-r' is included by default.
        """
        self._ad.log.info('Installing app %s from %s' %
                          (self.pkg_name, self.apk_path))
        args = '-r %s' % ' '.join(extra_args)
        self._ad.adb.install('%s %s' % (args, self.apk_path))

    def uninstall(self, *extra_args):
        """Uninstalls the apk from the device.

        Args:
            extra_args: Additional flags to the uninstall command.
        """
        self._ad.log.info('Uninstalling app %s' % self.pkg_name)
        if not self.is_installed():
            self._ad.log.warning('Unable to uninstall app %s. App is not '
                                 'installed.' % self.pkg_name)
            return
        self._ad.adb.shell(
            'pm uninstall %s %s' % (' '.join(extra_args), self.pkg_name))

    def is_installed(self):
        """Verifies that the apk is installed on the device.

        Returns: True if the apk is installed on the device.
        """
        if not self.pkg_name:
            self._ad.log.warning('No package name found for %s' % self.apk_path)
            return False
        return self._ad.is_apk_installed(self.pkg_name)
