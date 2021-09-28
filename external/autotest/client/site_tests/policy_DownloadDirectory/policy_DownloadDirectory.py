# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base

from telemetry.core import exceptions


class policy_DownloadDirectory(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of DownloadDirectory policy on Chrome OS.

    This policy can only be set to default or Google Drive. Test each case,
    and verify the test download file goes to the proper location.

    """
    version = 1

    POLICY_NAME = 'DownloadDirectory'
    _DOWNLOAD_BASE = ('http://commondatastorage.googleapis.com/'
                      'chromiumos-test-assets-public/audio_power/')
    DOWNLOAD_DIR = '/home/chronos/user/Downloads/download'
    POLICY_SETTING = {'PromptForDownloadLocation': False}

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        if case:
            self.POLICY_SETTING[self.POLICY_NAME] = '${google_drive}'

        self.setup_case(user_policies=self.POLICY_SETTING,
                        real_gaia=True)

        self.GDRIVE_DIR = self._get_Gdrive_path()
        self._wait_for_mount_ready()
        self._clear_test_locations()
        self._the_test(case)
        self._clear_test_locations()

    def _get_Gdrive_path(self):
        """Returns the path for the Google Drive Mountpoint."""
        self.GDRIVE_BASE = self._get_mount(False)
        return '{}/root/download'.format(self.GDRIVE_BASE)

    def _download_test_file(self):
        """Loads to the test URL which automatically downloads the test file."""
        try:
            self.navigate_to_url(self._DOWNLOAD_BASE)
        # This page empty with just a test download, so it will timeout.
        except exceptions.TimeoutException:
            pass


    def _the_test(self, case):
        """
        Download the test file, and verify it is in the proper directory,
        and not in the incorrect directory

        @param case: If the download location was set to Gdrive or not.

        """
        self._download_test_file()

        if case:
            if (not os.path.isfile(self.GDRIVE_DIR) or
                    os.path.isfile(self.DOWNLOAD_DIR)):
                raise error.TestError(
                    'Download file not in Gdrive Dir, OR found in downloads.')
        else:
            if (not os.path.isfile(self.DOWNLOAD_DIR) or
                    os.path.isfile(self.GDRIVE_DIR)):
                raise error.TestError(
                    'Download file in Gdrive Dir, OR not found in downloads.')

    def _clear_test_locations(self):
        """Deletes the 'download' file in both test locations."""
        for test_dir in [self.DOWNLOAD_DIR, self.GDRIVE_DIR]:
            try:
                # Adding the * incase the file was downloaded multiple times.
                utils.system_output('rm {}*'.format(test_dir))
            # If there is no file present this will return -1. Thats OK!
            except error.CmdError:
                pass

    def _wait_for_mount_ready(self):
        """Wait for the mount to be ready."""
        def _mount_ready():
            try:
                utils.system_output('ls {}/root/'.format(self.GDRIVE_BASE))
                return True
            except error.CmdError:
                return False

        utils.poll_for_condition(
            lambda: (_mount_ready()),
            exception=error.TestFail('derek mounts not ready ever'),
            timeout=15,
            sleep_interval=1,
            desc='Polling for mounts to be ready.')

    def _get_mount(self, case):
        """Get the Google Drive mount path."""
        e_msg = 'Should have found mountpoint but did not!'
        # It may take some time until drivefs is started, so poll for the
        # mountpoint until timeout.
        utils.poll_for_condition(
            lambda: (self._find_drivefs_mount() is not None),
            exception=error.TestFail(e_msg),
            timeout=10,
            sleep_interval=1,
            desc='Polling for Google Drive to load.')

        mountpoint = self._find_drivefs_mount()
        return mountpoint

    def _find_drivefs_mount(self):
        """Return the mount point of the drive if found, else return None."""
        for mount in utils.mounts():
            if mount['type'] == 'fuse.drivefs':
                return mount['dest']
        return None
