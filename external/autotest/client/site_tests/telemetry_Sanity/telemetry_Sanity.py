# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.bin import test, telemetry_sanity


class telemetry_Sanity(test.test):
    """Run telemetry_sanity."""
    version = 1

    def run_once(self, count, run_cryptohome, run_incognito, run_screenlock):
        telemetry_sanity.TelemetrySanity(
            count=count,
            run_cryptohome=run_cryptohome,
            run_incognito=run_incognito,
            run_screenlock=run_screenlock).Run()
