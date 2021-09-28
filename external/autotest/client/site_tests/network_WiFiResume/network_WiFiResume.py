# Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import logging
import random

from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros.network import interface
from autotest_lib.client.cros.power import sys_power

class network_WiFiResume(test.test):
    """
    Ensure wireless interface comes up after suspend-resume.
    """
    version = 1

    def _get_wifi_dev(self):
        wlan_ifs = [nic for nic in interface.get_interfaces()
                    if nic.is_wifi_device()]
        if wlan_ifs:
            return wlan_ifs[0].name
        else:
            return None

    # Default suspend values are for smoke test that expects to run in <10s
    def _suspend_to_ram(self, min_secs_suspend=2, max_secs_suspend=7):
        secs_to_suspend = (random.randint(min_secs_suspend, max_secs_suspend))
        logging.info('Scheduling wakeup in %d seconds.\n', secs_to_suspend)
        sys_power.do_suspend(secs_to_suspend)

    def run_once(self, wifi_timeout=2, dev=None):
        '''Check that WiFi interface is available after a resume

        @param dev: device (eg 'wlan0') to use for wifi tests.
                    autodetected if unset.
        @param wifi_timeout: number of seconds within which the interface must
                             come up after a suspend/resume cycle.
        '''

        if dev is None:
            dev = self._get_wifi_dev()
        if dev is None:
            raise error.TestError('No wifi device supplied to check for'
                                  'or found on system')

        random.seed()
        self._suspend_to_ram()
        start_time = datetime.datetime.now()
        deadline = start_time + datetime.timedelta(seconds=wifi_timeout)
        found_dev = None

        while (not found_dev) and (deadline > datetime.datetime.now()):
            found_dev = self._get_wifi_dev()
            if found_dev == dev:
                delay = datetime.datetime.now() - start_time
                logging.info('Found %s about %d ms after resume'%
                             (dev, int(delay.total_seconds()*1000)))
                return
            elif found_dev is not None:
                logging.error('Found %s on resume, was %s before suspend' %
                              (found_dev, dev))

        if found_dev != dev:
            delay = datetime.datetime.now() - start_time
            raise error.TestFail('Did not find %s after %d ms' %
                                 (dev, int(delay.total_seconds()*1000)))
