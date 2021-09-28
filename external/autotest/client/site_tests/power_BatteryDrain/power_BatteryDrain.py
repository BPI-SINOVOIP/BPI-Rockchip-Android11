# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.common_lib import utils
from autotest_lib.client.cros.power import power_status
from autotest_lib.client.cros.power import power_utils

class power_BatteryDrain(test.test):
    """Not a test, but a utility for server tests to drain the battery below
    a certain threshold within a certain timeframe."""
    version = 1

    backlight = None
    keyboard_backlight = None

    url = 'https://crospower.page.link/power_BatteryDrain'

    def cleanup(self):
        '''Cleanup for a test run'''
        if self._force_discharge:
            if not power_utils.charge_control_by_ectool(True):
                logging.warn('Can not restore from force discharge.')
        if self.backlight:
            self.backlight.restore()
        if self.keyboard_backlight:
            default_level = self.keyboard_backlight.get_default_level()
            self.keyboard_backlight.set_level(default_level)

    def run_once(self, drain_to_percent, drain_timeout, force_discharge=False):
        '''
        Entry point of this test. The DUT must not be connected to AC.

        It turns the screen and keyboard backlight up as high as possible, and
        then opens Chrome to a WebGL heavy webpage. I also tried using a
        dedicated tool for stress-testing the CPU
        (https://github.com/intel/psst), but that only drew 27 watts on my DUT,
        compared with 35 watts using the WebGL website. If you find a better
        way to use a lot of power, please modify this test!

        @param drain_to_percent: Battery percentage to drain to.
        @param drain_timeout: In seconds.
        @param force_discharge: Force discharge even with AC plugged in.
        '''
        status = power_status.get_status()
        if not status.battery:
            raise error.TestNAError('DUT has no battery. Test Skipped')

        self._force_discharge = force_discharge
        if force_discharge:
            if not power_utils.charge_control_by_ectool(False):
                raise error.TestError('Could not run battery force discharge.')

        ac_error = error.TestFail('DUT is on AC power, but should not be')
        if not force_discharge and status.on_ac():
            raise ac_error

        self.backlight = power_utils.Backlight()
        self.backlight.set_percent(100)
        try:
            self.keyboard_backlight = power_utils.KbdBacklight()
            self.keyboard_backlight.set_percent(100)
        except power_utils.KbdBacklightException as e:
            logging.info("Assuming no keyboard backlight due to %s", str(e))
            self.keyboard_backlight = None

        with chrome.Chrome(init_network_controller=True) as cr:
            tab = cr.browser.tabs.New()
            tab.Navigate(self.url)

            logging.info(
                'Waiting {} seconds for battery to drain to {} percent'.format(
                    drain_timeout, drain_to_percent))

            def is_battery_low_enough():
                """Check if battery level reach target."""
                status.refresh()
                if not force_discharge and status.on_ac():
                    raise ac_error
                return status.percent_display_charge() <= drain_to_percent

            err = error.TestFail(
                "Battery did not drain to {} percent in {} seconds".format(
                    drain_to_percent, drain_timeout))
            utils.poll_for_condition(is_battery_low_enough,
                                            exception=err,
                                            timeout=drain_timeout,
                                            sleep_interval=1)
