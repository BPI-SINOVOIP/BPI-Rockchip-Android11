# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import time
from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros import rtc
from autotest_lib.client.cros.power import sys_power

def read_rtc_wakeup(rtc_device):
    """
    Read the wakeup setting for for the RTC device.
    """
    sysfs_path = '/sys/class/rtc/%s/device/power/wakeup' % rtc_device
    if os.path.isfile(sysfs_path):
        return file(sysfs_path).read().strip()


def read_rtc_wakeup_active_count(rtc_device):
    """
    Read the current wakeup active count for the RTC device.
    """
    path = '/sys/class/rtc/%s/device/power/wakeup_active_count' % rtc_device
    return int(file(path).read())


def fire_wakealarm(rtc_device):
    """
    Schedule a wakealarm and wait for it to fire.
    """
    rtc.set_wake_alarm('+1', rtc_device)
    time.sleep(2)


class power_WakeupRTC(test.test):
    """Test RTC wake events."""

    version = 1

    def run_once(self):
        """
        Tests that RTC devices generate wakeup events.
        We require /dev/rtc0 to work since there are many things which rely
        on the rtc0 wakeup alarm. For all the other RTCs, only test those
        that have wakeup alarm capabilities.
        """
        default_rtc = "/dev/rtc0"
        if not os.path.exists(default_rtc):
            raise error.TestFail('RTC device %s does not exist' % default_rtc)
        default_rtc_device = os.path.basename(default_rtc)
        if read_rtc_wakeup(default_rtc_device) != 'enabled':
            raise error.TestFail('RTC wakeup is not enabled: %s' % default_rtc_device)
        for rtc_device in rtc.get_rtc_devices():
            if read_rtc_wakeup(rtc_device) != 'enabled':
                logging.info('RTC wakeup is not enabled: %s' % rtc_device)
            else:
                logging.info('RTC wakeup is enabled for: %s' % rtc_device)
                self.run_once_rtc(rtc_device)

    def run_once_rtc(self, rtc_device):
        """Tests that a RTC device generate wakeup events.

        @param rtc_device: RTC device to be tested.
        """
        logging.info('testing rtc device %s', rtc_device)

        # Test that RTC can generate wake events
        old_sys_wakeup_count = sys_power.read_wakeup_count()
        old_rtc_wakeup_active_count = read_rtc_wakeup_active_count(rtc_device)
        fire_wakealarm(rtc_device)
        new_sys_wakeup_count = sys_power.read_wakeup_count()
        new_rtc_wakeup_active_count = read_rtc_wakeup_active_count(rtc_device)
        if new_rtc_wakeup_active_count == old_rtc_wakeup_active_count:
            raise error.TestFail(
                    'RTC alarm should increase RTC wakeup_active_count: %s'
                    % rtc_device)
        if new_sys_wakeup_count == old_sys_wakeup_count:
            raise error.TestFail(
                    'RTC alarm should increase system wakeup_count: %s'
                    % rtc_device)
