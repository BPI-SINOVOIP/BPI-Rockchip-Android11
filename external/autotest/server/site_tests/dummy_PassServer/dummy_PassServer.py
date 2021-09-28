# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.server import test

class dummy_PassServer(test.test):
    """Tests that server tests can pass."""
    version = 1

    def run_once(self, expect_ssp=None):
        """There is no body for this test.

        @param expect_ssp: If True, ensure test is running inside a container.
                If False, ensure test is not running inside a container.
                If None (default), do nothing.
        """
        if expect_ssp is not None:
            if expect_ssp and not utils.is_in_container():
                raise error.TestFail('The test is not running inside container')
            if not expect_ssp and utils.is_in_container():
                raise error.TestFail('Test test is running inside container')
