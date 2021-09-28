# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import shutil
import tempfile

from autotest_lib.client.bin import test, utils
from autotest_lib.client.common_lib import error

MOUNT_PATH=tempfile.mkdtemp()

class security_NosymfollowMountOption(test.test):
    """
    Mount filesystems with the "nosymfollow" option and ensure symlink
    traversal is blocked.
    """
    version = 1

    def __init__(self, *args, **kwargs):
        # TODO(mortonm): add a function to utils to do this kernel version
        # check and raise NAError.
        version = utils.get_kernel_version()
        if version == "3.8.11":
            raise error.TestNAError('Test is n/a for kernels older than 3.10')
        super(security_NosymfollowMountOption,
            self).__init__(*args, **kwargs)
        self._failure = False

    def cleanup(self):
        """
        Clean up test environment.
        """
        super(security_NosymfollowMountOption, self).cleanup()
        shutil.rmtree(MOUNT_PATH)

    def _fail(self, msg):
        """
        Log failure message and record failure.

        @param msg: String to log.

        """
        logging.error(msg)
        self._failure = True

    def umount(self):
        """
        Unmount file system at MOUNT_PATH location.
        """
        try:
            subprocess.check_output(["/bin/umount", MOUNT_PATH])
        except subprocess.CalledProcessError, e:
            self._fail("umount call failed")

    def mount_and_test_with_string(self, mount_options, restrict_symlinks):
        """
        Mount file system with given options, check it was mounted with
        correct options, and make sure symlink traversal restriction works as
        expected.

        @param mount_options: Mount options string.

        @param restrict_symlinks: True if mount options should cause symlinks
        to be restricted, False otherwise.

        """
        try:
            subprocess.check_output(["/bin/mount",
                                            "-n",
                                            "-t",
                                            "tmpfs",
                                            "-o",
                                            mount_options,
                                            "tmpfs",
                                            MOUNT_PATH])
        except subprocess.CalledProcessError:
            self._fail("mount call failed")
            return

        try:
            ps = subprocess.Popen(('mount'), stdout=subprocess.PIPE)
            output = subprocess.check_output(('grep',MOUNT_PATH),
                                                        stdin=ps.stdout)
            ps.wait()

            for arg in mount_options.split(','):
                if arg == "nosymfollow":
                    continue
                else:
                    if output.find(arg) == -1:
                        self._fail("filesystem missing '%s' arg" % arg)
                        return

            try:
                open(MOUNT_PATH + "/file", "w+")
                os.symlink(MOUNT_PATH + "/file", MOUNT_PATH + "/link")
            except IOError:
                self._fail("creating/linking files failed")
                return

            traversal_restricted = False
            try:
                open(MOUNT_PATH + "/link", "r")
            except IOError:
                traversal_restricted = True

            if restrict_symlinks:
                if not traversal_restricted:
                    self._fail("symlink traversal was not restricted")
                    return
            else:
                if traversal_restricted:
                    self._fail("symlink traversal was restricted")
        finally:
            self.umount()

    def run_once(self, test_selinux_interaction):
        """
        Runs the test, mounting filesystems and checking symlink traversal
        behavior.
        """
        self.mount_and_test_with_string("nosymfollow", True)
        self.mount_and_test_with_string("nodev,noexec,nosuid,nosymfollow", True)
        self.mount_and_test_with_string("nodev,noexec,nosuid", False)

        if test_selinux_interaction:
            if not os.path.exists('/etc/selinux'):
                raise error.TestNAError('Test is n/a if selinux is not enabled')
            self.mount_and_test_with_string("nosymfollow,"
                                            "context=u:object_r:tmpfs:s0,"
                                            "fscontext=u:object_r:tmpfs:s0",
                                            True)
            self.mount_and_test_with_string("context=u:object_r:tmpfs:s0,"
                                            "nosymfollow,"
                                            "fscontext=u:object_r:tmpfs:s0",
                                            True)
            self.mount_and_test_with_string("context=u:object_r:tmpfs:s0,"
                                            "fscontext=u:object_r:tmpfs:s0,"
                                            "nosymfollow",
                                            True)

        # Make the test fail if any unexpected behaviour got detected. Note
        # that the error log output that will be included in the failure
        # message mentions the failed location to aid debugging.
        if self._failure:
            raise error.TestFail('Unexpected mount behavior')
