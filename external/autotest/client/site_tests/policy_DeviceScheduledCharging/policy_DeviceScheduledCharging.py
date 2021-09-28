# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.bin import utils
from autotest_lib.client.cros import upstart
from autotest_lib.client.cros.enterprise import charging_policy_tests


class policy_DeviceScheduledCharging(charging_policy_tests.ChargingPolicyTest):
    """
    Variant of ChargingPolicyTest for schedule-based charging policies. As of
    this writing, these features are only present on the Wilco platform.

    This variation of ChargingPolicyTest only has to do a bit of warmup and
    cleanup before and after each call to run_once(). Users should assume that
    the EC thinks the time is |MOCK_TIME|.
    """
    version = 1

    SYNC_EC_RTC_UPSTART_JOB = 'wilco_sync_ec_rtc'
    # Noon on a Monday.
    MOCK_TIME = '1/1/01 12:00:00'

    def warmup(self):
        """
        For the first step in the test we set the EC's RTC to a consistent,
        mock time. The EC, or Embedded Controller, is a microcontroller
        separate from the main system-on-a-chip. The EC controls charge
        scheduling, among other things. Setting the EC's RTC allows us to
        use a hardcoded list of schedules as our test cases. We also need to
        disable the upstart job that keeps the EC's RTC in sync with the
        local time of the DUT.
        """
        super(policy_DeviceScheduledCharging, self).warmup()
        upstart.stop_job(self.SYNC_EC_RTC_UPSTART_JOB)
        utils.set_hwclock(
                time=self.MOCK_TIME,
                utc=False,
                rtc='/dev/rtc1',
                noadjfile=True)

    def cleanup(self):
        """
        Get the DUT back to a clean state after messing with it in warmup().
        """
        utils.set_hwclock(
                time='system', utc=False, rtc='/dev/rtc1', noadjfile=True)
        upstart.restart_job(self.SYNC_EC_RTC_UPSTART_JOB)
        super(policy_DeviceScheduledCharging, self).cleanup()
