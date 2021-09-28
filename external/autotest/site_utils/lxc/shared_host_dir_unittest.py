#!/usr/bin/python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import shutil
import subprocess
import tempfile
import unittest

import common
from autotest_lib.client.common_lib import error
from autotest_lib.site_utils import lxc
from autotest_lib.site_utils.lxc import utils as lxc_utils

# TODO (crbug/960959): Fix this flakey test.
@unittest.skip('Flakey (http://crbug/960959)')
class SharedHostDirTests(lxc_utils.LXCTests):
    """Unit tests for the ContainerBucket class."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.shared_host_path = os.path.join(self.tmpdir, 'host')


    def tearDown(self):
        shutil.rmtree(self.tmpdir)


    def testHostDirCreationAndCleanup(self):
        """Verifies that the host dir is properly created and cleaned up when
        the container bucket is set up and destroyed.
        """
        # Precondition: host path nonexistent
        self.assertFalse(os.path.isdir(self.shared_host_path))

        host_dir = lxc.SharedHostDir(self.shared_host_path)

        # Verify the host path in the host_dir.
        self.assertEqual(os.path.realpath(host_dir.path),
                         os.path.realpath(self.shared_host_path))
        self.assertTrue(os.path.isdir(self.shared_host_path))

        # Clean up, verify that the path is removed.
        host_dir.cleanup()
        self.assertFalse(os.path.isdir(self.shared_host_path))


    def testHostDirNotMounted(self):
        """Verifies that an unmounted host dir does not cause container bucket
        construction to crash.
        """
        # Create the shared host dir, but do not mount it.
        os.makedirs(self.shared_host_path)

        # Setup then destroy the HPM.  This should not emit any exceptions.
        try:
            host_dir = lxc.SharedHostDir(self.shared_host_path)
            host_dir.cleanup()
        except:
            self.fail('SharedHostDir crashed.\n%s' % error.format_error())


    def testHostDirAccess(self):
        """Verifies that sudo is not required to write to the shared host dir.
        """
        try:
            host_dir = lxc.SharedHostDir(self.shared_host_path)
            tempfile.NamedTemporaryFile(dir=host_dir.path)
        except OSError:
            self.fail('Unable to write to shared host dir.\n%s' %
                      error.format_error())
        finally:
            host_dir.cleanup()


@unittest.skip('Flakey (http://crbug/960959)')
class TimeoutTests(lxc_utils.LXCTests):
    """Test the timeouts on the shared host dir class."""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.shared_host_path = os.path.join(self.tmpdir, 'host')


    def tearDown(self):
        shutil.rmtree(self.tmpdir)


    def testTimeout(self):
        """Verifies that cleanup code correctly times out.

        Cleanup can fail because of I/O caches and other similar things keeping
        the mount active.  Test that the cleanup code properly times out in
        these scenarios.
        """
        host_dir = lxc.SharedHostDir(self.shared_host_path)

        # Create a process in the shared dir to force unmounting to fail.
        p = subprocess.Popen(['sleep', '2'], cwd=self.shared_host_path)

        # Cleanup should time out.
        with self.assertRaises(error.CmdError):
            logging.debug('attempting cleanup (should fail)')
            # Use a short timeout so the test doesn't take forever.
            host_dir.cleanup(timeout=1)
            logging.debug('cleanup did not fail')

        # Kill the process occupying the mount.
        p.terminate()

        # Cleanup should succeed.
        try:
            # Use the default timeout so this doesn't raise false errors.
            host_dir.cleanup()
        except error.CmdError as e:
            self.fail('Unexpected cleanup error: %r' % e)


if __name__ == '__main__':
    unittest.main()
