# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import subprocess

from autotest_lib.client.bin import test, utils
from autotest_lib.client.common_lib import error


class security_ProcessManagementPolicy(test.test):
    """
    Forks processes as non-root users and ensures the processes can change UID
    to a user that is explicitly allowed in the system-wide whitelist, but no
    other user.
    """
    version = 1

    _WHITELIST_DICT = {
        "cros-disks": set(("avfs", "chronos", "fuse-exfat",
                           "fuse-sshfs", "nobody", "ntfs-3g")),
        "shill": set(("dhcp", "ipsec", "openvpn", "syslog", "nobody")),
    }

    def __init__(self, *args, **kwargs):
        version = utils.get_kernel_version()
        if version == "3.8.11":
            raise error.TestNAError('Test is n/a for kernels older than 3.10')
        super(security_ProcessManagementPolicy,
            self).__init__(*args, **kwargs)
        self._failure = False

    def cleanup(self):
        """
        Clean up the test environment.
        """
        super(security_ProcessManagementPolicy, self).cleanup()

    def _fail(self, msg):
        """
        Log failure message and record failure.

        @param msg: String to log.

        """
        logging.error(msg)
        self._failure = True

    def _test_setuid(self, parent, child, give_cap_setuid, expect_success):
        if give_cap_setuid:
            caps = "0xc0"
        else:
            caps = "0x0"
        try:
            subprocess.check_output(["/sbin/minijail0",
                                            "-u",
                                            parent,
                                            "-g",
                                            parent,
                                            "-c",
                                            caps,
                                            "--",
                                            "/sbin/capsh",
                                            "--user=" + child,
                                            "--",
                                            "-c",
                                            "/usr/bin/whoami"])

        except subprocess.CalledProcessError, e:
            if expect_success:
                logging.error(" " + parent + " not able to setuid to " + child)
                self._failure = True
            return
        if not expect_success:
            logging.error(" " + parent + " able to setuid to " + child)
            self._failure = True

    def run_once(self):
        """
        Runs the test, spawning processes as users and checking setuid()
        behavior.
        """
        for parent in self._WHITELIST_DICT:
            for child in self._WHITELIST_DICT[parent]:
                # Expect the setuid() call to be permitted
                self._test_setuid(parent, child, True, True)
                # Expect the setuid() call to be denied
                self._test_setuid(parent, child, False, False)


        # Make sure 'cros-disks' can't setuid() to 'root'
        self._test_setuid("cros-disks", "root", True, False)
        # Make sure 'shill' can't setuid() to 'chronos'
        self._test_setuid("shill", "chronos", True, False)
        # Make sure 'openvpn' can't setuid() to 'root'
        self._test_setuid("openvpn", "root", True, False)
        # Make sure 'ipsec' can't setuid() to 'root'
        self._test_setuid("ipsec", "root", True, False)

        # Make the test fail if any unexpected behaviour got detected. Note
        # that the error log output that will be included in the failure
        # message mentions the failed location to aid debugging.
        if self._failure:
            raise error.TestFail('Unexpected setuid() behavior')
