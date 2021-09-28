# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import time
from autotest_lib.client.bin import test, utils
from autotest_lib.client.cros import upstart

class platform_AnomalyDetector(test.test):
    "Tests the anomaly detector daemon"
    version = 1

    def run_once(self):
        "Runs the test once"

        # Restart the anomaly detector daemon (to clear its cache of
        # "already-seen warnings" and ensure this one is logged)
        upstart.restart_job("anomaly-detector")

        # Give enough time for the anomaly detector to open the journal and scan
        # to the end. (otherwise, it will miss the warning message).
        # TODO(https://crbug.com/983725): Check that it's opened the journal.
        time.sleep(0.5)

        # Delete old kernel warning files to distinguish this one.
        utils.system("rm -rf /var/spool/crash/kernel_warning*")

        lkdtm = "/sys/kernel/debug/provoke-crash/DIRECT"
        if os.path.exists(lkdtm):
            utils.system("echo WARNING > %s" % (lkdtm))
        else:
            utils.system("echo warning > /proc/breakme")

        cmd = "test -f /var/spool/crash/kernel_warning*.kcrash"
        utils.poll_for_condition_ex(lambda: utils.system(cmd) == 0)
