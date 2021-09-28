#!/usr/bin/python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mox
import unittest

import common
from autotest_lib.client.bin import utils
from autotest_lib.site_utils.lxc import container_bucket


class ContainerBucketTests(mox.MoxTestBase):
    """Unit tests for the ContainerBucket class."""

    def testForceDestruction(self):
        """Verifies that the force destruction logic produces the right cmd.
        """
        self.mox.StubOutWithMock(utils, 'run')
        utils.run('sudo lxc-destroy -P '
                  '/usr/local/autotest/containers -n nonexistent-name -f -s',
                  ignore_status=mox.IgnoreArg(),
                  timeout=mox.IgnoreArg()
        ).AndReturn(mox.MockAnything())
        self.mox.ReplayAll()
        bucket = container_bucket.ContainerBucket(
            container_factory=mox.MockAnything())
        bucket.scrub_container_location("nonexistent-name")
        self.mox.VerifyAll()


if __name__ == '__main__':
    unittest.main()
