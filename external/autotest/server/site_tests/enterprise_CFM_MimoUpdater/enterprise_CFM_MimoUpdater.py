# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Autotest for mimo-updater."""

from autotest_lib.server import test
from autotest_lib.client.common_lib import error

class enterprise_CFM_MimoUpdater(test.test):
    """
    Tests that mimo-updater runs with a firmware version check
    mimo-updater is also responsible for determining if a fw update is needed.
    However, those parts are currently untestable.
    """
    version = 1

    def run_once(self, host):
        """Top level function that is called by autoserv."""
        host.run("rm --force /var/log/messages")
        host.reboot()
        host.wait_up()
        # grep's exit status is 0 if pattern matched, 1 ow.
        # utils.grep() doesn't use extended regexp
        cmd = 'grep -E "Firmware Version: 0x[0-9]+" /var/log/messages'
        output = host.run(cmd, ignore_status=True)
        if output.stderr:
            raise error.TestFail(output.stderr)


